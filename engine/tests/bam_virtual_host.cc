/**
 * Copyright 2026 Centreon
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

#include <gtest/gtest.h>

#include "cbmod_test.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using com::centreon::broker::neb::cbmod_test;

/* host::find_host() and service::find_service() resolve the BAM virtual host
 * '_Module_BAM_<id>' with either the Broker uid (cbm->poller_id()) or the
 * 'poller_id' key (cbm->original_poller_id()). */
class bam_virtual_host_test : public ::testing::Test {
 protected:
  cbmod_test* _cbm = nullptr;

 public:
  void SetUp() override {
    init_config_state();
    _cbm = dynamic_cast<cbmod_test*>(cbm.get());
    ASSERT_NE(_cbm, nullptr);
    _cbm->poller_id_value = 1001;
    _cbm->original_poller_id_value = 1;
  }

  void TearDown() override {
    if (_cbm) {
      _cbm->poller_id_value = 1;
      _cbm->original_poller_id_value = 1;
    }
    downtimes::downtime_manager::instance().clear_scheduled_downtimes();
    deinit_config_state();
    events::loop::instance().clear();
  }

  /* Creates the host 'host_name' with the service 'ba_1'. */
  void create_bam_host(const std::string& host_name) {
    configuration::applier::command cmd_aply;
    configuration::applier::host hst_aply;
    configuration::applier::service svc_aply;

    configuration::Command cmd;
    configuration::command_helper cmd_hlp(&cmd);
    cmd.set_command_name("cmd");
    cmd.set_command_line("/usr/bin/echo 1");
    cmd_aply.add_object(cmd);

    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst.set_host_name(host_name);
    hst.set_address("127.0.0.1");
    hst.set_host_id(12);
    hst.set_check_command("cmd");
    hst_hlp.set_default_values();
    hst_aply.add_object(hst);

    configuration::Service svc;
    configuration::service_helper svc_hlp(&svc);
    svc.set_host_name(host_name);
    svc.set_service_description("ba_1");
    svc.set_service_id(25);
    svc.set_host_id(12);
    svc.set_check_command("cmd");
    svc_hlp.set_default_values();
    svc_aply.add_object(svc);
  }

  /* Schedules a downtime on the host 12, or on its service 25 when
   * 'on_service' is set, starting in one hour. */
  void schedule_bam_downtime(bool on_service) {
    time_t now = time(nullptr);
    uint64_t downtime_id;
    ASSERT_EQ(downtimes::downtime_manager::instance().schedule_downtime(
                  on_service ? downtimes::downtime::service_downtime
                             : downtimes::downtime::host_downtime,
                  12, on_service ? 25 : 0, now, "admin", "bam", now + 3600,
                  now + 7200, true, 0, 3600, &downtime_id),
              OK);
  }
};

TEST_F(bam_virtual_host_test, exact_name) {
  create_bam_host("_Module_BAM_1");
  std::shared_ptr<host> hst = host::find_host("_Module_BAM_1");
  ASSERT_TRUE(hst);
  ASSERT_EQ(hst->name(), "_Module_BAM_1");
  ASSERT_TRUE(service::find_service("_Module_BAM_1", "ba_1"));
}

TEST_F(bam_virtual_host_test, uid_name_finds_poller_id_host) {
  create_bam_host("_Module_BAM_1");
  std::shared_ptr<host> hst = host::find_host("_Module_BAM_1001");
  ASSERT_TRUE(hst);
  ASSERT_EQ(hst->name(), "_Module_BAM_1");
  std::shared_ptr<service> svc =
      service::find_service("_Module_BAM_1001", "ba_1");
  ASSERT_TRUE(svc);
  ASSERT_EQ(svc->get_hostname(), "_Module_BAM_1");
}

TEST_F(bam_virtual_host_test, poller_id_name_finds_uid_host) {
  create_bam_host("_Module_BAM_1001");
  std::shared_ptr<host> hst = host::find_host("_Module_BAM_1");
  ASSERT_TRUE(hst);
  ASSERT_EQ(hst->name(), "_Module_BAM_1001");
  std::shared_ptr<service> svc = service::find_service("_Module_BAM_1", "ba_1");
  ASSERT_TRUE(svc);
  ASSERT_EQ(svc->get_hostname(), "_Module_BAM_1001");
}

TEST_F(bam_virtual_host_test, other_names_are_not_resolved) {
  create_bam_host("_Module_BAM_1");
  ASSERT_FALSE(host::find_host("_Module_BAM_7"));
  ASSERT_FALSE(host::find_host("_Module_BAM_x"));
  ASSERT_FALSE(host::find_host("_Module_BAM_"));
  ASSERT_FALSE(host::find_host("host_1001"));
  ASSERT_FALSE(service::find_service("_Module_BAM_7", "ba_1"));
  ASSERT_FALSE(service::find_service("_Module_BAM_1001", "ba_2"));
}

TEST_F(bam_virtual_host_test, no_cbmod_no_fallback) {
  create_bam_host("_Module_BAM_1");
  auto saved = std::move(cbm);
  std::shared_ptr<host> hst = host::find_host("_Module_BAM_1001");
  std::shared_ptr<service> svc =
      service::find_service("_Module_BAM_1001", "ba_1");
  bool exact_found = static_cast<bool>(host::find_host("_Module_BAM_1"));
  cbm = std::move(saved);
  ASSERT_FALSE(hst);
  ASSERT_FALSE(svc);
  ASSERT_TRUE(exact_found);
}

TEST_F(bam_virtual_host_test, delete_service_downtime_by_uid_name) {
  create_bam_host("_Module_BAM_1");
  schedule_bam_downtime(true);
  ASSERT_EQ(
      downtimes::downtime_manager::instance().get_scheduled_downtimes().size(),
      1u);
  ASSERT_EQ(
      downtimes::downtime_manager::instance()
          .delete_downtime_by_hostname_service_description_start_time_comment(
              "_Module_BAM_1001", "ba_1", {false, 0}, ""),
      1);
  ASSERT_TRUE(downtimes::downtime_manager::instance()
                  .get_scheduled_downtimes()
                  .empty());
}

TEST_F(bam_virtual_host_test, delete_host_downtime_by_poller_id_name) {
  create_bam_host("_Module_BAM_1001");
  schedule_bam_downtime(false);
  ASSERT_EQ(
      downtimes::downtime_manager::instance()
          .delete_downtime_by_hostname_service_description_start_time_comment(
              "_Module_BAM_1", "", {false, 0}, ""),
      1);
  ASSERT_TRUE(downtimes::downtime_manager::instance()
                  .get_scheduled_downtimes()
                  .empty());
}

TEST_F(bam_virtual_host_test, delete_downtime_other_name_keeps_it) {
  create_bam_host("_Module_BAM_1");
  schedule_bam_downtime(true);
  ASSERT_EQ(
      downtimes::downtime_manager::instance()
          .delete_downtime_by_hostname_service_description_start_time_comment(
              "_Module_BAM_7", "ba_1", {false, 0}, ""),
      0);
  ASSERT_EQ(
      downtimes::downtime_manager::instance().get_scheduled_downtimes().size(),
      1u);
}
