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
#include <boost/system/detail/error_code.hpp>

#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

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
      _fd{inotify_init()},
      _sd(*_io_context, _fd),
      _logger{log_v2::instance().get(log_v2::CORE)} {
  if (_fd < 0) {
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

  _wd = inotify_add_watch(_fd, to_watch_dir.c_str(), mask);
  if (_wd < 0)
    throw msg_fmt(
        "directory_watcher: failed to add inotify watch on directory {}",
        to_watch_dir);
}

/**
 *  Destructor.
 */
directory_watcher::~directory_watcher() {
  std::cout << "directory_watcher destructor" << std::endl;
  boost::system::error_code ec;
  auto ec1 = _sd.close(ec);
  if (ec1) {
    _logger->error("Error while closing the directory watcher: {}",
                   ec1.message());
  }

  inotify_rm_watch(_fd, _wd);
  close(_fd);
}

/**
 * @brief Watch for events on the watched directory and return an iterator over
 * the events.
 *
 * @return directory_watcher::iterator an iterator over the events
 */
directory_watcher::iterator directory_watcher::watch() {
  boost::system::error_code ec;
  auto buffer = boost::asio::buffer(_buffer);
  _bytes_read = _sd.read_some(buffer, ec);
  iterator retval(this);
  if (ec)
    _logger->error("Error while reading from inotify: {}", ec.message());
  return retval;
}
