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

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "configuration/state.pb.h"

using namespace com::centreon::engine;

class ApplierState : public ::testing::Test {
 protected:
  configuration::State pb_config;

 public:
  void SetUp() override {
    auto tps = pb_config.mutable_timeperiods();
    for (int i = 0; i < 10; i++) {
      auto* tp = tps->Add();
      tp->set_alias(fmt::format("timeperiod {}", i));
      tp->set_name(fmt::format("Timeperiod {}", i));
    }
    for (int i = 0; i < 5; i++) {
      auto cts = pb_config.mutable_contacts();
      configuration::Contact ct;
      std::string name(fmt::format("name{:2}", i));
      ct.set_name(name);
      ct.set_alias(fmt::format("alias{:2}", i));
      for (int j = 0; j < 3; j++)
        ct.add_address(fmt::format("address{:2}", j));
      for (int j = 0; j < 10; j++) {
        configuration::CustomVariable* cv = ct.add_customvariables();
        cv->set_name(fmt::format("key_{}_{}", name, j));
        cv->set_value(fmt::format("value_{}_{}", name, j));
      }
      (*cts)[name] = std::move(ct);
    }
  }
};

using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;

TEST_F(ApplierState, DiffOnTimeperiod) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  EXPECT_TRUE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_TRUE(dstate.to_add().empty());
  ASSERT_TRUE(dstate.to_remove().empty());
  ASSERT_TRUE(dstate.to_modify().empty());
}

TEST_F(ApplierState, DiffOnTimeperiodOneRemoved) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  new_config.mutable_timeperiods()->RemoveLast();

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_EQ(dstate.to_remove().size(), 1u);
  ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 1);
  ASSERT_EQ(dstate.to_remove()[0].key()[1].i32(), 9);
  ASSERT_EQ(dstate.to_remove()[0].key().size(), 2);
  ASSERT_TRUE(dstate.to_add().empty());
  ASSERT_TRUE(dstate.to_modify().empty());
}

TEST_F(ApplierState, DiffOnTimeperiodNewOne) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto tps = new_config.mutable_timeperiods();
  auto* tp = tps->Add();
  tp->set_alias("timeperiod 11");
  tp->set_name("Timeperiod 11");

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_TRUE(dstate.to_remove().empty());
  ASSERT_TRUE(dstate.to_modify().empty());
  ASSERT_EQ(dstate.to_add().size(), 1u);
  ASSERT_TRUE(dstate.to_add()[0].val().has_value_tp());
  const configuration::Timeperiod& new_tp = dstate.to_add()[0].val().value_tp();
  ASSERT_EQ(new_tp.alias(), std::string("timeperiod 11"));
  ASSERT_EQ(new_tp.name(), std::string("Timeperiod 11"));
}

TEST_F(ApplierState, DiffOnTimeperiodAliasRenamed) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto tps = new_config.mutable_timeperiods();
  tps->at(7).set_alias("timeperiod changed");

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_TRUE(dstate.to_remove().empty());
  ASSERT_TRUE(dstate.to_add().empty());
  ASSERT_EQ(dstate.to_modify().size(), 1u);
  const configuration::PathWithValue& path = dstate.to_modify()[0];
  ASSERT_EQ(path.path().key().size(), 4u);
  // number 1 => timeperiods
  ASSERT_EQ(path.path().key()[0].i32(), 1);
  // index 7 => timeperiods[7]
  ASSERT_EQ(path.path().key()[1].i32(), 7);
  // number 1 => timeperiods.alias
  ASSERT_EQ(path.path().key()[2].i32(), 1);
  // No more key...
  ASSERT_EQ(path.path().key()[3].i32(), -1);
  ASSERT_TRUE(path.val().has_value_str());
  // The new value of timeperiods[7].alias is "timeperiod changed"
  ASSERT_EQ(path.val().value_str(), std::string("timeperiod changed"));
}

TEST_F(ApplierState, DiffOnContactOneRemoved) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto cts = new_config.mutable_contacts();
  cts->erase("name 4");

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_EQ(dstate.to_remove().size(), 1u);

  ASSERT_EQ(dstate.to_remove()[0].key().size(), 2);
  // number 4 => for contacts
  ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 4);
  // "name 4" => contacts["name 4"]
  ASSERT_EQ(dstate.to_remove()[0].key()[1].str(), std::string("name 4"));

  ASSERT_TRUE(dstate.to_add().empty());
  ASSERT_TRUE(dstate.to_modify().empty());
}

TEST_F(ApplierState, DiffOnContactOneAdded) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto cts = pb_config.mutable_contacts();
  cts->erase("name 4");

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_TRUE(dstate.to_remove().empty());
  ASSERT_TRUE(dstate.to_modify().empty());
  ASSERT_EQ(dstate.to_add().size(), 1u);
  const configuration::PathWithValue& to_add = dstate.to_add()[0];
  ASSERT_EQ(to_add.path().key().size(), 2u);
  ASSERT_EQ(to_add.path().key()[0].i32(), 4);
  // ASSERT_EQ(to_add.path().key()[1].str(), std::string("name 4"));
  ASSERT_TRUE(to_add.val().has_value_ct());
}

/**
 * @brief Contact "name 3" has a new address added. Addresses are stored in
 * an array. We don't have the information if an address is added or removed
 * so we send all the addresses in the difference. That's why the difference
 * tells about 4 addresses as difference.
 */
TEST_F(ApplierState, DiffOnContactOneNewAddress) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto cts = new_config.mutable_contacts();
  auto& ct = (*cts)["name 3"];
  ct.add_address("new address");

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  //  ASSERT_TRUE(dstate.dcontacts().to_add().empty());
  //  ASSERT_TRUE(dstate.dcontacts().to_remove().empty());
  //  ASSERT_EQ(dstate.dcontacts().to_modify().size(), 1u);
  //  auto to_modify = dstate.dcontacts().to_modify();
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->id(), 3);
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->value_str(), "new address");
}

/**
 * @brief Contact "name 3" has its first address removed. Addresses are stored
 * in an array. We don't have the information if an address is added or removed
 * so we send all the addresses in the difference. That's why the difference
 * tells about 4 addresses as difference.
 */
TEST_F(ApplierState, DiffOnContactFirstAddressRemoved) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto cts = new_config.mutable_contacts();
  auto& ct = (*cts)["name 3"];
  ct.mutable_address()->erase(ct.mutable_address()->begin());

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  //  ASSERT_TRUE(dstate.dcontacts().to_add().empty());
  //  ASSERT_TRUE(dstate.dcontacts().to_remove().empty());
  //  ASSERT_EQ(dstate.dcontacts().to_modify().size(), 1u);
  //  auto to_modify = dstate.dcontacts().to_modify();
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->id(), 2);
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->value_str(), "address 2");
}

/**
 * @brief Contact "name 3" has its first address removed. Addresses are stored
 * in an array. We don't have the information if an address is added or removed
 * so we send all the addresses in the difference. That's why the difference
 * tells about 4 addresses as difference.
 */
TEST_F(ApplierState, DiffOnContactSecondAddressUpdated) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto cts = new_config.mutable_contacts();
  auto& ct = (*cts)["name 3"];
  (*ct.mutable_address())[1] = "this address is different";

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  //  ASSERT_TRUE(dstate.dcontacts().to_add().empty());
  //  ASSERT_TRUE(dstate.dcontacts().to_remove().empty());
  //  ASSERT_EQ(dstate.dcontacts().to_modify().size(), 1u);
  //  auto to_modify = dstate.dcontacts().to_modify();
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->id(), 2);
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->value_str(), "address 2");
}

TEST_F(ApplierState, DiffOnContactRemoveCustomvariable) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  auto cts = new_config.mutable_contacts();
  auto& ct = (*cts)["name 3"];
  ct.mutable_customvariables()->erase(ct.mutable_customvariables()->begin());

  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesToString(&output);
  // differencer.set_repeated_field_comparison(
  //     util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(pb_config, new_config));
  std::cout << "Output= " << output << std::endl;

  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  //  ASSERT_TRUE(dstate.dcontacts().to_add().empty());
  //  ASSERT_TRUE(dstate.dcontacts().to_remove().empty());
  //  ASSERT_EQ(dstate.dcontacts().to_modify().size(), 1u);
  //  auto to_modify = dstate.dcontacts().to_modify();
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->id(), 2);
  //  ASSERT_EQ(to_modify["name 3"].list().begin()->value_str(), "address 2");
}
