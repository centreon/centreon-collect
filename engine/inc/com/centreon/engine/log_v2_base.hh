/*
** Copyright 2022 Centreon
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

#include "namespace.hh"

CCE_BEGIN()

class log_v2_base {
 protected:

  std::string _log_name;
  std::chrono::seconds _flush_interval;
  std::string _file_path;

 public:
  log_v2_base(const std::string& logger_name) : _log_name(logger_name) {}
  virtual ~log_v2_base() noexcept = default;

  const std::string& log_name() const { return _log_name; }
  const std::string& file_path() const { return _file_path; }

  std::chrono::seconds get_flush_interval() const { return _flush_interval; }
  void set_flush_interval(uint32_t second_flush_interval) {
    _flush_interval = std::chrono::seconds(second_flush_interval);
  }
};

class log_v2_logger : public spdlog::logger {
  log_v2_base* _parent;

 public:
  template <class sink_iter>
  log_v2_logger(std::string name,
                log_v2_base* parent,
                sink_iter begin,
                sink_iter end)
      : spdlog::logger(name, begin, end), _parent(parent) {}

  log_v2_logger(std::string name, log_v2_base* parent, spdlog::sink_ptr sink)
      : spdlog::logger(name, sink), _parent(parent) {}

  log_v2_base* get_parent() { return _parent; }
};

CCE_END()

#endif
