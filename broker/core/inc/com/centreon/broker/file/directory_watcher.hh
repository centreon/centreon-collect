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

namespace com::centreon::broker::file {

constexpr size_t BUF_LEN = sizeof(struct inotify_event) + NAME_MAX + 1;

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
  /* Watch descriptor */
  int _wd;
  int _fd;

  boost::asio::posix::stream_descriptor _sd;
  char _buffer[BUF_LEN];
  size_t _bytes_read = 0;

  std::shared_ptr<spdlog::logger> _logger;

 public:
  class iterator {
    directory_watcher* _watcher;
    size_t _offset = 0;
    inotify_event* _event = nullptr;
    std::pair<uint32_t, std::string_view> _current;

    inline void _update_current() {
      if (_offset < _watcher->_bytes_read) {
        _current.first = _event->mask;
        _current.second = _event->name;
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

    std::pair<uint32_t, std::string_view>& operator*() { return _current; }

    iterator& operator++() {
      _offset += sizeof(inotify_event) + _event->len;
      _event = reinterpret_cast<inotify_event*>(_watcher->_buffer + _offset);
      _update_current();
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
  iterator begin() { return iterator(this); }
  iterator end() { return iterator(this, _bytes_read); }
};
}  // namespace com::centreon::broker::file

#endif  // !CCB_FILE_DIRECTORY_WATCHER_HH
