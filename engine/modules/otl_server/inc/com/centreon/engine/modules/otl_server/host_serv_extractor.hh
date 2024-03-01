/*
** Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_SERVER_HOST_SERV_EXTRACTOR_HH
#define CCE_MOD_OTL_SERVER_HOST_SERV_EXTRACTOR_HH

#include "com/centreon/engine/commands/otel_interface.hh"
#include "otl_data_point.hh"

namespace com::centreon::engine::modules::otl_server {

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
 * @brief base class of host serv extractor
 *
 */
class host_serv_extractor : public commands::otel::host_serv_extractor {
  const std::string& _command_line;

  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      _host_serv_allowed;

  mutable absl::Mutex _host_serv_allowed_m;

 public:
  host_serv_extractor(const std::string& command_line)
      : _command_line(command_line) {}

  host_serv_extractor(const host_serv_extractor&) = delete;
  host_serv_extractor& operator=(const host_serv_extractor&) = delete;

  const std::string& get_command_line() const { return _command_line; }

  static std::shared_ptr<host_serv_extractor> create(
      const std::string& command_line);

  virtual host_serv_metric extract_host_serv_metric(
      const data_point&) const = 0;

  void register_host_serv(const std::string& host,
                          const std::string& service_description) override;

  void unregister_host_serv(const std::string& host,
                            const std::string& service_description) override;

  bool is_allowed(const std::string& host,
                  const std::string& service_description) const;

  template <typename host_set, typename service_set>
  host_serv_metric is_allowed(const host_set& hosts,
                              const service_set& services) const;
};

template <typename host_set, typename service_set>
host_serv_metric host_serv_extractor::is_allowed(
    const host_set& hosts,
    const service_set& services) const {
  host_serv_metric ret;
  absl::ReaderMutexLock l(&_host_serv_allowed_m);
  for (const auto& host : hosts) {
    auto host_search = _host_serv_allowed.find(host);
    if (host_search != _host_serv_allowed.end()) {
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

class host_serv_attributes_extractor : public host_serv_extractor {
  enum class attribute_owner { resource, scope, data_point };
  attribute_owner _host_path;
  std::string _host_key;
  attribute_owner _serv_path;
  std::string _serv_key;

 public:
  host_serv_attributes_extractor(const std::string& command_line);

  host_serv_metric extract_host_serv_metric(
      const data_point& data_pt) const override;
};

}  // namespace com::centreon::engine::modules::otl_server

#endif
