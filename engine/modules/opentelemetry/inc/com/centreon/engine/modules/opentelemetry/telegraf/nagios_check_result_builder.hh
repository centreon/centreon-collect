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

#ifndef CCE_MOD_OTL_NAGIOS_CONVERTER_HH
#define CCE_MOD_OTL_NAGIOS_CONVERTER_HH

namespace com::centreon::engine::modules::opentelemetry::telegraf {
/**
 * @brief telegraf accept to use nagios plugins
 * This converter parse metrics special naming to rebuild original check_result
 * an example of output:
 * @code {.json}
 *   {
 *       "name": "check_icmp_warning_gt",
 *       "gauge": {
 *           "dataPoints": [
 *               {
 *                   "timeUnixNano": "1707744430000000000",
 *                   "asDouble": 200,
 *                   "attributes": [
 *                       {
 *                           "key": "host",
 *                           "value": {
 *                               "stringValue": "localhost"
 *                           }
 *                       },
 *                       {
 *                           "key": "perfdata",
 *                           "value": {
 *                               "stringValue": "rta"
 *                           }
 *                       },
 *                       {
 *                           "key": "service",
 *                           "value": {
 *                               "stringValue": "check_icmp"
 *                           }
 *                       },
 *                       {
 *                           "key": "unit",
 *                           "value": {
 *                               "stringValue": "ms"
 *                           }
 *                       }
 *                   ]
 *               }
 *           ]
 *       }
 *   },
 *   {
 *       "name": "check_icmp_state",
 *       "gauge": {
 *           "dataPoints": [
 *               {
 *                   "timeUnixNano": "1707744430000000000",
 *                   "asInt": "0",
 *                   "attributes": [
 *                       {
 *                           "key": "host",
 *                           "value": {
 *                               "stringValue": "localhost"
 *                           }
 *                       },
 *                       {
 *                           "key": "service",
 *                           "value": {
 *                               "stringValue": "check_icmp"
 *                           }
 *                       }
 *                   ]
 *               }
 *           ]
 *       }
 *   },
 * @endcode
 *
 *
 */
class nagios_check_result_builder : public otl_check_result_builder {
 protected:
  bool _build_result_from_metrics(metric_name_to_fifo& fifos,
                                  commands::result& res) override;

 public:
  nagios_check_result_builder(const std::string& cmd_line,
                              uint64_t command_id,
                              const host& host,
                              const service* service,
                              std::chrono::system_clock::time_point timeout,
                              commands::otel::result_callback&& handler,
                              const std::shared_ptr<spdlog::logger>& logger)
      : otl_check_result_builder(cmd_line,
                                 command_id,
                                 host,
                                 service,
                                 timeout,
                                 std::move(handler),
                                 logger) {}
};

}  // namespace com::centreon::engine::modules::opentelemetry::telegraf

#endif
