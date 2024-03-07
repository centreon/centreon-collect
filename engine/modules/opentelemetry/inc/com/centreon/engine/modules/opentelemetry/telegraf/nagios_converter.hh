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

#ifndef CCE_MOD_OTL_NAGIOS_CONVERTER_HH
#define CCE_MOD_OTL_NAGIOS_CONVERTER_HH

namespace com::centreon::engine::modules::opentelemetry::telegraf {
class otl_nagios_converter : public otl_converter {
 protected:
  bool _build_result_from_metrics(metric_name_to_fifo& fifos,
                                  commands::result& res) override;

 public:
  otl_nagios_converter(const std::string& cmd_line,
                       uint64_t command_id,
                       const host& host,
                       const service* service,
                       std::chrono::system_clock::time_point timeout,
                       commands::otel::result_callback&& handler)
      : otl_converter(cmd_line,
                      command_id,
                      host,
                      service,
                      timeout,
                      std::move(handler)) {}
};

}  // namespace com::centreon::engine::modules::opentelemetry::telegraf

#endif
