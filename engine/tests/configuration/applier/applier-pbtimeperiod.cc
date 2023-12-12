/**
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "common/configuration/timeperiod_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierTimeperiod : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given a timeperiod applier
// And a timeperiod configuration just with a name.
// Then the applier add_object() throws an error because of the missing alias.
TEST_F(ApplierTimeperiod, AddTimeperiodName) {
  configuration::applier::timeperiod aply;
  configuration::timeperiod tp;
  tp.parse("timeperiod_name", "timeperiod");
  ASSERT_THROW(aply.add_object(tp), std::exception);
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name.
// Then the applier add_object() throws an error because of the missing alias.
TEST_F(ApplierTimeperiod, PbAddTimeperiodName) {
  configuration::applier::timeperiod aply;
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_timeperiod_name("timeperiod");
  ASSERT_THROW(aply.add_object(tp), std::exception);
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name.
// Then the applier add_object() throws an error because of the missing alias.
TEST_F(ApplierTimeperiod, AddTimeperiodNameAlias) {
  configuration::applier::timeperiod aply;
  configuration::timeperiod tp;
  tp.parse("timeperiod_name", "timeperiod");
  tp.parse("alias", "timeperiod_alias");
  ASSERT_NO_THROW(aply.add_object(tp));
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name.
// Then the applier add_object() throws an error because of the missing alias.
TEST_F(ApplierTimeperiod, PbAddTimeperiodNameAlias) {
  configuration::applier::timeperiod aply;
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_timeperiod_name("timeperiod");
  tp.set_alias("timeperiod_alias");
  ASSERT_NO_THROW(aply.add_object(tp));
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name, an id and a type.
// Then the applier add_object adds the timeperiod in the configuration set
// and in the timeperiods map.
TEST_F(ApplierTimeperiod, AddTimeperiodFromConfig) {
  configuration::applier::timeperiod aply;
  configuration::timeperiod tp;
  tp.parse("timeperiod_name", "timeperiod");
  tp.parse("alias", "timeperiod_alias");
  tp.parse("january 1\t00:00-24:00");
  tp.parse("november 11\t00:00-24:00");
  aply.add_object(tp);
  const set_timeperiod& s = config->timeperiods();
  ASSERT_EQ(s.size(), 1u);
  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);
  ASSERT_NO_THROW(aply.expand_objects(*config));
  ASSERT_NO_THROW(aply.resolve_object(tp));
  ASSERT_EQ(tp.exceptions().size(), 5U);
  ASSERT_EQ(tp.exceptions()[1].size(), 2U);
  std::ostringstream oss;
  oss << tp.exceptions()[1].front();
  ASSERT_EQ(oss.str(), std::string_view("november 11 00:00-24:00"));
  oss.str("");
  oss << tp.exceptions()[1].back();
  ASSERT_EQ(oss.str(), std::string_view("january 1 00:00-24:00"));
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name, an id and a type.
// Then the applier add_object adds the timeperiod in the configuration set
// and in the timeperiods map.
TEST_F(ApplierTimeperiod, PbAddTimeperiodFromConfig) {
  configuration::applier::timeperiod aply;
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_alias("timeperiod_alias");
  tp.set_timeperiod_name("timeperiod");
  tp_hlp.hook("january 1", "00:00-24:00");
  tp_hlp.hook("november 11", "00:00-24:00");
  aply.add_object(tp);
  const auto& s = pb_config.timeperiods();
  ASSERT_EQ(s.size(), 1u);
  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);
  ASSERT_NO_THROW(aply.expand_objects(pb_config));
  ASSERT_NO_THROW(aply.resolve_object(tp));
  ASSERT_EQ(tp.exceptions().month_date_size(), 2U);
  auto d = tp.exceptions().month_date().at(1);
  ASSERT_EQ(daterange_to_str(d), std::string_view("november 11 00:00-24:00"));
  d = tp.exceptions().month_date().at(0);
  ASSERT_EQ(daterange_to_str(d), std::string_view("january 1 00:00-24:00"));
}

// Given a timeperiod applier
// And a timeperiod configuration is added
// Then it is modified
// Then the real object is well modified.
TEST_F(ApplierTimeperiod, ModifyTimeperiodFromConfig) {
  configuration::applier::timeperiod aply;
  configuration::timeperiod tp;
  tp.parse("timeperiod_name", "timeperiod");
  tp.parse("alias", "timeperiod_alias");
  aply.add_object(tp);

  const set_timeperiod& s = config->timeperiods();
  ASSERT_EQ(s.size(), 1u);

  tp.parse("alias", "timeperiod_alias1");
  aply.modify_object(tp);

  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);
  ASSERT_EQ(engine::timeperiod::timeperiods.begin()->second->get_alias(),
            std::string_view("timeperiod_alias1"));
}

// Given a timeperiod applier
// And a timeperiod configuration is added
// Then it is modified
// Then the real object is well modified.
// Then it is modified by doing nothing
// Then it is not modified at all.
TEST_F(ApplierTimeperiod, PbModifyTimeperiodFromConfig) {
  configuration::applier::timeperiod aply;
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_alias("timeperiod_alias");
  tp.set_timeperiod_name("timeperiod");
  aply.add_object(tp);

  const auto& s = pb_config.timeperiods();
  ASSERT_EQ(s.size(), 1u);

  tp.set_alias("timeperiod_alias1");

  aply.modify_object(&pb_config.mutable_timeperiods()->at(0), tp);

  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);
  ASSERT_EQ(engine::timeperiod::timeperiods.begin()->second->get_alias(),
            std::string_view("timeperiod_alias1"));

  // No change here
  aply.modify_object(&pb_config.mutable_timeperiods()->at(0), tp);
  ASSERT_EQ(engine::timeperiod::timeperiods.begin()->second->get_alias(),
            std::string_view("timeperiod_alias1"));
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name, an id and a type.
// This configuration is added and then removed.
// There is no more timeperiod.
TEST_F(ApplierTimeperiod, RemoveTimeperiodFromConfig) {
  configuration::applier::timeperiod aply;
  configuration::timeperiod tp;
  tp.parse("alias", "timeperiod_alias");
  tp.parse("timeperiod_name", "timeperiod");
  aply.add_object(tp);
  const set_timeperiod& s = config->timeperiods();
  ASSERT_EQ(s.size(), 1u);

  aply.remove_object(tp);
  ASSERT_TRUE(s.empty());
}

// Given a timeperiod applier
// And a timeperiod configuration just with a name, an id and a type.
// This configuration is added and then removed.
// There is no more timeperiod.
TEST_F(ApplierTimeperiod, PbRemoveTimeperiodFromConfig) {
  configuration::applier::timeperiod aply;
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_alias("timeperiod_alias");
  tp.set_timeperiod_name("timeperiod");
  aply.add_object(tp);
  const auto& s = pb_config.timeperiods();
  ASSERT_EQ(s.size(), 1u);

  aply.remove_object(0);
  ASSERT_TRUE(s.empty());
}
