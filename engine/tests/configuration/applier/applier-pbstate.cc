/*
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

#include <absl/strings/string_view.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <algorithm>
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/indexed_state.hh"
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/parser.hh"
#include "common/engine_conf/state.pb.h"
#include "tests/helper.hh"

using namespace com::centreon::engine;
using com::centreon::engine::configuration::TagType;

extern configuration::indexed_state pb_indexed_config;

class ApplierState : public ::testing::Test {
 protected:
 public:
  void SetUp() override {
    init_config_state();
    auto tps = pb_indexed_config.mut_state().mutable_timeperiods();
    for (int i = 0; i < 10; i++) {
      auto* tp = tps->Add();
      tp->set_alias(fmt::format("timeperiod {}", i));
      tp->set_timeperiod_name(fmt::format("Timeperiod {}", i));
    }
    for (int i = 0; i < 5; i++) {
      configuration::Contact* ct = pb_indexed_config.mut_state().add_contacts();
      configuration::contact_helper ct_hlp(ct);
      std::string name(fmt::format("name{:2}", i));
      ct->set_contact_name(name);
      ct->set_alias(fmt::format("alias{:2}", i));
      for (int j = 0; j < 3; j++)
        ct->add_address(fmt::format("address{:2}", j));
      for (int j = 0; j < 10; j++) {
        configuration::CustomVariable* cv = ct->add_customvariables();
        cv->set_name(fmt::format("key_{}_{}", name, j));
        cv->set_value(fmt::format("value_{}_{}", name, j));
      }
    }
  }

  void TearDown() override { deinit_config_state(); }
};

using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;

static void CreateFile(const std::string& filename,
                       const std::string& content) {
  std::ofstream oss(filename);
  oss << content;
  oss.close();
}

static void AddCfgFile(const std::string& filename) {
  std::ifstream ss("/tmp/centengine.cfg");
  std::list<std::string> lines;
  std::string s;
  while (getline(ss, s)) {
    lines.push_back(std::move(s));
  }
  for (auto it = lines.begin(); it != lines.end(); ++it) {
    if (it->find("cfg_file") == 0) {
      lines.insert(it, fmt::format("cfg_file={}", filename));
      break;
    }
  }
  std::ofstream oss("/tmp/centengine.cfg");
  for (auto& l : lines)
    oss << l << std::endl;
}

static void RmConf() {
  std::remove("/tmp/ad.cfg");
  std::remove("/tmp/centengine.cfg");
  std::remove("/tmp/commands.cfg");
  std::remove("/tmp/connectors.cfg");
  std::remove("/tmp/contactgroups.cfg");
  std::remove("/tmp/contacts.cfg");
  std::remove("/tmp/dependencies.cfg");
  std::remove("/tmp/hostescalations.cfg");
  std::remove("/tmp/hostgroups.cfg");
  std::remove("/tmp/hosts.cfg");
  std::remove("/tmp/resource.cfg");
  std::remove("/tmp/servicedependencies.cfg");
  std::remove("/tmp/serviceescalations.cfg");
  std::remove("/tmp/servicegroups.cfg");
  std::remove("/tmp/services.cfg");
  std::remove("/tmp/severities.cfg");
  std::remove("/tmp/tags.cfg");
  std::remove("/tmp/test-config.cfg");
  std::remove("/tmp/timeperiods.cfg");
}

enum class ConfigurationObject {
  ANOMALYDETECTION = 0,
  CONTACTGROUP = 1,
  DEPENDENCY = 2,
  ESCALATION = 3,
  SERVICEGROUP = 4,
  SEVERITY = 5,
  TAG = 6,
  CONTACTGROUP_NE = 7,
};

static void CreateConf(int idx) {
  constexpr const char* cmd1 =
      "for i in " ENGINE_CFG_TEST "/conf1/*.cfg ; do cp $i /tmp ; done";
  switch (idx) {
    case 1:
      system(cmd1);
      break;
    default:
      ASSERT_EQ(1, 0);
      break;
  }
}

static void CreateBadConf(ConfigurationObject obj) {
  CreateConf(1);
  switch (obj) {
    case ConfigurationObject::SERVICEGROUP:
      CreateFile("/tmp/servicegroups.cfg",
                 "define servicegroup {\n"
                 "    servicegroup_id                1000\n"
                 "    name                           sg_tpl\n"
                 "    members                        "
                 "host_1,service_1,host_1,service_2,host_1,service_4\n"
                 "    notes                          notes for sg template\n"
                 "}\n"
                 "define servicegroup {\n"
                 "    servicegroup_id                1\n"
                 "    alias                          sg1\n"
                 "    members                        "
                 "host_1,service_1,host_1,service_2,host_1,service_3\n"
                 "    notes                          notes for sg1\n"
                 "    notes_url                      notes url for sg1\n"
                 "    action_url                     action url for sg1\n"
                 "    use                            sg_tpl\n"
                 "}\n");
      break;
    case ConfigurationObject::TAG:
      CreateFile("/tmp/tags.cfg",
                 "define tag {\n"
                 "    tag_id                         1\n"
                 "    tag_name                       tag1\n"
                 "}\n");
      break;
    case ConfigurationObject::ANOMALYDETECTION:
      CreateFile("/tmp/ad.cfg",
                 "define anomalydetection {\n"
                 "   service_description             service_ad\n"
                 "   host_name                       Centreon-central\n"
                 "   service_id                      2000\n"
                 "   register                        1\n"
                 "   dependent_service_id            1\n"
                 "   thresholds_file                 /tmp/toto\n"
                 "}\n");
      AddCfgFile("/tmp/ad.cfg");
      break;
    case ConfigurationObject::CONTACTGROUP:
      CreateFile("/tmp/contactgroups.cfg",
                 "define contactgroup {\n"
                 "    name                           cg_tpl\n"
                 "    members                        user1,user2,user3\n"
                 "    contactgroup_members           cg2\n"
                 "}\n");
      break;
    case ConfigurationObject::CONTACTGROUP_NE:
      CreateFile("/tmp/contactgroups.cfg",
                 "define contactgroup {\n"
                 "    contactgroup_name              cg1\n"
                 "    alias                          cg1_a\n"
                 "    members                        user1,user2\n"
                 "    contactgroup_members           +cg3\n"
                 "}\n"
                 "define contactgroup {\n"
                 "    contactgroup_name              cg2\n"
                 "    alias                          cg2_a\n"
                 "    members                        user1,user2\n"
                 "    contactgroup_members           cg3\n"
                 "}\n"
                 "define contactgroup {\n"
                 "    contactgroup_name              cg3\n"
                 "    alias                          cg3_a\n"
                 "    use                            cg_tpl\n"
                 "    members                        +user3\n"
                 "    contactgroup_members           cg3\n"
                 "}\n"
                 "define contactgroup {\n"
                 "    contactgroup_name              cg_tpl\n"
                 "    name                           cg_tpl\n"
                 "    members                        user1,user2\n"
                 "    contactgroup_members           cg2\n"
                 "}\n");
      break;
    case ConfigurationObject::SEVERITY:
      CreateFile("/tmp/severities.cfg",
                 "define severity {\n"
                 "   severity_name                   sev1\n"
                 "   id                              3\n"
                 "   level                           14\n"
                 "   icon_id                         123\n"
                 "}\n");
      break;
    case ConfigurationObject::ESCALATION:
      CreateFile("/tmp/escalations.cfg",
                 "define serviceescalation {\n"
                 "    host_name                      host_1\n"
                 "    description                    service_2\n"
                 "}\n"
                 "define serviceescalation {\n"
                 "    host_name                      host_1\n"
                 "    contact_groups                 cg1\n"
                 "}\n"
                 "define hostescalation {\n"
                 "    contact_groups                 cg1,cg2\n"
                 "    name                           he_tmpl\n"
                 "}\n"
                 "define hostescalation {\n"
                 "    contact_groups                 +cg1\n"
                 "    hostgroup_name                 hg1,hg2\n"
                 "    use                            he_tmpl\n"
                 "}\n");
      break;
    case ConfigurationObject::DEPENDENCY:
      CreateFile("/tmp/dependencies.cfg",
                 "define hostdependency {\n"
                 "    dependent_hostgroup_name       hg1,hg2\n"
                 "    dependent_host_name            host_2\n"
                 "    name                           hd_tmpl\n"
                 "}\n"
                 "define servicedependency {\n"
                 "    servicegroup_name              sg1\n"
                 "    host                           host_1\n"
                 "    dependent_hostgroup_name       host_3\n"
                 "    dependent_host_name            host_2\n"
                 "    use                            hd_tmpl\n"
                 "}\n"
                 "define servicedependency {\n"
                 "    service_description            service_2\n"
                 "    dependent_description          service_1\n"
                 "}\n");
      break;
    default:
      break;
  }
}

constexpr size_t CFG_FILES = 19u;
constexpr size_t RES_FILES = 1u;
constexpr size_t HOSTS = 11u;
constexpr size_t SERVICES = 363u;
constexpr size_t TIMEPERIODS = 2u;
constexpr size_t CONTACTS = 1u;
constexpr size_t HOSTGROUPS = 2u;
constexpr size_t SERVICEGROUPS = 1u;
constexpr size_t HOSTDEPENDENCIES = 2u;

// TEST_F(ApplierState, DiffOnTimeperiod) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   EXPECT_TRUE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_TRUE(dstate.to_add().empty());
//   ASSERT_TRUE(dstate.to_remove().empty());
//   ASSERT_TRUE(dstate.to_modify().empty());
// }
//
// TEST_F(ApplierState, DiffOnTimeperiodOneRemoved) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   new_config.mutable_timeperiods()->RemoveLast();
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_EQ(dstate.to_remove().size(), 1u);
//   // Number 142 is to remove.
//   ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 143);
//   ASSERT_EQ(dstate.to_remove()[0].key()[1].i32(), 9);
//   ASSERT_EQ(dstate.to_remove()[0].key().size(), 2);
//   ASSERT_TRUE(dstate.to_add().empty());
//   ASSERT_TRUE(dstate.to_modify().empty());
// }

// TEST_F(ApplierState, DiffOnTimeperiodNewOne) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   auto tps = new_config.mutable_timeperiods();
//   auto* tp = tps->Add();
//   tp->set_alias("timeperiod 11");
//   tp->set_timeperiod_name("Timeperiod 11");
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_TRUE(dstate.to_remove().empty());
//   ASSERT_TRUE(dstate.to_modify().empty());
//   ASSERT_EQ(dstate.to_add().size(), 1u);
//   ASSERT_TRUE(dstate.to_add()[0].val().has_value_tp());
//   const configuration::Timeperiod& new_tp =
//   dstate.to_add()[0].val().value_tp(); ASSERT_EQ(new_tp.alias(),
//   std::string("timeperiod 11")); ASSERT_EQ(new_tp.timeperiod_name(),
//   std::string("Timeperiod 11"));
// }

// TEST_F(ApplierState, DiffOnTimeperiodAliasRenamed) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   auto tps = new_config.mutable_timeperiods();
//   tps->at(7).set_alias("timeperiod changed");
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_TRUE(dstate.to_remove().empty());
//   ASSERT_TRUE(dstate.to_add().empty());
//   ASSERT_EQ(dstate.to_modify().size(), 1u);
//   const configuration::PathWithValue& path = dstate.to_modify()[0];
//   ASSERT_EQ(path.path().key().size(), 4u);
//   // number 142 => timeperiods
//   ASSERT_EQ(path.path().key()[0].i32(), 143);
//   // index 7 => timeperiods[7]
//   ASSERT_EQ(path.path().key()[1].i32(), 7);
//   // number 2 => timeperiods.alias
//   ASSERT_EQ(path.path().key()[2].i32(), 2);
//   // No more key...
//   ASSERT_EQ(path.path().key()[3].i32(), -1);
//   ASSERT_TRUE(path.val().has_value_str());
//   // The new value of timeperiods[7].alias is "timeperiod changed"
//   ASSERT_EQ(path.val().value_str(), std::string("timeperiod changed"));
// }

// TEST_F(ApplierState, DiffOnContactOneRemoved) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   new_config.mutable_contacts()->DeleteSubrange(4, 1);
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_EQ(dstate.to_remove().size(), 1u);
//
//   ASSERT_EQ(dstate.to_remove()[0].key().size(), 2);
//   // number 131 => for contacts
//   ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 132);
//   // "name 4" => contacts["name 4"]
//   ASSERT_EQ(dstate.to_remove()[0].key()[1].i32(), 4);
//
//   ASSERT_TRUE(dstate.to_add().empty());
//   ASSERT_TRUE(dstate.to_modify().empty());
// }

// TEST_F(ApplierState, DiffOnContactOneAdded) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   pb_indexed_config.state().mutable_contacts()->DeleteSubrange(4, 1);
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_TRUE(dstate.to_remove().empty());
//   ASSERT_TRUE(dstate.to_modify().empty());
//   ASSERT_EQ(dstate.to_add().size(), 1u);
//   const configuration::PathWithValue& to_add = dstate.to_add()[0];
//   ASSERT_EQ(to_add.path().key().size(), 2u);
//   // Contact -> number 131
//   ASSERT_EQ(to_add.path().key()[0].i32(), 132);
//   // ASSERT_EQ(to_add.path().key()[1].str(), std::string("name 4"));
//   ASSERT_TRUE(to_add.val().has_value_ct());
// }

/**
 * @brief Contact "name 3" has a new address added. Addresses are stored in
 * an array. We don't have the information if an address is added or removed
 * so we send all the addresses in the difference. That's why the difference
 * tells about 4 addresses as difference.
 */
// TEST_F(ApplierState, DiffOnContactOneNewAddress) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   auto& ct = new_config.mutable_contacts()->at(3);
//   ct.add_address("new address");
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_EQ(dstate.to_add().size(), 1u);
//   ASSERT_TRUE(dstate.to_modify().empty());
//   ASSERT_TRUE(dstate.to_remove().empty());
//   ASSERT_EQ(dstate.to_add()[0].path().key().size(), 4u);
//   // Number of Contacts in State
//   ASSERT_EQ(dstate.to_add()[0].path().key()[0].i32(), 132);
//   // Key to the context to change
//   ASSERT_EQ(dstate.to_add()[0].path().key()[1].i32(), 3);
//   // Number of the object to modify
//   ASSERT_EQ(dstate.to_add()[0].path().key()[2].i32(), 2);
//   // Index of the new object to add.
//   ASSERT_EQ(dstate.to_add()[0].path().key()[3].i32(), 3);
//
//   ASSERT_EQ(dstate.to_add()[0].val().value_str(), std::string("new
//   address"));
// }

/**
 * @brief Contact "name 3" has its first address removed. Addresses are stored
 * in an array. We don't have the information if an address is added or removed
 * so we send all the addresses in the difference. That's why the difference
 * tells about 4 addresses as difference.
 */
// TEST_F(ApplierState, DiffOnContactFirstAddressRemoved) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   auto& ct = new_config.mutable_contacts()->at(3);
//   ct.mutable_address()->erase(ct.mutable_address()->begin());
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   ASSERT_TRUE(dstate.to_add().empty());
//   ASSERT_EQ(dstate.to_modify().size(), 2u);
//   ASSERT_EQ(dstate.to_remove().size(), 1u);
//   ASSERT_EQ(dstate.to_modify()[0].path().key().size(), 4u);
//   // Number of contacts in State
//   ASSERT_EQ(dstate.to_modify()[0].path().key()[0].i32(), 132);
//   // Key "name 3" to the good contact
//   ASSERT_EQ(dstate.to_modify()[0].path().key()[1].i32(), 3);
//   // Number of addresses in Contact
//   ASSERT_EQ(dstate.to_modify()[0].path().key()[2].i32(), 2);
//   // Index of the address to modify
//   ASSERT_EQ(dstate.to_modify()[0].path().key()[3].i32(), 0);
//   // New value of the address
//   ASSERT_EQ(dstate.to_modify()[0].val().value_str(), std::string("address
//   1"));
//
//   ASSERT_EQ(dstate.to_remove()[0].key().size(), 4u);
//   // Number of contacts in State
//   ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 132);
//   // Key "name 3" to the good contact
//   ASSERT_EQ(dstate.to_remove()[0].key()[1].i32(), 3);
//   // Number of addresses in Contact
//   ASSERT_EQ(dstate.to_remove()[0].key()[2].i32(), 2);
//   // Index of the address to remove
//   ASSERT_EQ(dstate.to_remove()[0].key()[3].i32(), 2);
// }

/**
 * @brief Contact "name 3" has its first address removed. Addresses are stored
 * in an array. We don't have the information if an address is added or removed
 * so we send all the addresses in the difference. That's why the difference
 * tells about 4 addresses as difference.
 */
// TEST_F(ApplierState, DiffOnContactSecondAddressUpdated) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   auto& ct = new_config.mutable_contacts()->at(3);
//   (*ct.mutable_address())[1] = "this address is different";
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   //  ASSERT_TRUE(dstate.dcontacts().to_add().empty());
//   //  ASSERT_TRUE(dstate.dcontacts().to_remove().empty());
//   //  ASSERT_EQ(dstate.dcontacts().to_modify().size(), 1u);
//   //  auto to_modify = dstate.dcontacts().to_modify();
//   //  ASSERT_EQ(to_modify["name 3"].list().begin()->id(), 2);
//   //  ASSERT_EQ(to_modify["name 3"].list().begin()->value_str(), "address
//   2");
// }

// TEST_F(ApplierState, DiffOnContactRemoveCustomvariable) {
//   configuration::State new_config;
//   new_config.CopyFrom(pb_indexed_config.state());
//   auto& ct = new_config.mutable_contacts()->at(3);
//   ct.mutable_customvariables()->erase(ct.mutable_customvariables()->begin());
//
//   std::string output;
//   MessageDifferencer differencer;
//   differencer.set_report_matches(false);
//   differencer.ReportDifferencesToString(&output);
//   // differencer.set_repeated_field_comparison(
//   //     util::MessageDifferencer::AS_SMART_LIST);
//   EXPECT_FALSE(differencer.Compare(pb_indexed_config.state(), new_config));
//   std::cout << "Output= " << output << std::endl;
//
//   configuration::DiffState dstate =
//       configuration::applier::state::instance().build_difference(pb_indexed_config.state(),
//                                                                  new_config);
//   //  ASSERT_TRUE(dstate.dcontacts().to_add().empty());
//   //  ASSERT_TRUE(dstate.dcontacts().to_remove().empty());
//   //  ASSERT_EQ(dstate.dcontacts().to_modify().size(), 1u);
//   //  auto to_modify = dstate.dcontacts().to_modify();
//   //  ASSERT_EQ(to_modify["name 3"].list().begin()->id(), 2);
//   //  ASSERT_EQ(to_modify["name 3"].list().begin()->value_str(), "address
//   2");
// }

TEST_F(ApplierState, StateParsing) {
  configuration::error_cnt err;
  configuration::indexed_state state;
  configuration::State& cfg = state.mut_state();
  configuration::parser p;
  CreateConf(1);
  p.parse("/tmp/centengine.cfg", &cfg, err);
  state.index();
  ASSERT_EQ(cfg.check_service_freshness(), false);
  ASSERT_EQ(cfg.enable_flap_detection(), false);
  ASSERT_EQ(cfg.instance_heartbeat_interval(), 30);
  ASSERT_EQ(cfg.log_level_functions(), configuration::LogLevel::warning);
  ASSERT_EQ(cfg.cfg_file().size(), CFG_FILES);
  ASSERT_EQ(cfg.resource_file().size(), RES_FILES);
  ASSERT_EQ(state.hosts().size(), HOSTS);
  auto& h1 = state.hosts().at(30);
  ASSERT_EQ(h1->host_name(), std::string("Centreon-central"));
  ASSERT_TRUE(h1->obj().register_());
  ASSERT_EQ(h1->host_id(), 30);
  auto& h2 = state.hosts().at(31);
  ASSERT_EQ(h2->host_name(), std::string("Centreon-central_1"));
  ASSERT_TRUE(h2->obj().register_());
  ASSERT_EQ(h2->host_id(), 31);
  auto& h3 = state.hosts().at(32);
  ASSERT_EQ(h3->host_name(), std::string("Centreon-central_2"));
  ASSERT_TRUE(h3->obj().register_());
  ASSERT_EQ(h3->host_id(), 32);
  auto& h4 = state.hosts().at(33);
  ASSERT_EQ(h4->host_name(), std::string("Centreon-central_3"));
  ASSERT_TRUE(h4->obj().register_());
  ASSERT_EQ(h4->host_id(), 33);

  /* Service */
  ASSERT_EQ(cfg.services().size(), SERVICES);
  ASSERT_EQ(cfg.services()[0].service_id(), 196);
  ASSERT_TRUE(cfg.services()[0].obj().register_());
  ASSERT_TRUE(cfg.services()[0].checks_active());
  ASSERT_EQ(cfg.services()[0].host_name(),
            std::string_view("Centreon-central"));
  ASSERT_EQ(cfg.services()[0].service_description(),
            std::string_view("proc-sshd"));
  ASSERT_EQ(cfg.services()[0].contactgroups().data().size(), 2u);
  EXPECT_EQ(cfg.services()[0].contactgroups().data()[0],
            std::string_view("Guest"));
  EXPECT_EQ(cfg.services()[0].contactgroups().data()[1],
            std::string_view("Supervisors"));

  EXPECT_EQ(cfg.services()[0].contacts().data().size(), 1u);
  EXPECT_EQ(cfg.services()[0].contacts().data()[0], std::string("John_Doe"));
  EXPECT_EQ(cfg.services()[0].notification_options(), 0x3f);
  std::set<std::pair<uint64_t, uint16_t>> exp{{2, tag::servicegroup}};
  std::set<std::pair<uint64_t, uint16_t>> res;
  for (auto& t : cfg.services()[0].tags()) {
    uint16_t c;
    switch (t.second()) {
      case TagType::tag_servicegroup:
        c = tag::servicegroup;
        break;
      case TagType::tag_hostgroup:
        c = tag::hostgroup;
        break;
      case TagType::tag_servicecategory:
        c = tag::servicecategory;
        break;
      case TagType::tag_hostcategory:
        c = tag::hostcategory;
        break;
      default:
        assert("Should not be raised" == nullptr);
    }
    res.emplace(t.first(), c);
  }
  EXPECT_EQ(res, exp);

  ASSERT_EQ(cfg.commands().size(), 15u);
  auto fnd_cmd =
      std::find_if(cfg.commands().begin(), cfg.commands().end(),
                   [](const configuration::Command& cmd) {
                     return cmd.command_name() ==
                            std::string_view("App-Centreon-MySQL-Partitioning");
                   });
  ASSERT_TRUE(fnd_cmd != cfg.commands().end());
  ASSERT_EQ(
      fnd_cmd->command_line(),
      std::string_view(
          "$CENTREONPLUGINS$/centreon_centreon_database.pl "
          "--plugin=database::mysql::plugin "
          "--dyn-mode=apps::centreon::sql::mode::partitioning "
          "--host='$HOSTADDRESS$' --username='$_HOSTMYSQLUSERNAME$' "
          "--password='$_HOSTMYSQLPASSWORD$' --port='$_HOSTMYSQLPORT$' "
          "--tablename='$_SERVICETABLENAME1$' "
          "--tablename='$_SERVICETABLENAME2$' "
          "--tablename='$_SERVICETABLENAME3$' "
          "--tablename='$_SERVICETABLENAME4$' --warning='$_SERVICEWARNING$' "
          "--critical='$_SERVICECRITICAL$'"));

  /* One command inherites from command_template */
  auto cit = std::find_if(cfg.commands().begin(), cfg.commands().end(),
                          [](const configuration::Command& cmd) {
                            return cmd.command_name() ==
                                   std::string_view("base_host_alive");
                          });

  ASSERT_NE(cit, cfg.commands().end());
  ASSERT_EQ(cit->command_name(), std::string_view("base_host_alive"));
  ASSERT_EQ(cit->command_line(),
            std::string_view("$USER1$/check_icmp -H $HOSTADDRESS$ -w "
                             "3000.0,80% -c 5000.0,100% -p 1"));

  ASSERT_EQ(cfg.timeperiods().size(), TIMEPERIODS);
  auto tit =
      std::find_if(cfg.timeperiods().begin(), cfg.timeperiods().end(),
                   [](const configuration::Timeperiod& tp) {
                     return tp.timeperiod_name() == std::string_view("24x7");
                   });
  ASSERT_NE(tit, cfg.timeperiods().end());
  EXPECT_EQ(tit->alias(), std::string_view("24_Hours_A_Day,_7_Days_A_Week"));
  EXPECT_EQ(tit->timeranges().sunday().size(),
            1u);  // std::string("00:00-24:00"));
  EXPECT_EQ(tit->timeranges().sunday()[0].range_start(), 0);
  EXPECT_EQ(tit->timeranges().sunday()[0].range_end(), 3600 * 24);
  EXPECT_EQ(tit->timeranges().monday().size(), 1u);
  EXPECT_EQ(tit->timeranges().monday()[0].range_start(), 0);
  EXPECT_EQ(tit->timeranges().monday()[0].range_end(), 86400);
  EXPECT_EQ(tit->timeranges().monday().size(), 1);
  EXPECT_EQ(tit->timeranges().tuesday().size(), 1u);
  EXPECT_EQ(tit->timeranges().wednesday().size(), 1u);
  EXPECT_EQ(tit->timeranges().thursday().size(), 1u);
  EXPECT_EQ(tit->timeranges().friday().size(), 1u);
  EXPECT_EQ(tit->timeranges().saturday().size(), 1u);

  ASSERT_EQ(cfg.contacts().size(), CONTACTS);
  const auto& ct = cfg.contacts().at(0);
  EXPECT_EQ(ct.contact_name(), std::string("John_Doe"));
  EXPECT_TRUE(ct.can_submit_commands());
  EXPECT_TRUE(ct.host_notifications_enabled());
  EXPECT_EQ(ct.host_notification_options(),
            configuration::action_hst_up | configuration::action_hst_down |
                configuration::action_hst_unreachable);
  EXPECT_TRUE(ct.retain_nonstatus_information());
  EXPECT_TRUE(ct.retain_status_information());
  EXPECT_TRUE(ct.service_notifications_enabled());
  EXPECT_EQ(ct.service_notification_options(),
            configuration::action_svc_warning |
                configuration::action_svc_unknown |
                configuration::action_svc_critical);
  EXPECT_EQ(ct.alias(), std::string_view("admin"));
  EXPECT_EQ(ct.contactgroups().data().size(), 0u);

  ASSERT_EQ(cfg.hostgroups().size(), HOSTGROUPS);
  auto hgit = cfg.hostgroups().begin();
  while (hgit != cfg.hostgroups().end() && hgit->hostgroup_name() != "hg1")
    ++hgit;
  ASSERT_TRUE(hgit != cfg.hostgroups().end());
  const auto hg = *hgit;
  ASSERT_EQ(hg.hostgroup_id(), 3u);
  ASSERT_EQ(hg.hostgroup_name(), std::string_view("hg1"));
  ASSERT_EQ(hg.alias(), std::string_view("hg1"));
  ASSERT_EQ(hg.members().data().size(), 3u);
  {
    auto it = hg.members().data().begin();
    ASSERT_EQ(*it, std::string_view("Centreon-central_2"));
    ++it;
    ASSERT_EQ(*it, std::string_view("Centreon-central_3"));
    ++it;
    ASSERT_EQ(*it, std::string_view("Centreon-central_4"));
  }
  ASSERT_EQ(hg.notes(), std::string_view("note_hg1"));
  ASSERT_EQ(hg.notes_url(), std::string_view());
  ASSERT_EQ(hg.action_url(), std::string_view());

  ASSERT_EQ(cfg.servicegroups().size(), SERVICEGROUPS);
  auto sgit = cfg.servicegroups().begin();
  while (sgit != cfg.servicegroups().end() &&
         sgit->servicegroup_name() != "Database-MySQL")
    ++sgit;
  ASSERT_TRUE(sgit != cfg.servicegroups().end());
  const auto sg = *sgit;
  ASSERT_EQ(sg.servicegroup_id(), 2u);
  ASSERT_EQ(sg.servicegroup_name(), std::string_view("Database-MySQL"));
  ASSERT_EQ(sg.alias(), std::string_view("Database-MySQL"));
  ASSERT_EQ(sg.members().data().size(), 67u);
  {
    auto find_pair = [&data = sg.members().data()](std::string_view first,
                                                   std::string_view second) {
      auto retval = std::find_if(
          data.begin(), data.end(),
          [&first, &second](const configuration::PairStringSet_Pair& m) {
            return m.first() == first && m.second() == second;
          });
      return retval;
    };

    auto it = sg.members().data().begin();
    it = find_pair("Centreon-central", "Connection-Time");
    ASSERT_NE(it, sg.members().data().end());

    it = find_pair("Centreon-central", "Connections-Number");
    ASSERT_NE(it, sg.members().data().end());

    it = find_pair("Centreon-central", "Myisam-Keycache");
    ASSERT_NE(it, sg.members().data().end());
  }
  ASSERT_EQ(sg.notes(), std::string_view());
  ASSERT_EQ(sg.notes_url(), std::string_view());
  ASSERT_EQ(sg.action_url(), std::string_view());

  auto sdit = cfg.servicedependencies().begin();
  while (sdit != cfg.servicedependencies().end() &&
         std::find(sdit->servicegroups().data().begin(),
                   sdit->servicegroups().data().end(),
                   "sg1") != sdit->servicegroups().data().end())
    ++sdit;
  ASSERT_TRUE(sdit != cfg.servicedependencies().end());
  ASSERT_TRUE(*sdit->hosts().data().begin() ==
              std::string_view("Centreon-central"));
  ASSERT_TRUE(*sdit->dependent_service_description().data().begin() ==
              std::string_view("Connections-Number"));
  ASSERT_TRUE(*sdit->dependent_hosts().data().begin() ==
              std::string_view("Centreon-central"));
  ASSERT_TRUE(sdit->inherits_parent());
  ASSERT_EQ(sdit->execution_failure_options(),
            configuration::action_sd_unknown | configuration::action_sd_ok);
  ASSERT_EQ(
      sdit->notification_failure_options(),
      configuration::action_sd_warning | configuration::action_sd_critical);

  // Anomalydetections
  ASSERT_TRUE(cfg.anomalydetections().empty());
  //  auto adit = cfg.anomalydetections().begin();
  //  while (adit != cfg.anomalydetections().end() &&
  //         (adit->service_id() != 2001 || adit->host_id() != 1))
  //    ++adit;
  //  ASSERT_TRUE(adit != cfg.anomalydetections().end());
  //  ASSERT_TRUE(adit->service_description() == "service_ad2");
  //  ASSERT_EQ(adit->dependent_service_id(), 1);
  //  ASSERT_TRUE(adit->metric_name() == "metric2");
  //  ASSERT_EQ(adit->customvariables().size(), 1);
  //  ASSERT_EQ(adit->customvariables().at(0).value(),
  //            std::string("this_is_a_test"));
  //  ASSERT_EQ(adit->contactgroups().data().size(), 2);
  //  ASSERT_EQ(adit->contacts().data().size(), 1);
  //  ASSERT_EQ(adit->servicegroups().data().size(), 2);

  auto cgit = cfg.contactgroups().begin();
  ASSERT_EQ(cfg.contactgroups().size(), 2u);
  ASSERT_EQ(cgit->contactgroup_name(), std::string_view("Guest"));
  ASSERT_EQ(cgit->alias(), std::string_view("Guests Group"));
  ASSERT_EQ(cgit->members().data().size(), 0u);
  ASSERT_EQ(cgit->contactgroup_members().data().size(), 0u);

  ++cgit;
  ASSERT_TRUE(cgit != cfg.contactgroups().end());
  ASSERT_EQ(cgit->contactgroup_name(), std::string_view("Supervisors"));
  ASSERT_EQ(cgit->alias(), std::string_view("Centreon supervisors"));
  ASSERT_EQ(cgit->members().data().size(), 1u);
  ASSERT_EQ(*cgit->members().data().begin(), "John_Doe");
  ASSERT_EQ(cgit->contactgroup_members().data().size(), 0u);

  ++cgit;
  ASSERT_TRUE(cgit == cfg.contactgroups().end());

  ASSERT_EQ(cfg.connectors().size(), 2);
  auto cnit = cfg.connectors().begin();
  ASSERT_TRUE(cnit != cfg.connectors().end());
  ASSERT_EQ(cnit->connector_name(), std::string_view("Perl Connector"));
  ASSERT_EQ(cnit->connector_line(),
            std::string_view(
                "/usr/lib64/centreon-connector/centreon_connector_perl "
                "--log-file=/var/log/centreon-engine/connector-perl.log"));
  ++cnit;
  ASSERT_EQ(cnit->connector_name(), std::string_view("SSH Connector"));
  ASSERT_EQ(cnit->connector_line(),
            std::string_view(
                "/usr/lib64/centreon-connector/centreon_connector_ssh "
                "--log-file=/var/log/centreon-engine/connector-ssh.log"));
  ++cnit;
  ASSERT_TRUE(cnit == cfg.connectors().end());

  /* Severities */
  ASSERT_EQ(state.severities().size(), 2);
  auto& sv = state.severities().at({5, configuration::SeverityType::service});
  //  auto svit = state.severities().begin();
  //  ++svit;
  //  ASSERT_TRUE(svit != cfg.severities().end());
  ASSERT_EQ(sv->severity_name(), std::string_view("severity1"));
  EXPECT_EQ(sv->key().id(), 5);
  EXPECT_EQ(sv->level(), 1);
  EXPECT_EQ(sv->icon_id(), 3);
  ASSERT_EQ(sv->key().type(), configuration::SeverityType::service);

  /* Serviceescalations */
  ASSERT_EQ(cfg.serviceescalations().size(), 6);
  auto seit = cfg.serviceescalations().begin();
  EXPECT_TRUE(seit != cfg.serviceescalations().end());
  EXPECT_EQ(*seit->hosts().data().begin(),
            std::string_view("Centreon-central"));
  ASSERT_EQ(*seit->service_description().data().begin(),
            std::string_view("Cpu"));
  ++seit;
  ASSERT_EQ(seit->hosts().data().size(), 1);
  ASSERT_EQ(seit->contactgroups().data().size(), 1);
  EXPECT_EQ(*seit->contactgroups().data().begin(), "Supervisors");
  ASSERT_EQ(seit->servicegroups().data().size(), 0);
  std::list<std::string> se_names;
  std::list<std::string> se_base{"Connection-Time", "Cpu", "Cpu",
                                 "Database-Size"};
  for (auto& se : cfg.serviceescalations()) {
    if (se.service_description().data().size()) {
      ASSERT_EQ(se.service_description().data().size(), 1);
      se_names.push_back(*se.service_description().data().begin());
    }
  }
  se_names.sort();
  ASSERT_EQ(se_names, se_base);

  /*Hostescalations */
  auto heit = cfg.hostescalations().begin();
  ASSERT_TRUE(heit != cfg.hostescalations().end());
  std::set<std::string> cts{"Supervisors"};
  std::set<std::string> he_cts;
  for (auto& cg : heit->contactgroups().data())
    he_cts.insert(cg);
  ASSERT_EQ(he_cts, cts);
  ++heit;

  std::set<std::string> hgs{"hg1"};
  std::set<std::string> he_hgs;
  for (auto& hg : heit->hostgroups().data())
    he_hgs.insert(hg);
  ASSERT_EQ(he_hgs, hgs);

  /*Hostdependencies */
  ASSERT_EQ(cfg.hostdependencies().size(), HOSTDEPENDENCIES);

  /* has_already_been_loaded is changed here */
  configuration::applier::state::instance().apply(state, err);

  ASSERT_TRUE(std::all_of(cfg.hostdependencies().begin(),
                          cfg.hostdependencies().end(), [](const auto& hd) {
                            return hd.hostgroups().data().empty() &&
                                   hd.dependent_hostgroups().data().empty();
                          }));

  RmConf();
  auto hst = host::hosts_by_id[30];
  ASSERT_TRUE(hst->custom_variables.find("CUSTOM_CV") !=
              hst->custom_variables.end());
  ASSERT_EQ(hst->custom_variables["CUSTOM_CV"].value(),
            std::string_view("custom_value"));
}

TEST_F(ApplierState, StateParsingServicegroupValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEGROUP);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config, err), std::exception);
}

TEST_F(ApplierState, StateParsingTagValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::TAG);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config, err), std::exception);
}

TEST_F(ApplierState, StateParsingAnomalydetectionValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::ANOMALYDETECTION);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config, err), std::exception);
}

TEST_F(ApplierState, StateParsingSeverityWithoutType) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SEVERITY);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config, err), std::exception);
}

TEST_F(ApplierState, StateParsingHostdependencyWithoutHost) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::DEPENDENCY);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config, err), std::exception);
}

TEST_F(ApplierState, StateParsingNonexistingContactgroup) {
  configuration::indexed_state state;
  configuration::State& cfg = state.mut_state();
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP_NE);
  configuration::error_cnt err;
  p.parse("/tmp/centengine.cfg", &cfg, err);
  state.index();
  ASSERT_THROW(configuration::applier::state::instance().apply(state, err),
               std::exception);
}

TEST_F(ApplierState, StateParsingContactgroupWithoutName) {
  configuration::State cfg;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &cfg, err), std::exception);
}
