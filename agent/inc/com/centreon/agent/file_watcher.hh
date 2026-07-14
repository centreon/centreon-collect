/**
 * Copyright 2026 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
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

#ifndef CENTREON_AGENT_FILE_WATCHER_HH
#define CENTREON_AGENT_FILE_WATCHER_HH

#include <filesystem>
#include <optional>

namespace com::centreon::agent {

/**
 * @brief watch a file for creation, modification and deletion
 * As there is no portable notification API between Linux and Windows, we poll
 * the last write time of the file at regular intervals. When a change is
 * detected, the on_change handler is called from the io_context thread.
 */
class file_watcher : public std::enable_shared_from_this<file_watcher> {
 public:
  using on_change_handler = std::function<void()>;

 private:
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  const std::filesystem::path _watched_path;
  const std::chrono::milliseconds _poll_interval;
  asio::steady_timer _poll_timer;
  on_change_handler _on_change;
  // nullopt when the file doesn't exist (or is not accessible)
  std::optional<std::filesystem::file_time_type> _last_write_time;
  bool _alive = true;

  void _start_poll_timer();
  void _poll_timer_handler(const boost::system::error_code& err);

 public:
  file_watcher(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string& watched_path,
               const std::chrono::milliseconds& poll_interval,
               on_change_handler&& on_change);

  file_watcher(const file_watcher&) = delete;
  file_watcher& operator=(const file_watcher&) = delete;

  static std::shared_ptr<file_watcher> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::string& watched_path,
      const std::chrono::milliseconds& poll_interval,
      on_change_handler&& on_change);

  void stop();

  const std::filesystem::path& get_watched_path() const {
    return _watched_path;
  }

  static std::optional<std::filesystem::file_time_type> get_last_write_time(
      const std::filesystem::path& file_path) noexcept;
};

}  // namespace com::centreon::agent

#endif
