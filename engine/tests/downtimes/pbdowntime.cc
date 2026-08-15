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

#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "common/downtimes/downtime_manager.hh"
#include "common/tests/timeperiods/utils.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
using namespace com::centreon::common::downtimes;

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
  ASSERT_TRUE(downtime_manager::instance()
                  .get_scheduled_downtimes()
                  .begin()
                  ->second->handle());
  set_time(20001);
  ASSERT_TRUE(downtime_manager::instance()
                  .get_scheduled_downtimes()
                  .begin()
                  ->second->handle());
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief Helper scheduling one host downtime on test_srv, used by the deletion
 * tests below.
 *
 * @return The time the downtime starts at.
 */
static time_t schedule_one_host_downtime() {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_srv");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  hst_aply.add_object(hst);

  set_time(20000);
  time_t now = time(nullptr);
  std::string query{
      fmt::format("test_srv;{};{};1;0;3600;admin;host", now, now + 3600)};
  cmd_schedule_downtime(CMD_SCHEDULE_HOST_DOWNTIME, now,
                        const_cast<char*>(query.c_str()));
  return now;
}

/**
 * @brief DEL_DOWNTIME_BY_HOST_NAME with only its mandatory field. The optional
 * service description, start time and comment used to be read as char* left at
 * nullptr, then handed over to parameters taking a std::string const&, which is
 * undefined behaviour. They are empty strings now, which the downtime manager
 * documents as "no filter on that criterion".
 */
TEST_F(DowntimeExternalCommand, DeleteDowntimeByHostNameAloneDeletesIt) {
  schedule_one_host_downtime();
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());

  std::string query{"test_srv"};
  ASSERT_EQ(cmd_delete_downtime_by_host_name(CMD_DEL_DOWNTIME_BY_HOST_NAME,
                                             const_cast<char*>(query.c_str())),
            OK);
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief Same command with every optional field left empty: an absent field and
 * an empty one must behave the same way.
 */
TEST_F(DowntimeExternalCommand,
       DeleteDowntimeByHostNameWithEmptyOptionalFields) {
  schedule_one_host_downtime();
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());

  std::string query{"test_srv;;;"};
  ASSERT_EQ(cmd_delete_downtime_by_host_name(CMD_DEL_DOWNTIME_BY_HOST_NAME,
                                             const_cast<char*>(query.c_str())),
            OK);
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief The service description filter is honoured: a service description on a
 * host downtime matches nothing, so the downtime survives.
 */
TEST_F(DowntimeExternalCommand,
       DeleteDowntimeByHostNameWithServiceKeepsHostDowntime) {
  schedule_one_host_downtime();

  std::string query{"test_srv;any_service"};
  ASSERT_EQ(cmd_delete_downtime_by_host_name(CMD_DEL_DOWNTIME_BY_HOST_NAME,
                                             const_cast<char*>(query.c_str())),
            ERROR);
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief All the fields at once, to check the whole splitting: host, service,
 * start time and comment. The start time is the one the downtime was scheduled
 * at, the comment the one cmd_schedule_downtime() stored.
 */
TEST_F(DowntimeExternalCommand, DeleteDowntimeByHostNameWithAllFields) {
  time_t now = schedule_one_host_downtime();
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());

  std::string query{fmt::format("test_srv;;{};host", now)};
  ASSERT_EQ(cmd_delete_downtime_by_host_name(CMD_DEL_DOWNTIME_BY_HOST_NAME,
                                             const_cast<char*>(query.c_str())),
            OK);
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief A start time that does not match leaves the downtime in place: the
 * field is really parsed, not ignored.
 */
TEST_F(DowntimeExternalCommand, DeleteDowntimeByHostNameWithWrongStartTime) {
  time_t now = schedule_one_host_downtime();

  std::string query{fmt::format("test_srv;;{};", now + 1)};
  ASSERT_EQ(cmd_delete_downtime_by_host_name(CMD_DEL_DOWNTIME_BY_HOST_NAME,
                                             const_cast<char*>(query.c_str())),
            ERROR);
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief DEL_DOWNTIME_BY_START_TIME_COMMENT, whose start time is optional too.
 */
TEST_F(DowntimeExternalCommand, DeleteDowntimeByStartTimeComment) {
  time_t now = schedule_one_host_downtime();
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());

  std::string query{fmt::format("{};host", now)};
  ASSERT_EQ(cmd_delete_downtime_by_start_time_comment(
                CMD_DEL_DOWNTIME_BY_START_TIME_COMMENT,
                const_cast<char*>(query.c_str())),
            OK);
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
}

/**
 * @brief An unknown host group is rejected before anything is deleted.
 */
TEST_F(DowntimeExternalCommand, DeleteDowntimeByUnknownHostgroupName) {
  schedule_one_host_downtime();

  std::string query{"no_such_group;test_srv"};
  ASSERT_EQ(
      cmd_delete_downtime_by_hostgroup_name(CMD_DEL_DOWNTIME_BY_HOSTGROUP_NAME,
                                            const_cast<char*>(query.c_str())),
      ERROR);
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
}
