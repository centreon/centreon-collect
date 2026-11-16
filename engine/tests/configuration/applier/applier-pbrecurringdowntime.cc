/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include <gtest/gtest.h>

#include "../../timeperiod/utils.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/recurringdowntime.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/recurring_downtime.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "helper.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using com::centreon::engine::downtimes::downtime;
using com::centreon::engine::downtimes::downtime_manager;
using com::centreon::engine::downtimes::recurring_downtime;
using com::centreon::engine::downtimes::service_downtime;

extern configuration::State pb_config;

namespace {
// Fabricated instants, Europe/Paris local time (the test process timezone).
constexpr time_t mon_2030_01_07_2155 = 1894049700;
constexpr time_t mon_2030_01_07_2200 = 1894050000;
constexpr time_t mon_2030_01_07_2210 = 1894050600;
constexpr time_t mon_2030_01_07_2330 = 1894055400;
}  // namespace

class recurringdowntime_applier_test : public ::testing::Test {
 protected:
  error_cnt _err;
  time_t _saved_program_start;

  void _add_host(uint64_t host_id, const std::string& name) {
    Host hst;
    host_helper hst_hlp(&hst);
    hst.set_host_name(name);
    hst.set_address("127.0.0.1");
    hst.set_host_id(host_id);
    hst.set_check_command("cmd");
    hst_hlp.set_default_values();
    applier::host().add_object(hst);
  }

  void _add_service(uint64_t host_id,
                    const std::string& host_name,
                    uint64_t service_id) {
    Service svc;
    service_helper svc_hlp(&svc);
    svc.set_host_name(host_name);
    svc.set_host_id(host_id);
    svc.set_service_description(fmt::format("svc_{}", service_id));
    svc.set_service_id(service_id);
    svc.set_check_command("cmd");
    svc_hlp.set_default_values();
    applier::service().add_object(svc);
  }

  Recurringdowntime _rule(uint64_t id,
                          std::string_view host_ids,
                          std::string_view service_ids,
                          std::string_view period) {
    Recurringdowntime rd;
    recurringdowntime_helper hlp(&rd);
    rd.set_downtime_id(id);
    rd.set_timezone(":Europe/Paris");
    if (!host_ids.empty()) {
      EXPECT_TRUE(hlp.hook("host_ids", host_ids));
    }
    if (!service_ids.empty()) {
      EXPECT_TRUE(hlp.hook("service_ids", service_ids));
    }
    EXPECT_TRUE(hlp.hook("period", period));
    return rd;
  }

  void _add_rule(const Recurringdowntime& rd) {
    applier::recurringdowntime aplyr;
    aplyr.add_object(rd);
    aplyr.resolve_object(rd, _err);
  }

  static size_t _downtimes() {
    return downtime_manager::instance().get_scheduled_downtimes().size();
  }

 public:
  void SetUp() override {
    init_config_state();
    // Engine started an hour before the fabricated "now".
    _saved_program_start = program_start;
    program_start = mon_2030_01_07_2155 - 3600;
    Command cmd;
    command_helper cmd_hlp(&cmd);
    cmd.set_command_name("cmd");
    cmd.set_command_line("/usr/bin/echo 1");
    applier::command().add_object(cmd);

    _add_host(1, "host_1");
    _add_host(2, "host_2");
    _add_service(1, "host_1", 3);
    _add_service(1, "host_1", 4);
    _add_service(2, "host_2", 5);
    _add_service(2, "host_2", 6);
    applier::host().expand_objects(pb_config);
    applier::service().expand_objects(pb_config);
    for (const auto& h : pb_config.hosts())
      applier::host().resolve_object(h, _err);
    for (const auto& s : pb_config.services())
      applier::service().resolve_object(s, _err);
    // Only count what the recurring downtimes report.
    _err = error_cnt();
    set_time(mon_2030_01_07_2155);
  }

  void TearDown() override {
    downtime_manager::instance().clear_scheduled_downtimes();
    events::loop::instance().clear();
    deinit_config_state();
    program_start = _saved_program_start;
  }

  static uint64_t _host_downtime_id() {
    for (const auto& [start, dt] :
         downtime_manager::instance().get_scheduled_downtimes())
      if (dt->get_type() == downtime::host_downtime)
        return dt->get_downtime_id();
    return 0;
  }
};

// A host target gets a host downtime and one per service; a service target
// gets one service downtime; all with the cron author and comment.
TEST_F(recurringdowntime_applier_test, host_and_service_downtimes_scheduled) {
  _add_rule(_rule(3, "1", "2:5", "weekly:1 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);

  const auto& dts = downtime_manager::instance().get_scheduled_downtimes();
  ASSERT_EQ(dts.size(), 4u);
  std::set<std::pair<uint64_t, uint64_t>> targets;
  for (const auto& [start, dt] : dts) {
    EXPECT_EQ(start, mon_2030_01_07_2200);
    EXPECT_EQ(dt->get_end_time(), mon_2030_01_07_2330);
    EXPECT_TRUE(dt->is_fixed());
    EXPECT_EQ(dt->get_duration(), mon_2030_01_07_2330 - mon_2030_01_07_2200);
    EXPECT_EQ(dt->get_author(), "Downtime cycle");
    EXPECT_EQ(dt->get_comment(), "[Downtime cycle #3]");
    if (dt->get_type() == downtime::host_downtime)
      targets.emplace(dt->host_id(), 0);
    else
      targets.emplace(dt->host_id(),
                      static_cast<service_downtime*>(dt.get())->service_id());
  }
  std::set<std::pair<uint64_t, uint64_t>> expected{
      {1, 0}, {1, 3}, {1, 4}, {2, 5}};
  EXPECT_EQ(targets, expected);
}

TEST_F(recurringdowntime_applier_test, no_duplicate_on_next_runs) {
  _add_rule(_rule(3, "1", "2:5", "weekly:1 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  ASSERT_EQ(_downtimes(), 4u);
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  recurring_downtime::schedule_all(mon_2030_01_07_2155 + 60);
  EXPECT_EQ(_downtimes(), 4u);
}

TEST_F(recurringdowntime_applier_test, nothing_outside_the_window) {
  _add_rule(_rule(3, "1", "", "weekly:2 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  EXPECT_EQ(_downtimes(), 0u);
}

TEST_F(recurringdowntime_applier_test, flexible_duration_kept) {
  _add_rule(_rule(3, "", "1:3", "weekly:1 22:00-23:30 flexible:1800"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  const auto& dts = downtime_manager::instance().get_scheduled_downtimes();
  ASSERT_EQ(dts.size(), 1u);
  EXPECT_FALSE(dts.begin()->second->is_fixed());
  EXPECT_EQ(dts.begin()->second->get_duration(), 1800);
}

// SCHEDULE_*_DOWNTIME refuses a flexible downtime without duration.
TEST_F(recurringdowntime_applier_test, flexible_without_duration_not_scheduled) {
  _add_rule(_rule(3, "", "1:3", "weekly:1 22:00-23:30 flexible:0"));
  EXPECT_EQ(_err.config_warnings, 1u);
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  EXPECT_EQ(_downtimes(), 0u);
}

TEST_F(recurringdowntime_applier_test, unknown_targets_are_warnings) {
  _add_rule(_rule(3, "1,99", "2:99,1:3", "weekly:1 22:00-23:30 fixed"));
  EXPECT_EQ(_err.config_warnings, 2u);
  EXPECT_EQ(_err.config_errors, 0u);
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  // Host 1 and its 2 services; the service target 1:3 is already covered.
  EXPECT_EQ(_downtimes(), 3u);
}

TEST_F(recurringdowntime_applier_test, duplicate_rule_id_refused) {
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  applier::recurringdowntime aplyr;
  EXPECT_THROW(aplyr.add_object(_rule(3, "2", "", "weekly:1 22:00-23:30 fixed")),
               std::exception);
}

TEST_F(recurringdowntime_applier_test, removal_deletes_future_downtimes) {
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  _add_rule(_rule(4, "2", "", "weekly:1 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  ASSERT_EQ(_downtimes(), 6u);

  ASSERT_EQ(pb_config.recurringdowntimes(0).downtime_id(), 3u);
  applier::recurringdowntime().remove_object(0);
  EXPECT_EQ(recurring_downtime::recurring_downtimes.count(3), 0u);
  ASSERT_EQ(pb_config.recurringdowntimes_size(), 1);
  // Only the downtimes of rule 4 remain.
  EXPECT_EQ(_downtimes(), 3u);
  for (const auto& [start, dt] : downtime_manager::instance()
                                     .get_scheduled_downtimes())
    EXPECT_EQ(dt->get_comment(), "[Downtime cycle #4]");
}

TEST_F(recurringdowntime_applier_test, removal_keeps_started_downtimes) {
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  set_time(mon_2030_01_07_2210);
  recurring_downtime::schedule_all(mon_2030_01_07_2210);
  ASSERT_EQ(_downtimes(), 3u);
  applier::recurringdowntime().remove_object(0);
  EXPECT_EQ(_downtimes(), 3u);
}

TEST_F(recurringdowntime_applier_test, modification_replaces_future_downtimes) {
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  ASSERT_EQ(_downtimes(), 3u);

  Recurringdowntime changed = _rule(3, "1", "", "weekly:1 22:05-23:30 fixed");
  applier::recurringdowntime aplyr;
  aplyr.modify_object(pb_config.mutable_recurringdowntimes(0), changed);
  aplyr.resolve_object(pb_config.recurringdowntimes(0), _err);
  EXPECT_EQ(_downtimes(), 0u);

  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  const auto& dts = downtime_manager::instance().get_scheduled_downtimes();
  ASSERT_EQ(dts.size(), 3u);
  EXPECT_EQ(dts.begin()->first, mon_2030_01_07_2200 + 300);
}

TEST_F(recurringdowntime_applier_test, cancelled_downtime_not_recreated) {
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  ASSERT_EQ(_downtimes(), 3u);

  uint64_t host_dt = _host_downtime_id();
  ASSERT_NE(host_dt, 0u);
  ASSERT_EQ(downtime_manager::instance().unschedule_downtime(host_dt), OK);
  ASSERT_EQ(_downtimes(), 2u);

  recurring_downtime::schedule_all(mon_2030_01_07_2155 + 60);
  set_time(mon_2030_01_07_2210);
  recurring_downtime::schedule_all(mon_2030_01_07_2210);
  EXPECT_EQ(_downtimes(), 2u);
  EXPECT_EQ(_host_downtime_id(), 0u);
}

TEST_F(recurringdowntime_applier_test,
       occurrence_started_before_engine_not_scheduled) {
  program_start = mon_2030_01_07_2200 + 300;
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  EXPECT_EQ(recurring_downtime::recurring_downtimes.at(3)->not_before(),
            program_start);
  set_time(mon_2030_01_07_2210);
  recurring_downtime::schedule_all(mon_2030_01_07_2210);
  EXPECT_EQ(_downtimes(), 0u);
}

TEST_F(recurringdowntime_applier_test, rule_added_by_reload_takes_effect_now) {
  program_start = mon_2030_01_07_2200 + 300;
  // The first configuration load is over: the rule comes from a reload.
  recurring_downtime::set_initial_load_done();
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  EXPECT_EQ(recurring_downtime::recurring_downtimes.at(3)->not_before(), 0);
  set_time(mon_2030_01_07_2210);
  recurring_downtime::schedule_all(mon_2030_01_07_2210);
  const auto& dts = downtime_manager::instance().get_scheduled_downtimes();
  ASSERT_EQ(dts.size(), 3u);
  EXPECT_EQ(dts.begin()->first, mon_2030_01_07_2200);
}

TEST_F(recurringdowntime_applier_test, modified_rule_forgets_handled_occurrences) {
  _add_rule(_rule(3, "1", "", "weekly:1 22:00-23:30 fixed"));
  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  ASSERT_EQ(downtime_manager::instance().unschedule_downtime(
                _host_downtime_id()),
            OK);
  ASSERT_EQ(_downtimes(), 2u);

  Recurringdowntime changed = _rule(3, "1", "", "weekly:1 22:00-23:30 fixed");
  changed.set_downtime_name("renamed");
  applier::recurringdowntime aplyr;
  aplyr.modify_object(pb_config.mutable_recurringdowntimes(0), changed);
  aplyr.resolve_object(pb_config.recurringdowntimes(0), _err);
  EXPECT_EQ(_downtimes(), 0u);

  recurring_downtime::schedule_all(mon_2030_01_07_2155);
  EXPECT_EQ(_downtimes(), 3u);
}
