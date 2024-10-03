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

#ifndef CCE_MOD_OTL_AGENT_CHECK_RESULT_BUILDER_HH
#define CCE_MOD_OTL_AGENT_CHECK_RESULT_BUILDER_HH

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

/**
 * @brief in order to save network usage, agent store metrics infos in examplar
 * An example of protobuf data:
 * @code {.json}
    {
    "name": "metric2",
    "unit": "ms",
    "gauge": {
        "dataPoints": [
            {
                "timeUnixNano": "1718345061381922153",
                "exemplars": [
                    {
                        "asDouble": 80,
                        "filteredAttributes": [
                            {
                                "key": "crit_gt"
                            }
                        ]
                    },
                    {
                        "asDouble": 75,
                        "filteredAttributes": [
                            {
                                "key": "crit_lt"
                            }
                        ]
                    },
                    {
                        "asDouble": 75,
                        "filteredAttributes": [
                            {
                                "key": "warn_gt"
                            }
                        ]
                    },
                    {
                        "asDouble": 50,
                        "filteredAttributes": [
                            {
                                "key": "warn_lt"
                            }
                        ]
                    },
                    {
                        "asDouble": 0,
                        "filteredAttributes": [
                            {
                                "key": "min"
                            }
                        ]
                    },
                    {
                        "asDouble": 100,
                        "filteredAttributes": [
                            {
                                "key": "max"
                            }
                        ]
                    }
                ],
                "asDouble": 30
            }
        ]
    }
 * @endcode
 *
 *
 */
class agent_check_result_builder : public otl_check_result_builder {
 public:
  agent_check_result_builder(const std::string& cmd_line,
                             const std::shared_ptr<spdlog::logger>& logger)
      : otl_check_result_builder(cmd_line, logger) {}

  bool build_result_from_metrics(const metric_to_datapoints& data_pts,
                                 check_result& res) override;
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif