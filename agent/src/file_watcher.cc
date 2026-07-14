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
#include "spdlog/spdlog.h"

using namespace com::centreon::agent;

/**
 * @brief Construct a new file_watcher, don't use it, use load instead
 *
 * @param io_context
 * @param logger
 * @param watched_path path of the file to watch
 * @param poll_interval interval between two last write time reads
 * @param on_change called from the io_context thread on file change
 */
file_watcher::file_watcher(const std::shared_ptr<asio::io_context>& io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           const std::string& watched_path,
                           const std::chrono::milliseconds& poll_interval,
                           on_change_handler&& on_change)
    : _io_context(io_context),
      _logger(logger),
      _watched_path(watched_path),
      _poll_interval(poll_interval),
      _poll_timer(*io_context),
      _on_change(std::move(on_change)),
      _last_write_time(get_last_write_time(_watched_path)) {}

/**
 * @brief create and start a new file_watcher
 *
 * @return std::shared_ptr<file_watcher>
 */
std::shared_ptr<file_watcher> file_watcher::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& watched_path,
    const std::chrono::milliseconds& poll_interval,
    on_change_handler&& on_change) {
  std::shared_ptr<file_watcher> watcher = std::make_shared<file_watcher>(
      io_context, logger, watched_path, poll_interval, std::move(on_change));
  SPDLOG_LOGGER_DEBUG(logger, "watch file {} every {}", watched_path,
                      poll_interval);
  watcher->_start_poll_timer();
  return watcher;
}

/**
 * @brief last write time of a file, nullopt if the file doesn't exist or is
 * not accessible
 */
std::optional<std::filesystem::file_time_type>
file_watcher::get_last_write_time(
    const std::filesystem::path& file_path) noexcept {
  std::error_code err;
  std::filesystem::file_time_type write_time =
      std::filesystem::last_write_time(file_path, err);
  if (err) {
    return std::nullopt;
  }
  return write_time;
}

void file_watcher::_start_poll_timer() {
  _poll_timer.expires_after(_poll_interval);
  _poll_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_poll_timer_handler(err);
      });
}

/**
 * @brief compare current file last write time to the last known one and call
 * _on_change if they differ (file created, modified or deleted)
 *
 * @param err timer error
 */
void file_watcher::_poll_timer_handler(const boost::system::error_code& err) {
  if (err || !_alive) {
    return;
  }
  std::optional<std::filesystem::file_time_type> current_write_time =
      get_last_write_time(_watched_path);
  if (current_write_time != _last_write_time) {
    _last_write_time = current_write_time;
    SPDLOG_LOGGER_DEBUG(_logger, "file {} has been {}", _watched_path.string(),
                        current_write_time ? "updated" : "removed");
    try {
      _on_change();
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(_logger, "error while handling change of file {}: {}",
                          _watched_path.string(), e.what());
    }
  }
  _start_poll_timer();
}

/**
 * @brief stop watching, after this call this object does nothing more and can
 * be deleted
 */
void file_watcher::stop() {
  _alive = false;
  _poll_timer.cancel();
}
