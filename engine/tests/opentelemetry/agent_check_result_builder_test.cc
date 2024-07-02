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

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_check_result_builder.hh"

#include "helper.hh"
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
  std::shared_ptr<check_result_builder_config> _builder_config;
  data_point_fifo_container _fifos;

 public:
  otl_agent_check_result_builder_test() {
    if (service::services.find({"test_host", "test_svc_builder_2"}) ==
        service::services.end()) {
      init_config_state();
      config->contacts().clear();
      configuration::error_cnt err;

      configuration::applier::contact ct_aply;
      configuration::contact ctct{new_configuration_contact("admin", true)};
      ct_aply.add_object(ctct);
      ct_aply.expand_objects(*config);
      ct_aply.resolve_object(ctct, err);

      configuration::host hst{
          new_configuration_host("test_host", "admin", 457)};
      configuration::applier::host hst_aply;
      hst_aply.add_object(hst);

      configuration::service svc{new_configuration_service(
          "test_host", "test_svc_builder", "admin", 458)};
      configuration::applier::service svc_aply;
      svc_aply.add_object(svc);
      configuration::service svc2{new_configuration_service(
          "test_host", "test_svc_builder_2", "admin", 459)};
      svc_aply.add_object(svc2);

      hst_aply.resolve_object(hst, err);
      svc_aply.resolve_object(svc, err);
      svc_aply.resolve_object(svc2, err);
    }

    _builder_config =
        otl_check_result_builder::create_check_result_builder_config(
            "--processor=centreon_agent");

    metric_request_ptr request =
        std::make_shared< ::opentelemetry::proto::collector::metrics::v1::
                              ExportMetricsServiceRequest>();

    ::google::protobuf::util::JsonStringToMessage(agent_exemple, request.get());

    otl_data_point::extract_data_points(
        request, [&](const otl_data_point& data_pt) {
          std::string service_name;
          for (const auto attrib : data_pt.get_resource().attributes()) {
            if (attrib.key() == "service.name") {
              service_name = attrib.value().string_value();
              break;
            }
          }
          _fifos.add_data_point("test_host", service_name,
                                data_pt.get_metric().name(), data_pt);
        });
  }
};

TEST_F(otl_agent_check_result_builder_test, test_svc_builder) {
  auto check_result_builder = otl_check_result_builder::create(
      "", _builder_config, 1789, *host::hosts.find("test_host")->second,
      service::services.find({"test_host", "test_svc_builder"})->second.get(),
      std::chrono::system_clock::time_point(), [&](const commands::result&) {},
      spdlog::default_logger());

  commands::result res;
  bool success =
      check_result_builder->sync_build_result_from_metrics(_fifos, res);

  ASSERT_TRUE(success);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(res.command_id, 1789);
  ASSERT_EQ(res.start_time.to_useconds(), 1718345061381922153 / 1000);
  ASSERT_EQ(res.end_time.to_useconds(), 1718345061381922153 / 1000);

  auto compare_to_excepted = [](const std::string& to_cmp) -> bool {
    return to_cmp ==
               "output of plugin| metric=12;0:50;0:75;; "
               "metric2=30ms;50:75;75:80;0;100" ||
           to_cmp ==
               "output of plugin| metric2=30ms;50:75;75:80;0;100 "
               "metric=12;0:50;0:75;;";
  };

  ASSERT_PRED1(compare_to_excepted, res.output);
}

TEST_F(otl_agent_check_result_builder_test, test_svc_builder_2) {
  auto check_result_builder = otl_check_result_builder::create(
      "", _builder_config, 1789, *host::hosts.find("test_host")->second,
      service::services.find({"test_host", "test_svc_builder_2"})->second.get(),
      std::chrono::system_clock::time_point(), [&](const commands::result&) {},
      spdlog::default_logger());

  commands::result res;
  bool success =
      check_result_builder->sync_build_result_from_metrics(_fifos, res);

  ASSERT_TRUE(success);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(res.command_id, 1789);
  ASSERT_EQ(res.start_time.to_useconds(), 1718345061713456225 / 1000);
  ASSERT_EQ(res.end_time.to_useconds(), 1718345061713456225 / 1000);

  auto compare_to_excepted = [](const std::string& to_cmp) -> bool {
    return to_cmp ==
               "output taratata| metric=12;@0:50;@~:75;; "
               "metric2=30ms;~:75;75:80;0;100" ||
           to_cmp ==
               "output taratata| metric2=30ms;~:75;75:80;0;100 "
               "metric=12;@0:50;@~:75;;";
  };

  ASSERT_PRED1(compare_to_excepted, res.output);
}