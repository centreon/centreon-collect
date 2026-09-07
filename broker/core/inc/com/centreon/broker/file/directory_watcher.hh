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

#ifndef CCB_FILE_DIRECTORY_WATCHER_HH
#define CCB_FILE_DIRECTORY_WATCHER_HH

#include <sys/inotify.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <cstdio>
#include <string>

namespace com::centreon::broker::file {

constexpr size_t BUF_LEN = 4096;

/**
 *  @class directory_watcher directory_watcher.hh
 * "com/centreon/broker/file/directory_watcher.hh"
 *  @brief Directory watcher.
 *
 *  Watch over directories for files modifications.
 *  This watcher can't be used by multiple threads.
 */
class directory_watcher {
  std::shared_ptr<boost::asio::io_context> _io_context;
  /* The watched directory and the events asked for. Kept because a watch can
   * be lost -- the directory removed, renamed or replaced -- and has then to be
   * established again from scratch. */
  std::string _to_watch_dir;
  uint32_t _mask;
  /* Watch descriptor, -1 when the watch is lost. */
  int _wd;

  boost::asio::posix::stream_descriptor _sd;
  char _buffer[BUF_LEN];
  size_t _bytes_read = 0;
  /* Set when the events read cannot be taken as the whole story: the kernel
   * dropped some, or the watch was lost. Consumed by take_rescan_request(). */
  bool _rescan_needed = false;
  /* A watch that cannot be re-established fails again on every cycle. Log the
   * reason once instead of once per cycle. */
  bool _rearm_failure_logged = false;

  std::shared_ptr<spdlog::logger> _logger;

  bool _rearm();

 public:
  class iterator {
    directory_watcher* _watcher;
    size_t _offset = 0;
    inotify_event* _event = nullptr;
    std::pair<uint32_t, std::string_view> _current;

    inline void _update_current() {
      if (_offset < _watcher->_bytes_read) {
        _current.first = _event->mask;
        /* Only a non-zero len means a name was written after the structure.
         * The kernel's own events (queue overflow, watch removal) carry none,
         * so reading `name` there would walk over whatever the buffer holds. */
        _current.second =
            _event->len ? std::string_view(_event->name) : std::string_view();
      } else {
        _current = {0, ""};
      }
    }

   public:
    iterator(directory_watcher* watcher, size_t offset = 0)
        : _watcher(watcher),
          _offset(offset),
          _event{
              reinterpret_cast<inotify_event*>(_watcher->_buffer + _offset)} {
      _update_current();
    }

    std::pair<uint32_t, std::string_view>& operator*() {
      _watcher->_logger->trace(
          "directory_watcher: current: {}, offset: {}, bytes_read: {}, end "
          "reached {}",
          _current.second, _offset, _watcher->_bytes_read,
          *this == _watcher->end());
      return _current;
    }

    iterator& operator++() {
      _offset += sizeof(inotify_event) + _event->len;
      _event = reinterpret_cast<inotify_event*>(_watcher->_buffer + _offset);
      _update_current();
      _watcher->_logger->trace(
          "directory_watcher: offset: {}, bytes_read: {}, end reached {}",
          _offset, _watcher->_bytes_read, *this == _watcher->end());
      return *this;
    }

    bool operator!=(const iterator& other) const {
      return _offset != other._offset;
    }
    bool operator==(const iterator& other) const {
      return _offset == other._offset;
    }
  };

  directory_watcher(const std::string& to_watch_dir,
                    uint32_t mask,
                    bool non_blocking);
  directory_watcher(const directory_watcher&) = delete;
  directory_watcher& operator=(const directory_watcher&) = delete;
  ~directory_watcher();

  iterator watch();
  /**
   * @brief Wait until the watched directory has something to report, then call
   * the handler.
   *
   * Nothing is read here: the handler is expected to call watch(), which reads
   * what is ready without blocking. Waiting rather than reading is what lets a
   * caller be event-driven -- no cycle spent when nothing happens -- while
   * keeping every bit of watch(), from the meta-events to the iterator.
   *
   * One buffer is read per watch(), so a burst larger than that leaves the
   * descriptor readable and the next wait completes at once: re-arming after
   * each read drains the queue instead of postponing the rest.
   *
   * @param handler Called with the completion error code. A cancel() reports
   * boost::asio::error::operation_aborted, on which the handler must not
   * re-arm.
   */
  template <typename Handler>
  void async_wait_readable(Handler&& handler) {
    _sd.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                   std::forward<Handler>(handler));
  }
  /**
   * @brief Whether the watch is down: it was lost and could not be established
   * again, typically because the directory does not exist at the moment.
   *
   * Nothing is reported while this holds, and no event can wake the caller up
   * to notice -- the caller has to come back on its own.
   */
  bool watch_lost() const noexcept { return _wd < 0; }
  /**
   * @brief Cancel a pending async_wait_readable(), whose handler is then called
   * with operation_aborted. Needed at shutdown: a wait that is never cancelled
   * holds a handler that may outlive this object.
   */
  void cancel() noexcept;
  /**
   * @brief Whether the last watch() call left the caller unable to rely on
   * events alone, and has therefore to scan the watched directory itself.
   * Reading the answer clears it.
   */
  bool take_rescan_request() noexcept {
    bool retval = _rescan_needed;
    _rescan_needed = false;
    return retval;
  }
  iterator begin() { return iterator(this); }
  iterator end() { return iterator(this, _bytes_read); }
};
}  // namespace com::centreon::broker::file

#endif  // !CCB_FILE_DIRECTORY_WATCHER_HH
