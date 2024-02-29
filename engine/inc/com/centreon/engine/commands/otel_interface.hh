/**
 * Copyright 2024 Centreon
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

#ifndef CCE_COMMANDS_OTEL_INTERFACE_HH
#define CCE_COMMANDS_OTEL_INTERFACE_HH

#include "com/centreon/engine/commands/result.hh"
#include "com/centreon/engine/macros/defines.hh"

namespace com::centreon::engine::commands::otel {

class host_serv_extractor {
 public:
  virtual ~host_serv_extractor() = default;

  virtual void register_host_serv(const std::string& host,
                                  const std::string& service_description) = 0;

  virtual void unregister_host_serv(const std::string& host,
                                    const std::string& service_description) = 0;
};

using result_callback = std::function<void(const result&)>;

class open_telemetry_base;

class open_telemetry_base
    : public std::enable_shared_from_this<open_telemetry_base> {
 protected:
  static std::shared_ptr<open_telemetry_base> _instance;

 public:
  virtual ~open_telemetry_base() = default;

  static std::shared_ptr<open_telemetry_base>& instance() { return _instance; }

  virtual std::shared_ptr<host_serv_extractor> create_extractor(
      const std::string& cmdline) = 0;

  virtual bool check(const std::string& processed_cmd,
                     uint64_t command_id,
                     nagios_macros& macros,
                     uint32_t timeout,
                     commands::result& res,
                     result_callback&& handler) = 0;
};

};  // namespace com::centreon::engine::commands::otel

#endif
