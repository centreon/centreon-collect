/*
** Copyright 2022-2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/
#include "com/centreon/log_v2_base.hh"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>

namespace com {
namespace centreon {

using namespace spdlog;

template <std::size_t N>
const std::string& log_v2_base<N>::log_name() const {
  return _log_name;
}

template <std::size_t N>
const std::string& log_v2_base<N>::file_path() const {
  return _file_path;
}

template <std::size_t N>
std::chrono::seconds log_v2_base<N>::get_flush_interval() const {
  return _flush_interval;
}

template <std::size_t N>
void log_v2_base<N>::set_flush_interval(uint32_t second_flush_interval) {
  _flush_interval = std::chrono::seconds(second_flush_interval);
}

/**
 * @brief logs are written periodically to disk
 *
 */
template <std::size_t N>
void log_v2_base<N>::start_flush_timer() {
  if (_flush_interval == std::chrono::seconds(0)) {
    stop_flush_timer();
    for (auto& logger : _log)
      logger->flush_on(spdlog::level::trace);
  }

  std::lock_guard<std::mutex> l(_flush_timer_m);

  _flush_timer.expires_after(_flush_interval);
  _flush_timer.async_wait([me = this](const asio::error_code& err) {
    if (err || !me->_flush_timer_active) {
      return;
    }
    for (auto& logger : me->_log)
      logger->flush();

    me->start_flush_timer();
  });
}

/**
 * @brief stop flush timer
 *
 */
template <std::size_t N>
void log_v2_base<N>::stop_flush_timer() {
  std::lock_guard<std::mutex> l(_flush_timer_m);
  _flush_timer_active = false;
  _flush_timer.cancel();
}

/**
 * @brief Check if the given level makes part of the available levels.
 *
 * @param level A level as a string
 *
 * @return A boolean.
 */
template <std::size_t N>
bool log_v2_base<N>::contains_level(const std::string& level_name) const {
  /* spdlog wants 'off' to disable a log but we tolerate 'disabled' */
  if (level_name == "disabled" || level_name == "off")
    return true;

  spdlog::level::level_enum l = spdlog::level::from_str(level_name);
  return l != spdlog::level::off;
}

template <std::size_t N>
void log_v2_base<N>::set_logger(uint32_t idx,
                                const std::shared_ptr<spdlog::logger>& lg) {
  _log[idx] = lg;
}

template <std::size_t N>
void log_v2_base<N>::set_file_path(const std::string& file_path) {
  _file_path = file_path;
}

/**
 * @brief this private static method is used to access a specific logger
 *
 * @param log_type
 * @param log_str
 * @return std::shared_ptr<spdlog::logger>
 */
template <std::size_t N>
std::shared_ptr<spdlog::logger> log_v2_base<N>::get_logger(
    uint32_t log_type,
    const char* log_str) {
  if (_running)
    return _log[log_type];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>(log_str, null_sink);
  }
}

template class log_v2_base<13>;
template class log_v2_base<17>;
}  // namespace centreon
}  // namespace com
