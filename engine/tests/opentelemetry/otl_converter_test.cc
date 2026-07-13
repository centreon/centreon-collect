/**
 * Copyright 2011-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>
#include <boost/algorithm/string.hpp>
#include <limits>

#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/modules/opentelemetry/otl_check_result_builder.hh"
#include "com/centreon/engine/modules/opentelemetry/telegraf/nagios_check_result_builder.hh"

#include "helper.hh"
#include "test_engine.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine;

class otl_converter_test : public TestEngine {};

TEST_F(otl_converter_test, empty_metrics) {
  telegraf::nagios_check_result_builder conv("", spdlog::default_logger());
  metric_to_datapoints empty;
  check_result res;
  ASSERT_FALSE(conv.build_result_from_metrics(empty, res));
}

const char* telegraf_example = R"(
  {
    "resourceMetrics": [
        {
            "resource": {
                "attributes": [
                    {
                        "key": "service.name",
                        "value": {
                            "stringValue": "demo_telegraf"
                        }
                    }
                ]
            },
            "scopeMetrics": [
                {
                    "scope": {},
                    "metrics": [
                        {
                            "name": "check_icmp_critical_lt",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0,
                                        "attributes": [
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            },
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "pl"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "%"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        },
                        {
                            "name": "check_icmp_critical_gt",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 500,
                                        "attributes": [
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            },
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 80,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "pl"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "%"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        },
                        {
                            "name": "check_icmp_min",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        },
                        {
                            "name": "check_icmp_value",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0.022,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "pl"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "%"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0.071,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rtmax"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0.008,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rtmin"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        },
                        {
                            "name": "check_icmp_warning_lt",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 0,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "pl"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "%"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        },
                        {
                            "name": "check_icmp_warning_gt",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 200,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 40,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "pl"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "%"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        },
                        {
                            "name": "check_icmp_state",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asInt": "0",
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            }
                                        ]
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

TEST_F(otl_converter_test, nagios_telegraf) {
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  (void)::google::protobuf::util::JsonStringToMessage(telegraf_example,
                                                      request.get());

  metric_to_datapoints received;
  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received[data_pt.get_metric().name()].insert(data_pt);
      });

  telegraf::nagios_check_result_builder conv("", spdlog::default_logger());
  check_result res;
  ASSERT_TRUE(conv.build_result_from_metrics(received, res));
  ASSERT_EQ(res.get_start_time().tv_sec, 1707744430);
  ASSERT_EQ(res.get_finish_time().tv_sec, 1707744430);
  ASSERT_EQ(res.get_return_code(), 0);
  ASSERT_EQ(res.get_output(),
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;0; rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
}

TEST_F(otl_converter_test, nagios_telegraf_le_ge) {
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  std::string example = telegraf_example;
  boost::algorithm::replace_all(example, "check_icmp_critical_gt",
                                "check_icmp_critical_ge");
  boost::algorithm::replace_all(example, "check_icmp_critical_lt",
                                "check_icmp_critical_le");

  (void)::google::protobuf::util::JsonStringToMessage(example, request.get());

  metric_to_datapoints received;
  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received[data_pt.get_metric().name()].insert(data_pt);
      });

  telegraf::nagios_check_result_builder conv("", spdlog::default_logger());
  check_result res;
  ASSERT_TRUE(conv.build_result_from_metrics(received, res));
  ASSERT_EQ(res.get_start_time().tv_sec, 1707744430);
  ASSERT_EQ(res.get_finish_time().tv_sec, 1707744430);
  ASSERT_EQ(res.get_return_code(), 0);
  ASSERT_EQ(
      res.get_output(),
      "OK|pl=0%;0:40;@0:80;; rta=0.022ms;0:200;@0:500;0; rtmax=0.071ms;;;; "
      "rtmin=0.008ms;;;;");
}

TEST_F(otl_converter_test, nagios_telegraf_max) {
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  std::string example = telegraf_example;
  boost::algorithm::replace_all(example, "check_icmp_min", "check_icmp_max");

  (void)::google::protobuf::util::JsonStringToMessage(example, request.get());

  metric_to_datapoints received;
  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received[data_pt.get_metric().name()].insert(data_pt);
      });

  telegraf::nagios_check_result_builder conv("", spdlog::default_logger());
  check_result res;
  ASSERT_TRUE(conv.build_result_from_metrics(received, res));
  ASSERT_EQ(res.get_start_time().tv_sec, 1707744430);
  ASSERT_EQ(res.get_finish_time().tv_sec, 1707744430);
  ASSERT_EQ(res.get_return_code(), 0);
  ASSERT_EQ(res.get_output(),
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;;0 rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
}
