/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/anomalydetection.hh"

#include <gtest/gtest.h>

#include "../test_engine.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "com/centreon/engine/version.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class ADExtCmd : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();

    configuration::error_cnt err;
    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_indexed_config);
    ct_aply.resolve_object(ctct, err);

    configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::Service svc{
        new_pb_configuration_service("test_host", "test_svc", "admin")};
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    hst_aply.resolve_object(hst, err);
    svc_aply.resolve_object(svc, err);

    configuration::Anomalydetection ad{new_pb_configuration_anomalydetection(
        "test_host", "test_ad", "admin",
        12,  // service_id of the anomalydetection
        13,  // service_id of the dependent service
        "/tmp/thresholds_status_change.json")};
    configuration::applier::anomalydetection ad_aply;
    ad_aply.add_object(ad);

    ad_aply.resolve_object(ad, err);

    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));

    service_map const& sm{engine::service::services};
    for (auto& p : sm) {
      std::shared_ptr<engine::service> svc = p.second;
      if (svc->service_id() == 12)
        _ad = std::static_pointer_cast<engine::anomalydetection>(svc);
      else
        _svc = svc;
    }
  }

  void TearDown() override {
    _host.reset();
    _svc.reset();
    _ad.reset();
    deinit_config_state();
  }

  std::list<std::string> execute(const std::string& command) {
    std::list<std::string> retval;
    char path[1024];
    std::ostringstream oss;
    oss << "tests/rpc_client " << command;

    FILE* fp = popen(oss.str().c_str(), "r");
    while (fgets(path, sizeof(path), fp) != nullptr) {
      size_t count = strlen(path);
      if (count > 0)
        --count;
      retval.push_back(std::string(path, count));
    }
    pclose(fp);
    return retval;
  }

  void CreateFile(std::string const& filename, std::string const& content) {
    std::ofstream oss(filename);
    oss << content;
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
  std::shared_ptr<engine::anomalydetection> _ad;
};

TEST_F(ADExtCmd, NewThresholdsFile) {
  CreateFile(
      "/tmp/thresholds_file.json",
      "[{\n \"host_id\": \"12\",\n \"service_id\": \"12\",\n \"metric_name\": "
      "\"metric\",\n \"predict\": [{\n \"timestamp\": 50000,\n \"upper\": "
      "84,\n \"lower\": 74,\n \"fit\": 79\n }, {\n \"timestamp\": 100000,\n "
      "\"upper\": 10,\n \"lower\": 5,\n \"fit\": 51.5\n }, {\n \"timestamp\": "
      "150000,\n \"upper\": 100,\n \"lower\": 93,\n \"fit\": 96.5\n }, {\n "
      "\"timestamp\": 200000,\n \"upper\": 100,\n \"lower\": 97,\n \"fit\": "
      "98.5\n }, {\n \"timestamp\": 250000,\n \"upper\": 100,\n \"lower\": "
      "21,\n \"fit\": 60.5\n }\n]}]");
  std::ostringstream oss;
  oss << '[' << std::time(nullptr) << ']'
      << " NEW_THRESHOLDS_FILE;/tmp/thresholds_file.json";
  process_external_command(oss.str().c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_thresholds_file(), "/tmp/thresholds_file.json");
}
