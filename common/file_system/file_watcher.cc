/**
 * Copyright 2026 Centreon
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

#include "file_watcher.hh"

#include <array>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#else
#include <sys/inotify.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cstring>
#endif

using namespace com::centreon::common;

namespace {
// A single logical change (an editor save, ...) may emit a
// burst of native events. We coalesce them: on_change is called once, this long
// after the last event of a burst.
constexpr std::chrono::milliseconds debounce_delay{200};
}  // namespace

/**
 * @brief Platform specific state of a file_watcher.
 *
 * On Linux it holds the inotify instance (wrapped in an asio descriptor) and
 * its read buffer. On Windows it holds the directory handle opened for
 * overlapped ReadDirectoryChangesW, the OVERLAPPED and its completion event
 * (wrapped in an asio object_handle) and the notification buffer.
 */
struct file_watcher::impl {
#ifdef _WIN32
  HANDLE dir_handle = INVALID_HANDLE_VALUE;
  OVERLAPPED overlapped{};
  std::unique_ptr<asio::windows::object_handle> event_handle;
  // ReadDirectoryChangesW output; must stay DWORD aligned (std::vector
  // satisfies it) and live for the whole overlapped operation
  std::vector<std::byte> buffer;
#else
  int inotify_fd = -1;
  int watch_descriptor = -1;
  std::unique_ptr<asio::posix::stream_descriptor> descriptor;
  // large enough to drain several inotify_event records (name included) at
  // once; aligned as inotify requires for the reinterpret_cast of its content
  alignas(struct inotify_event) std::array<char, 8 * 1024> buffer;
#endif
};

/**
 * @brief Construct a new file_watcher, don't use it directly, use load instead.
 *
 * @param io_context io_context whose thread runs the notifications
 * @param logger logger
 * @param watched_path path of the file to watch
 * @param on_change called from the io_context thread on file change
 */
file_watcher::file_watcher(const std::shared_ptr<asio::io_context>& io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           const std::filesystem::path& watched_path,
                           on_change_handler&& on_change)
    : _io_context(io_context),
      _logger(logger),
      _watched_path(watched_path),
      _parent_path(watched_path.has_parent_path() ? watched_path.parent_path()
                                                  : std::filesystem::path(".")),
      _file_name(watched_path.filename()),
      _debounce_timer(*io_context),
      _on_change(std::move(on_change)),
      _impl(std::make_unique<impl>()) {}

file_watcher::~file_watcher() {
  _stop_native();
}

/**
 * @brief Create and start a new file_watcher.
 *
 * If the native watch can't be established, an error is logged and the returned
 * watcher stays inert (it never calls on_change).
 *
 * @return std::shared_ptr<file_watcher>
 */
std::shared_ptr<file_watcher> file_watcher::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::filesystem::path& watched_path,
    on_change_handler&& on_change) {
  std::shared_ptr<file_watcher> watcher = std::make_shared<file_watcher>(
      io_context, logger, watched_path, std::move(on_change));
  // the native descriptors and asio objects must only be touched from the
  // io_context thread, so start the watch there
  asio::post(*io_context, [watcher]() {
    if (!watcher->_alive) {
      return;
    }
    if (watcher->_start_native()) {
      SPDLOG_LOGGER_DEBUG(watcher->_logger, "watching file {}",
                          watcher->_watched_path.string());
      watcher->_arm_native();
    } else {
      SPDLOG_LOGGER_ERROR(watcher->_logger,
                          "can't watch file {}: changes won't be notified",
                          watcher->_watched_path.string());
    }
  });
  return watcher;
}

/**
 * @brief Stop watching. After this call the object does nothing more and can be
 * deleted. Must be called from the io_context thread (asio timers and
 * descriptors are not thread safe).
 */
void file_watcher::stop() {
  _alive = false;
  _debounce_timer.cancel();
  _stop_native();
}

/**
 * @brief (Re)arm the debounce timer. When it expires without any further event,
 * _debounce_handler calls on_change. Called from the io_context thread.
 */
void file_watcher::_schedule_change() {
  if (!_alive) {
    return;
  }
  _debounce_timer.expires_after(debounce_delay);
  _debounce_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_debounce_handler(err);
      });
}

/**
 * @brief Called once per burst of changes of the watched file (created,
 * modified or deleted), from the io_context thread.
 *
 * @param err timer error, set (operation_aborted) when the timer was
 * rescheduled by a new event or cancelled by stop()
 */
void file_watcher::_debounce_handler(const boost::system::error_code& err) {
  if (err || !_alive) {
    return;
  }
  SPDLOG_LOGGER_DEBUG(_logger, "file {} has changed", _watched_path.string());
  try {
    _on_change();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "error while handling change of file {}: {}",
                        _watched_path.string(), e.what());
  }
}

#ifdef _WIN32

/**
 * @brief Open the watched directory for overlapped change notifications.
 *
 * @return true on success, false if the directory can't be opened.
 */
bool file_watcher::_start_native() noexcept {
  try {
    _impl->buffer.resize(64 * 1024);
    _impl->dir_handle =
        CreateFileW(_parent_path.wstring().c_str(), FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (_impl->dir_handle == INVALID_HANDLE_VALUE) {
      return false;
    }
    // manual reset event, signalled by ReadDirectoryChangesW completion
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
      CloseHandle(_impl->dir_handle);
      _impl->dir_handle = INVALID_HANDLE_VALUE;
      return false;
    }
    _impl->overlapped.hEvent = event;
    // object_handle takes ownership of the event and closes it on destruction
    _impl->event_handle =
        std::make_unique<asio::windows::object_handle>(*_io_context, event);
    return true;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "can't start watch on {}: {}",
                        _parent_path.string(), e.what());
    _stop_native();
    return false;
  }
}

/**
 * @brief Issue a ReadDirectoryChangesW and wait for its completion event.
 */
void file_watcher::_arm_native() {
  if (!_alive || _impl->dir_handle == INVALID_HANDLE_VALUE) {
    return;
  }
  ResetEvent(_impl->overlapped.hEvent);

  BOOL ok = ReadDirectoryChangesW(
      _impl->dir_handle, _impl->buffer.data(),
      static_cast<DWORD>(_impl->buffer.size()), FALSE,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
          FILE_NOTIFY_CHANGE_SIZE,
      nullptr, &_impl->overlapped, nullptr);
  if (!ok) {
    SPDLOG_LOGGER_ERROR(_logger, "ReadDirectoryChangesW failed on {}: {}",
                        _parent_path.string(), GetLastError());
    return;
  }
  _impl->event_handle->async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_on_native_event();
        if (!err) {
          me->_arm_native();
        }
      });
}

/**
 * @brief Parse the completed notification buffer and schedule a change if the
 * watched file is impacted.
 */
void file_watcher::_on_native_event() {
  if (!_alive || _impl->dir_handle == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD bytes = 0;
  if (!GetOverlappedResult(_impl->dir_handle, &_impl->overlapped, &bytes,
                           FALSE)) {
    return;
  }
  // zero bytes means the buffer overflowed: we can't tell what changed, assume
  // our file did
  if (bytes == 0) {
    _schedule_change();
    return;
  }
  const std::wstring& target = _file_name.native();
  DWORD offset = 0;
  for (;;) {
    auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
        _impl->buffer.data() + offset);
    // react to modification
    bool relevant = info->Action == FILE_ACTION_MODIFIED;
    size_t name_len = info->FileNameLength / sizeof(WCHAR);
    if (relevant && name_len == target.size() &&
        _wcsnicmp(info->FileName, target.c_str(), name_len) == 0) {
      _schedule_change();
      break;
    }
    if (info->NextEntryOffset == 0) {
      break;
    }
    offset += info->NextEntryOffset;
  }
}

/**
 * @brief Cancel the pending read and release the directory handle and event.
 */
void file_watcher::_stop_native() noexcept {
  if (_impl->dir_handle != INVALID_HANDLE_VALUE) {
    CancelIoEx(_impl->dir_handle, &_impl->overlapped);
    CloseHandle(_impl->dir_handle);
    _impl->dir_handle = INVALID_HANDLE_VALUE;
  }
  if (_impl->event_handle) {
    boost::system::error_code ignored;
    _impl->event_handle->close(ignored);
    _impl->event_handle.reset();
  } else if (_impl->overlapped.hEvent) {
    // event created but not yet adopted by event_handle ( _start_native
    // failed between CreateEventW and object_handle construction): close it
    // here so it doesn't leak
    CloseHandle(_impl->overlapped.hEvent);
  }
  _impl->overlapped.hEvent = nullptr;
}

#else

/**
 * @brief Create the inotify instance and add a watch on the file itself.
 *
 * The watch is placed on the file.
 *
 * @return true on success, false if inotify can't be set up.
 */
bool file_watcher::_start_native() noexcept {
  try {
    _impl->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (_impl->inotify_fd < 0) {
      return false;
    }
    // watch the file itself for content changes (IN_CLOSE_WRITE only)
    _impl->watch_descriptor = inotify_add_watch(
        _impl->inotify_fd, _watched_path.c_str(), IN_CLOSE_WRITE);
    if (_impl->watch_descriptor < 0) {
      close(_impl->inotify_fd);
      _impl->inotify_fd = -1;
      return false;
    }
    _impl->descriptor = std::make_unique<asio::posix::stream_descriptor>(
        *_io_context, _impl->inotify_fd);
    return true;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "can't start watch on {}: {}",
                        _watched_path.string(), e.what());
    _stop_native();
    return false;
  }
}

/**
 * @brief Wait for the inotify descriptor to become readable.
 */
void file_watcher::_arm_native() {
  if (!_alive || !_impl->descriptor) {
    return;
  }
  _impl->descriptor->async_wait(
      asio::posix::stream_descriptor::wait_read,
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (err) {
          return;
        }
        me->_on_native_event();
        me->_arm_native();
      });
}

/**
 * @brief Drain pending inotify events and schedule a change. A single file is
 * watched with IN_CLOSE_WRITE only, so every event delivered here is a
 * completed write to that file.
 */
void file_watcher::_on_native_event() {
  if (!_alive || _impl->inotify_fd < 0) {
    return;
  }
  for (;;) {
    ssize_t len =
        read(_impl->inotify_fd, _impl->buffer.data(), _impl->buffer.size());
    if (len <= 0) {
      // EAGAIN: no more events queued
      break;
    }
    for (char* ptr = _impl->buffer.data(); ptr < _impl->buffer.data() + len;) {
      auto* event = reinterpret_cast<const inotify_event*>(ptr);
      if (event->mask & IN_CLOSE_WRITE) {
        _schedule_change();
      }
      ptr += sizeof(inotify_event) + event->len;
    }
  }
}

/**
 * @brief Close the inotify descriptor (which removes the watch).
 */
void file_watcher::_stop_native() noexcept {
  if (_impl->descriptor) {
    boost::system::error_code ignored;
    _impl->descriptor->close(ignored);
    _impl->descriptor.reset();
    // the descriptor owns and closed inotify_fd
    _impl->inotify_fd = -1;
  } else if (_impl->inotify_fd >= 0) {
    close(_impl->inotify_fd);
    _impl->inotify_fd = -1;
  }
  _impl->watch_descriptor = -1;
}

#endif
