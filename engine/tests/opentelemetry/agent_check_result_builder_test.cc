/**
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>
#include <boost/algorithm/string.hpp>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/common/perfdata.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_check_result_builder.hh"

#include "test_engine.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine;

static const char* agent_exemple = R"(
{
  "resourceMetrics": [
      {
          "resource": {
              "attributes": [
                  {
                      "key": "host.name",
                      "value": {
                          "stringValue": "test_host"
                      }
                  },
                  {
                      "key": "service.name",
                      "value": {
                          "stringValue": ""
                      }
                  }
              ]
          },
          "scopeMetrics": [
              {
                  "metrics": [
                      {
                          "name": "status",
                          "description": "0",
                          "gauge": {
                              "dataPoints": [
                                  {
                                      "timeUnixNano": "1718345061146529731",
                                      "asInt": "0"
                                  }
                              ]
                          }
                      }
                  ]
              }
          ]
      },
      {
          "resource": {
              "attributes": [
                  {
                      "key": "host.name",
                      "value": {
                          "stringValue": "test_host"
                      }
                  },
                  {
                      "key": "service.name",
                      "value": {
                          "stringValue": "test_svc_builder"
                      }
                  }
              ]
          },
          "scopeMetrics": [
              {
                  "metrics": [
                      {
                          "name": "status",
                          "description": "output of plugin",
                          "gauge": {
                              "dataPoints": [
                                  {
                                      "timeUnixNano": "1718345061381922153",
                                      "asInt": "0"
                                  }
                              ]
                          }
                      },
                      {
                          "name": "metric",
                          "gauge": {
                              "dataPoints": [
                                  {
                                      "timeUnixNano": "1718345061381922153",
                                      "exemplars": [
                                          {
                                              "asDouble": 75,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "crit_gt"
                                                  }
                                              ]
                                          },
                                          {
                                              "asDouble": 0,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "crit_lt"
                                                  }
                                              ]
                                          },
                                          {
                                              "asDouble": 50,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "warn_gt"
                                                  }
                                              ]
                                          },
                                          {
                                              "asDouble": 0,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "warn_lt"
                                                  }
                                              ]
                                          }
                                      ],
                                      "asInt": "12"
                                  }
                              ]
                          }
                      },
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
                                      "asInt": "30"
                                  }
                              ]
                          }
                      }
                  ]
              }
          ]
      },
      {
          "resource": {
              "attributes": [
                  {
                      "key": "host.name",
                      "value": {
                          "stringValue": "test_host"
                      }
                  },
                  {
                      "key": "service.name",
                      "value": {
                          "stringValue": "test_svc_builder_2"
                      }
                  }
              ]
          },
          "scopeMetrics": [
              {
                  "metrics": [
                      {
                          "name": "status",
                          "description": "output taratata",
                          "gauge": {
                              "dataPoints": [
                                  {
                                      "timeUnixNano": "1718345061713456225",
                                      "asInt": "0"
                                  }
                              ]
                          }
                      },
                      {
                          "name": "metric",
                          "gauge": {
                              "dataPoints": [
                                  {
                                      "timeUnixNano": "1718345061713456225",
                                      "exemplars": [
                                          {
                                              "asDouble": 75,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "crit_ge"
                                                  }
                                              ]
                                          },
                                          {
                                              "asDouble": 50,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "warn_ge"
                                                  }
                                              ]
                                          },
                                          {
                                              "asDouble": 0,
                                              "filteredAttributes": [
                                                  {
                                                      "key": "warn_le"
                                                  }
                                              ]
                                          }
                                      ],
                                      "asInt": "12"
                                  }
                              ]
                          }
                      },
                      {
                          "name": "metric2",
                          "unit": "ms",
                          "gauge": {
                              "dataPoints": [
                                  {
                                      "timeUnixNano": "1718345061713456225",
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
                                      "asInt": "30"
                                  }
                              ]
                          }
                      }
                  ]
              }
          ]
      }
  ]
}
)";

class otl_agent_check_result_builder_test : public TestEngine {
 protected:
  absl::flat_hash_map<std::string /*service name*/, metric_to_datapoints>
      _received;

 public:
  otl_agent_check_result_builder_test() {
    metric_request_ptr request =
        std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                              ExportMetricsServiceRequest>();

    (void)::google::protobuf::util::JsonStringToMessage(agent_exemple,
                                                        request.get());

    otl_data_point::extract_data_points(
        request, [&](const otl_data_point& data_pt) {
          std::string service_name;
          for (const auto& attrib : data_pt.get_resource().attributes()) {
            if (attrib.key() == "service.name") {
              service_name = attrib.value().string_value();
              break;
            }
          }
          _received[service_name][data_pt.get_metric().name()].insert(data_pt);
        });
  }
};

TEST_F(otl_agent_check_result_builder_test, test_svc_builder) {
  auto check_result_builder = otl_check_result_builder::create(
      "--processor=centreon_agent", spdlog::default_logger());

  check_result res;
  bool success = check_result_builder->build_result_from_metrics(
      _received["test_svc_builder"], res);

  ASSERT_TRUE(success);
  ASSERT_EQ(res.get_return_code(), 0);
  ASSERT_EQ(res.get_start_time().tv_sec, 1718345061381922153 / 1000000000);
  ASSERT_EQ(res.get_finish_time().tv_sec, 1718345061381922153 / 1000000000);

  auto compare_to_excepted = [](const std::string& to_cmp) -> bool {
    return to_cmp ==
               "output of plugin| metric=12;0:50;0:75;; "
               "metric2=30ms;50:75;75:80;0;100" ||
           to_cmp ==
               "output of plugin| metric2=30ms;50:75;75:80;0;100 "
               "metric=12;0:50;0:75;;";
  };

  ASSERT_PRED1(compare_to_excepted, res.get_output());
}

TEST_F(otl_agent_check_result_builder_test, test_svc_builder_2) {
  auto check_result_builder = otl_check_result_builder::create(
      "--processor=centreon_agent", spdlog::default_logger());

  check_result res;
  bool success = check_result_builder->build_result_from_metrics(
      _received["test_svc_builder_2"], res);

  ASSERT_TRUE(success);
  ASSERT_EQ(res.get_return_code(), 0);
  ASSERT_EQ(res.get_start_time().tv_sec, 1718345061713456225 / 1000000000);
  ASSERT_EQ(res.get_finish_time().tv_sec, 1718345061713456225 / 1000000000);

  auto compare_to_excepted = [](const std::string& to_cmp) -> bool {
    return to_cmp ==
               "output taratata| metric=12;@0:50;@~:75;; "
               "metric2=30ms;~:75;75:80;0;100" ||
           to_cmp ==
               "output taratata| metric2=30ms;~:75;75:80;0;100 "
               "metric=12;@0:50;@~:75;;";
  };

  ASSERT_PRED1(compare_to_excepted, res.get_output());
}

static const char* agent_quoted_label_exemple = R"(
{
  "resourceMetrics": [
    {
      "resource": {
        "attributes": [
          { "key": "host.name", "value": { "stringValue": "test_host" } },
          { "key": "service.name", "value": { "stringValue": "test_svc_cert" } }
        ]
      },
      "scopeMetrics": [
        {
          "metrics": [
            {
              "name": "status",
              "description": "certificate ok",
              "gauge": {
                "dataPoints": [
                  { "timeUnixNano": "1718345061381922153", "asInt": "0" }
                ]
              }
            },
            {
              "name": "C=FR, S=Côte-d'Or#cert.expires",
              "unit": "s",
              "gauge": {
                "dataPoints": [
                  { "timeUnixNano": "1718345061381922153", "asInt": "123" }
                ]
              }
            }
          ]
        }
      ]
    }
  ]
}
)";

/**
 * Given a metric whose name contains spaces, equal signs and a quote
 * Then the perfdata label is quoted with its quote doubled, so that it is
 * parsed back to the same name
 */
TEST_F(otl_agent_check_result_builder_test, test_svc_builder_quoted_label) {
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  ASSERT_TRUE(::google::protobuf::util::JsonStringToMessage(
                  agent_quoted_label_exemple, request.get())
                  .ok());

  metric_to_datapoints received;
  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received[data_pt.get_metric().name()].insert(data_pt);
      });

  auto check_result_builder = otl_check_result_builder::create(
      "--processor=centreon_agent", spdlog::default_logger());

  check_result res;
  ASSERT_TRUE(check_result_builder->build_result_from_metrics(received, res));
  ASSERT_EQ(res.get_output(),
            "certificate ok| 'C=FR, S=Côte-d''Or#cert.expires'=123s;;;;");

  std::string_view perfdata(res.get_output());
  perfdata.remove_prefix(perfdata.find('|') + 1);
  std::list<com::centreon::common::perfdata> parsed =
      com::centreon::common::perfdata::parse_perfdata(
          0, 0, std::string(perfdata).c_str(), spdlog::default_logger());
  ASSERT_EQ(parsed.size(), 1u);
  ASSERT_EQ(parsed.begin()->name(), "C=FR, S=Côte-d'Or#cert.expires");
  ASSERT_EQ(parsed.begin()->value(), 123);
  ASSERT_EQ(parsed.begin()->unit(), "s");
}
