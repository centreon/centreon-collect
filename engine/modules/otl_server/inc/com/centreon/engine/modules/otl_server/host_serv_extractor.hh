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
};

class host_serv_attributes_extractor : public host_serv_extractor {
  enum class attribute_owner { resource, scope, data_point };
  attribute_owner _host_path;
  std::string _host_tag;
  attribute_owner _serv_path;
  std::string _serv_tag;

 public:
  host_serv_attributes_extractor(const std::string& command_line);

  host_serv_metric extract_host_serv_metric(
      const data_point& data_pt) const override;
};

}  // namespace com::centreon::engine::modules::otl_server

#endif
