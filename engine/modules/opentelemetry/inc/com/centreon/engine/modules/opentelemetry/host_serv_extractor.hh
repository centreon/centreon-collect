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

#ifndef CCE_MOD_OTL_SERVER_HOST_SERV_EXTRACTOR_HH
#define CCE_MOD_OTL_SERVER_HOST_SERV_EXTRACTOR_HH

#include "com/centreon/engine/commands/otel_interface.hh"
#include "otl_data_point.hh"

namespace com::centreon::engine::modules::opentelemetry {

using host_serv_metric = commands::otel::host_serv_metric;

/**
 * @brief base class of host serv extractor
 *
 */
class host_serv_extractor : public commands::otel::host_serv_extractor {
  const std::string _command_line;
  const commands::otel::host_serv_list::pointer _host_serv_list;

 public:
  host_serv_extractor(
      const std::string& command_line,
      const commands::otel::host_serv_list::pointer& host_serv_list)
      : _command_line(command_line), _host_serv_list(host_serv_list) {}

  host_serv_extractor(const host_serv_extractor&) = delete;
  host_serv_extractor& operator=(const host_serv_extractor&) = delete;

  const std::string& get_command_line() const { return _command_line; }

  static std::shared_ptr<host_serv_extractor> create(
      const std::string& command_line,
      const commands::otel::host_serv_list::pointer& host_serv_list);

  virtual host_serv_metric extract_host_serv_metric(
      const otl_data_point&) const = 0;

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
  return _host_serv_list->match(hosts, services);
}

/**
 * @brief this class try to find host service in opentelemetry attributes object
 * It may search in data resource attributes, scope attributes or otl_data_point
 * attributes
 * An example of telegraf otel data:
 * @code {.json}
 *   "dataPoints": [
 *     {
 *         "timeUnixNano": "1707744430000000000",
 *         "asDouble": 500,
 *         "attributes": [
 *             {
 *                 "key": "unit",
 *                 "value": {
 *                     "stringValue": "ms"
 *                 }
 *             },
 *             {
 *                 "key": "host",
 *                 "value": {
 *                     "stringValue": "localhost"
 *                 }
 *             },
 *             {
 *                 "key": "perfdata",
 *                 "value": {
 *                     "stringValue": "rta"
 *                 }
 *             },
 *             {
 *                 "key": "service",
 *                 "value": {
 *                     "stringValue": "check_icmp"
 *                 }
 *             }
 *         ]
 *     },
 * @endcode
 *
 */
class host_serv_attributes_extractor : public host_serv_extractor {
  enum class attribute_owner { resource, scope, otl_data_point };
  attribute_owner _host_path;
  std::string _host_key;
  attribute_owner _serv_path;
  std::string _serv_key;

 public:
  host_serv_attributes_extractor(
      const std::string& command_line,
      const commands::otel::host_serv_list::pointer& host_serv_list);

  host_serv_metric extract_host_serv_metric(
      const otl_data_point& data_pt) const override;
};

}  // namespace com::centreon::engine::modules::opentelemetry

namespace fmt {

template <>
struct formatter<
    ::com::centreon::engine::modules::opentelemetry::host_serv_extractor> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const ::com::centreon::engine::modules::opentelemetry::
                  host_serv_extractor& extract,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    return format_to(ctx.out(), "cmd_line: {}", extract.get_command_line());
  }
};

}  // namespace fmt

#endif
