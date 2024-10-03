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

namespace com::centreon::engine::modules::opentelemetry {
class metric_to_datapoints;
}

namespace com::centreon::engine::commands::otel {

/**
 * @brief struct returned by otl_check_result_builder::extract_host_serv_metric
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
 * This list is shared between otel_connector and his extractor
 *
 */
class host_serv_list {
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> _data;
  mutable absl::Mutex _data_m;

 public:
  using pointer = std::shared_ptr<host_serv_list>;

  void register_host_serv(const std::string& host,
                          const std::string& service_description);
  void remove(const std::string& host, const std::string& service_description);

  template <class string_type>
  bool contains(const string_type& host,
                const string_type& service_description) const;

  template <typename host_set, typename service_set>
  host_serv_metric match(const host_set& hosts,
                         const service_set& services) const;
};

/**
 * @brief test if a host serv pair is contained in list
 *
 * @param host
 * @param service_description
 * @return true found
 * @return false  not found
 */
template <class string_type>
bool host_serv_list::contains(const string_type& host,
                              const string_type& service_description) const {
  absl::ReaderMutexLock l(&_data_m);
  auto host_search = _data.find(host);
  if (host_search != _data.end()) {
    return host_search->second.contains(service_description);
  }
  return false;
}

template <typename host_set, typename service_set>
host_serv_metric host_serv_list::match(const host_set& hosts,
                                       const service_set& services) const {
  host_serv_metric ret;
  absl::ReaderMutexLock l(&_data_m);
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

/**
 * @brief When we receive an opentelemetry metric, we have to extract host and
 * service name in order to convert it in check_result.
 *  This is the job of the daughters of this class
 *
 */
class host_serv_extractor {
 public:
  virtual ~host_serv_extractor() = default;
};

class otl_check_result_builder_base {
 public:
  virtual ~otl_check_result_builder_base() = default;
  virtual void process_data_pts(
      const std::string_view& host,
      const std::string_view& serv,
      const modules::opentelemetry::metric_to_datapoints& data_pts) = 0;
};

class open_telemetry_base;

/**
 * @brief access point of opentelemetry module used by engine
 * All calls use open_telemetry_base::_instance
 *
 */
class open_telemetry_base
    : public std::enable_shared_from_this<open_telemetry_base> {
 protected:
  static std::shared_ptr<open_telemetry_base> _instance;

 public:
  virtual ~open_telemetry_base() = default;

  static std::shared_ptr<open_telemetry_base>& instance() { return _instance; }

  virtual std::shared_ptr<host_serv_extractor> create_extractor(
      const std::string& cmdline,
      const host_serv_list::pointer& host_serv_list) = 0;

  virtual std::shared_ptr<otl_check_result_builder_base>
  create_check_result_builder(const std::string& cmdline) = 0;
};

};  // namespace com::centreon::engine::commands::otel

#endif
