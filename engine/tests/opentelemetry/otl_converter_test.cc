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
#include "common/engine_legacy_conf/host.hh"
#include "common/engine_legacy_conf/service.hh"

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/modules/opentelemetry/data_point_fifo_container.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_check_result_builder.hh"
#include "com/centreon/engine/modules/opentelemetry/telegraf/nagios_check_result_builder.hh"

#include "helper.hh"
#include "test_engine.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine;

class otl_converter_test : public TestEngine {
 public:
  void SetUp() override;
  void TearDown() override;
};

void otl_converter_test::SetUp() {
  configuration::error_cnt err;
  init_config_state();
  timeperiod::timeperiods.clear();
  contact::contacts.clear();
  host::hosts.clear();
  host::hosts_by_id.clear();
  service::services.clear();
  service::services_by_id.clear();
  config->contacts().clear();
  configuration::applier::contact ct_aply;
  configuration::contact ctct{new_configuration_contact("admin", true)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(*config);
  ct_aply.resolve_object(ctct, err);

  configuration::host hst{new_configuration_host("localhost", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::service svc{
      new_configuration_service("localhost", "check_icmp", "admin")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);
  data_point_fifo::update_fifo_limit(std::numeric_limits<time_t>::max(), 10);
}

void otl_converter_test::TearDown() {
  deinit_config_state();
}

TEST_F(otl_converter_test, empty_fifo) {
  data_point_fifo_container empty;
  telegraf::nagios_check_result_builder conv(
      "", 1, *host::hosts.begin()->second,
      service::services.begin()->second.get(),
      std::chrono::system_clock::time_point(), [&](const commands::result&) {},
      spdlog::default_logger());
  commands::result res;
  ASSERT_FALSE(conv.sync_build_result_from_metrics(empty, res));
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
  data_point_fifo_container received;
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  ::google::protobuf::util::JsonStringToMessage(telegraf_example,
                                                request.get());

  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received.add_data_point("localhost", "check_icmp",
                                data_pt.get_metric().name(), data_pt);
      });

  telegraf::nagios_check_result_builder conv(
      "", 1, *host::hosts.begin()->second,
      service::services.begin()->second.get(),
      std::chrono::system_clock::time_point(), [&](const commands::result&) {},
      spdlog::default_logger());
  commands::result res;
  ASSERT_TRUE(conv.sync_build_result_from_metrics(received, res));
  ASSERT_EQ(res.command_id, 1);
  ASSERT_EQ(res.start_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.end_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(res.output,
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;0; rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
}

TEST_F(otl_converter_test, nagios_telegraf_le_ge) {
  data_point_fifo_container received;
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  std::string example = telegraf_example;
  boost::algorithm::replace_all(example, "check_icmp_critical_gt",
                                "check_icmp_critical_ge");
  boost::algorithm::replace_all(example, "check_icmp_critical_lt",
                                "check_icmp_critical_le");

  ::google::protobuf::util::JsonStringToMessage(example, request.get());

  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received.add_data_point("localhost", "check_icmp",
                                data_pt.get_metric().name(), data_pt);
      });

  telegraf::nagios_check_result_builder conv(
      "", 1, *host::hosts.begin()->second,
      service::services.begin()->second.get(),
      std::chrono::system_clock::time_point(), [&](const commands::result&) {},
      spdlog::default_logger());
  commands::result res;
  ASSERT_TRUE(conv.sync_build_result_from_metrics(received, res));
  ASSERT_EQ(res.command_id, 1);
  ASSERT_EQ(res.start_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.end_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(
      res.output,
      "OK|pl=0%;0:40;@0:80;; rta=0.022ms;0:200;@0:500;0; rtmax=0.071ms;;;; "
      "rtmin=0.008ms;;;;");
}

TEST_F(otl_converter_test, nagios_telegraf_max) {
  data_point_fifo_container received;
  metric_request_ptr request =
      std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                            ExportMetricsServiceRequest>();
  std::string example = telegraf_example;
  boost::algorithm::replace_all(example, "check_icmp_min", "check_icmp_max");

  ::google::protobuf::util::JsonStringToMessage(example, request.get());

  otl_data_point::extract_data_points(
      request, [&](const otl_data_point& data_pt) {
        received.add_data_point("localhost", "check_icmp",
                                data_pt.get_metric().name(), data_pt);
      });

  telegraf::nagios_check_result_builder conv(
      "", 1, *host::hosts.begin()->second,
      service::services.begin()->second.get(),
      std::chrono::system_clock::time_point(), [&](const commands::result&) {},
      spdlog::default_logger());
  commands::result res;
  ASSERT_TRUE(conv.sync_build_result_from_metrics(received, res));
  ASSERT_EQ(res.command_id, 1);
  ASSERT_EQ(res.start_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.end_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(res.output,
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;;0 rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
}
