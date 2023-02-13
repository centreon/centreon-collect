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
#ifndef CCE_LOG_V2_BASE_HH
#define CCE_LOG_V2_BASE_HH

#include <spdlog/common.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <asio.hpp>

#include "com/centreon/namespace.hh"

CC_BEGIN()

template <std::size_t N>
class log_v2_base {
  std::string _log_name;
  std::string _file_path;

  std::mutex _flush_timer_m;
  asio::system_timer _flush_timer;
  bool _flush_timer_active;

  std::shared_ptr<asio::io_context> _io_context;

 protected:
  std::atomic_bool _running;
  std::chrono::seconds _flush_interval;
  std::array<std::shared_ptr<spdlog::logger>, N> _log;

  log_v2_base(const std::string& logger_name,
              uint32_t flush_interval,
              std::string&& file_path,
              const std::shared_ptr<asio::io_context>& io_context)
      : _log_name(logger_name),
        _file_path{std::move(file_path)},
        _flush_timer(*io_context),
        _flush_timer_active(flush_interval != 0),
        _io_context(io_context),
        _running(false),
        _flush_interval{std::chrono::seconds(flush_interval)} {}

 public:
  virtual ~log_v2_base() noexcept = default;

  const std::string& log_name() const;
  const std::string& file_path() const;

  std::chrono::seconds get_flush_interval() const;
  void set_flush_interval(uint32_t second_flush_interval);
  void start_flush_timer();
  void stop_flush_timer();
  bool contains_level(const std::string& level_name) const;
  std::shared_ptr<spdlog::logger> get_logger(uint32_t log_type,
                                             const char* log_str);
  void set_logger(uint32_t, const std::shared_ptr<spdlog::logger>& lg);
  void set_file_path(const std::string& file_path);
};

CC_END()

#endif
