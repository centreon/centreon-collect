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

#ifndef CCCM_FILE_WATCHER_HH
#define CCCM_FILE_WATCHER_HH

#include <filesystem>
#include <mutex>

namespace com::centreon::common {

/**
 * @brief Watch a single file for creation, modification and deletion.
 *
 * this class reacts to filesystem events natively: inotify on Linux and
 * ReadDirectoryChangesW on Windows, both driven inside a
 * boost::asio::io_context. It reacts to modification, creation and deletion of
 * the file. Because the file may not exist yet, or be deleted and later
 * recreated, it watches the *parent directory* and filters events on the
 * watched file name.
 *
 * Native watchers report every raw event and a single logical change ( an
 * editor save) can produce a burst of them. Events are therefore coalesced: the
 * on_change handler is called once, from the io_context thread, after a short
 * debounce delay with no further event.
 *
 * If the native watch cannot be established (parent directory missing, inotify
 * limit reached, directory open failure, ...) an error is logged and the
 * watcher stays inert: it never calls on_change.
 *
 * stop() may be called from any thread (this is the only method callers may
 * invoke from outside the io_context thread): a mutex serializes it with the
 * native watch setup/teardown and event handling that otherwise all run on the
 * io_context thread, so a concurrent stop() can't race a handle being armed or
 * used. The on_change handler itself is invoked without the lock held.
 */
class file_watcher : public std::enable_shared_from_this<file_watcher> {
 public:
  using on_change_handler = std::function<void()>;

 private:
  // platform specific state defined in file_watcher.cc
  struct impl;

  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  const std::filesystem::path _watched_path;
  // directory actually watched (parent of _watched_path)
  const std::filesystem::path _parent_path;
  // name of the file to filter events on, inside _parent_path
  const std::filesystem::path _file_name;
  asio::steady_timer _debounce_timer;
  on_change_handler _on_change;
  std::unique_ptr<impl> _impl;
  bool _alive = true;
  std::mutex _mutex;

  bool _start_native() noexcept;
  void _arm_native();
  void _stop_native() noexcept;
  void _on_native_event();
  void _schedule_change();
  void _debounce_handler(const boost::system::error_code& err);

 public:
  file_watcher(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::filesystem::path& watched_path,
               on_change_handler&& on_change);
  ~file_watcher();

  file_watcher(const file_watcher&) = delete;
  file_watcher& operator=(const file_watcher&) = delete;

  static std::shared_ptr<file_watcher> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::filesystem::path& watched_path,
      on_change_handler&& on_change);

  void stop();

  const std::filesystem::path& get_watched_path() const {
    return _watched_path;
  }
};

}  // namespace com::centreon::common

#endif
