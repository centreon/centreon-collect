/**
 * Copyright 2025 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/file/directory_watcher.hh"

#include <errno.h>
#include <boost/asio/error.hpp>
#include <boost/system/detail/error_code.hpp>

#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

namespace asio = boost::asio;

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::file;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief Construct a new directory watcher object
 *
 * @param to_watch_dir the directory to watch
 * @param mask the mask of events to watch for
 * @param non_blocking if true, the inotify instance will be non-blocking
 */
directory_watcher::directory_watcher(const std::string& to_watch_dir,
                                     uint32_t mask,
                                     bool non_blocking)
    : _io_context(com::centreon::common::pool::io_context_ptr()),
      _to_watch_dir{to_watch_dir},
      _mask{mask},
      _sd{asio::posix::stream_descriptor(*_io_context, inotify_init())},
      _logger{log_v2::instance().get(log_v2::CORE)} {
  int fd = _sd.native_handle();
  _logger->info(
      "directory_watcher: watching directory '{}' with mask {:#x} and "
      "non_blocking={}",
      to_watch_dir, mask, non_blocking);
  if (fd < 0) {
    throw msg_fmt("directory_watcher: couldn't create inotify instance: '{}'",
                  ::strerror(errno));
  }

  if (non_blocking) {
    boost::system::error_code ec;
    _sd.non_blocking(true, ec);
    if (ec) {
      _logger->error(
          "directory_watcher: couldn't set inotify instance to non-blocking: "
          "{}",
          ec.message());
    }
  }

  _wd = inotify_add_watch(fd, to_watch_dir.c_str(), mask);
  if (_wd < 0)
    throw msg_fmt(
        "directory_watcher: failed to add inotify watch on directory {}",
        to_watch_dir);
}

/**
 *  Destructor.
 */
directory_watcher::~directory_watcher() {
  /* Closing the asio descriptor closes the underlying inotify fd, which in turn
   * automatically removes every watch associated with it. Doing
   * inotify_rm_watch()/close() afterwards would operate on the already-closed
   * handle (now -1), so closing the descriptor is the only thing to do. */
  boost::system::error_code ec;
  auto ec1 = _sd.close(ec);
  if (ec1)
    _logger->error("Error while closing the directory watcher: {}",
                   ec1.message());
}

/**
 * @brief Watch for events on the watched directory and return an iterator over
 * the events.
 *
 * @return directory_watcher::iterator an iterator over the events
 */
directory_watcher::iterator directory_watcher::watch() {
  /* A watch lost on an earlier cycle is retried here: without a watch, nothing
   * would ever be reported again, and no amount of reading would tell. */
  if (_wd < 0)
    _rearm();

  boost::system::error_code ec;
  _bytes_read = _sd.read_some(boost::asio::buffer(_buffer), ec);
  if (ec == asio::error::would_block || ec == asio::error::try_again) {
    /* The descriptor is non-blocking, so an empty inotify queue is the
     * ordinary case -- most of the time nothing happened since the previous
     * call. Reporting it as an error would fill the logs with one error line
     * per watcher cycle. */
    _bytes_read = 0;
  } else if (ec) {
    _logger->error("Unable to read from inotify: {}", ec.message());
    _bytes_read = 0;
  }

  /* Among the events, the kernel also reports on the state of the watch
   * itself. Those carry no file name, so the caller's loop steps over them,
   * but they are what says whether the events can be taken as complete --
   * and they are the only warning that a watch has gone silent for good. */
  for (auto it = begin(), last = end(); it != last; ++it) {
    uint32_t mask = (*it).first;
    if (mask & IN_Q_OVERFLOW) {
      _logger->warn(
          "directory_watcher: the kernel event queue overflowed while watching "
          "'{}': some changes were dropped and can only be recovered by "
          "scanning the directory",
          _to_watch_dir);
      _rescan_needed = true;
    }
    if (mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF)) {
      _logger->warn(
          "directory_watcher: the watch on '{}' was lost (mask {:#x}), "
          "re-establishing it",
          _to_watch_dir, mask);
      _rearm();
      /* Whatever happened to the directory while it was unwatched is invisible
       * to us, hence the scan. */
      _rescan_needed = true;
    }
  }
  return iterator(this);
}

/**
 * @brief Cancel a pending asynchronous wait.
 */
void directory_watcher::cancel() noexcept {
  boost::system::error_code ec;
  _sd.cancel(ec);
  if (ec)
    _logger->error("directory_watcher: cannot cancel the wait on '{}': {}",
                   _to_watch_dir, ec.message());
}

/**
 * @brief Establish the watch again on the directory this watcher was built
 * for, after it was lost.
 *
 * @return True when the watch is in place, false when the directory could not
 * be watched -- typically because it does not exist any more, in which case the
 * next cycle tries again.
 */
bool directory_watcher::_rearm() {
  /* Which of the two cases we are in decides whether there is anything to
   * remove. A watch the kernel dropped itself (IN_IGNORED, after the directory
   * was deleted) is already gone and this call fails with EINVAL, harmlessly.
   * But a directory that was merely renamed keeps its watch alive on the moved
   * inode: not dropping it would leak one watch per rename, and leave us
   * listening to a directory nobody writes to any more. */
  if (_wd >= 0) {
    inotify_rm_watch(_sd.native_handle(), _wd);
    _wd = -1;
  }
  int wd = inotify_add_watch(_sd.native_handle(), _to_watch_dir.c_str(), _mask);
  if (wd < 0) {
    /* Every cycle retries, so this would otherwise be logged forever. */
    if (!_rearm_failure_logged) {
      _logger->error(
          "directory_watcher: cannot watch '{}' any more: {}. No configuration "
          "change will be detected there until it can be watched again",
          _to_watch_dir, ::strerror(errno));
      _rearm_failure_logged = true;
    }
    return false;
  }
  _wd = wd;
  _rearm_failure_logged = false;
  _logger->info("directory_watcher: watch on '{}' established again",
                _to_watch_dir);
  return true;
}
