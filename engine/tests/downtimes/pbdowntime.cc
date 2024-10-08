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

#include <gtest/gtest.h>

#include "../timeperiod/utils.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

class DowntimeExternalCommand : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override {
    downtime_manager::instance().clear_scheduled_downtimes();
    deinit_config_state();
  }
};

TEST_F(DowntimeExternalCommand, AddUnkownHostDowntime) {
  set_time(20000);

  time_t now = time(nullptr);

  std::stringstream s;
  s << "SCHEDULE_HOST_DOWNTIME;test_srv;" << now << ";" << now
    << ";1;0;7200;admin;host";

  ASSERT_EQ(cmd_schedule_downtime(CMD_SCHEDULE_HOST_DOWNTIME, now,
                                  const_cast<char*>(s.str().c_str())),
            ERROR);

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}

TEST_F(DowntimeExternalCommand, AddHostDowntime) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_srv");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));

  set_time(20000);

  time_t now = time(nullptr);

  std::string query{
      fmt::format("test_srv;{};{};1;0;1;admin;host", now, now + 1)};

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  ASSERT_EQ(cmd_schedule_downtime(CMD_SCHEDULE_HOST_DOWNTIME, now,
                                  const_cast<char*>(query.c_str())),
            OK);

  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
  ASSERT_EQ(
      downtime_manager::instance().get_scheduled_downtimes().begin()->first,
      20000);
  ASSERT_EQ(downtime_manager::instance()
                .get_scheduled_downtimes()
                .begin()
                ->second->host_id(),
            1);
  ASSERT_EQ(downtime_manager::instance()
                .get_scheduled_downtimes()
                .begin()
                ->second->get_duration(),
            1);
  ASSERT_EQ(downtime_manager::instance()
                .get_scheduled_downtimes()
                .begin()
                ->second->get_end_time(),
            20001);
  ASSERT_EQ(downtime_manager::instance()
                .get_scheduled_downtimes()
                .begin()
                ->second->handle(),
            OK);
  set_time(20001);
  ASSERT_EQ(downtime_manager::instance()
                .get_scheduled_downtimes()
                .begin()
                ->second->handle(),
            OK);
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}
