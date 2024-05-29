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

/**
 * @brief struct returned by otl_converter::extract_host_serv_metric
 * success if host not empty
 * service may be empty if it's a host check
 *
 */
struct host_serv_metric {
  std::string_view host;
  std::string_view service;
  std::string_view metric;

  bool operator<(const host_serv_metric& right) const {
    int ret = host.compare(right.host);
    if (ret < 0)
      return true;
    if (ret > 0)
      return false;
    ret = service.compare(right.service);
    if (ret < 0)
      return true;
    if (ret > 0)
      return false;
    return metric.compare(right.metric) < 0;
  }
};

/**
 * @brief this list is the list of host service(may be empty) pairs
 * This list is shared between otel_command and his extractor
 *
 */
class host_serv_list {
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> _data;
  mutable std::mutex _data_m;

 public:
  using pointer = std::shared_ptr<host_serv_list>;

  void register_host_serv(const std::string& host,
                          const std::string& service_description);
  void unregister_host_serv(const std::string& host,
                            const std::string& service_description);

  bool is_allowed(const std::string& host,
                  const std::string& service_description) const;

  template <typename host_set, typename service_set>
  host_serv_metric is_allowed(const host_set& hosts,
                              const service_set& services) const;
};

template <typename host_set, typename service_set>
host_serv_metric host_serv_list::is_allowed(const host_set& hosts,
                                            const service_set& services) const {
  host_serv_metric ret;
  std::lock_guard l(_data_m);
  for (const auto& host : hosts) {
    auto host_search = _data.find(host);
    if (host_search != _data.end()) {
      const absl::flat_hash_set<std::string>& allowed_services =
          host_search->second;
      if (services.empty() && allowed_services.contains("")) {
        ret.host = host;
        return ret;
      }
      for (const auto serv : services) {
        if (allowed_services.contains(serv)) {
          ret.host = host;
          ret.service = serv;
          return ret;
        }
      }
    }
  }
  return ret;
}

using result_callback = std::function<void(const result&)>;

class open_telemetry_base;

class open_telemetry_base
    : public std::enable_shared_from_this<open_telemetry_base> {
 protected:
  static std::shared_ptr<open_telemetry_base> _instance;

 public:
  virtual ~open_telemetry_base() = default;

  static std::shared_ptr<open_telemetry_base>& instance() { return _instance; }
};

};  // namespace com::centreon::engine::commands::otel

#endif
