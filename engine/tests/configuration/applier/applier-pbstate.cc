/**
 * Copyright 2023-2025 Centreon (https://www.centreon.com/)
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
#include <filesystem>
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/parser.hh"
#include "common/engine_conf/state.pb.h"
#include "tests/helper.hh"

using namespace com::centreon::engine;
using com::centreon::engine::configuration::TagType;

class ApplierState : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();
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

static void copy_cfg_files(const std::filesystem::path& source,
                           const std::filesystem::path& destination) {
  if (!std::filesystem::exists(source) ||
      !std::filesystem::is_directory(source)) {
    std::cerr << "The source doesn't exist or is not a directory.\n";
    return;
  }

  if (!std::filesystem::exists(destination))
    std::filesystem::create_directories(destination);

  // Parse the directory
  for (const auto& entry : std::filesystem::directory_iterator(source)) {
    const auto& path = entry.path();
    auto dest_path = destination / path.filename();

    if (!std::filesystem::is_directory(path) && path.extension() == ".cfg")
      std::filesystem::copy_file(
          path, dest_path, std::filesystem::copy_options::overwrite_existing);
  }
}

static void CreateConf(int idx) {
  switch (idx) {
    case 1:
      copy_cfg_files(ENGINE_CFG_TEST "/conf1", "/tmp");
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

TEST_F(ApplierState, StateParsing) {
  configuration::error_cnt err;
  auto cfg = std::make_unique<configuration::State>();
  configuration::state_helper cfg_hlp(cfg.get());
  configuration::parser p;
  CreateConf(1);
  p.parse("/tmp/centengine.cfg", cfg.get(), err);
  cfg_hlp.expand(err);

  ASSERT_EQ(cfg->check_service_freshness(), false);
  ASSERT_EQ(cfg->enable_flap_detection(), false);
  ASSERT_EQ(cfg->instance_heartbeat_interval(), 30);
  ASSERT_EQ(cfg->log_level_functions(), configuration::LogLevel::warning);
  ASSERT_EQ(cfg->cfg_file().size(), CFG_FILES);
  ASSERT_EQ(cfg->resource_file().size(), RES_FILES);
  ASSERT_EQ(cfg->hosts_size(), HOSTS);
  auto hit = std::find_if(cfg->hosts().begin(), cfg->hosts().end(),
                          [](const auto& h) { return h.host_id() == 30; });
  ASSERT_NE(hit, cfg->hosts().end());
  auto& h1 = *hit;
  ASSERT_EQ(h1.host_name(), std::string("Centreon-central"));
  ASSERT_TRUE(h1.obj().register_());
  ASSERT_EQ(h1.host_id(), 30);
  hit = std::find_if(cfg->hosts().begin(), cfg->hosts().end(),
                     [](const auto& h) { return h.host_id() == 31; });
  ASSERT_NE(hit, cfg->hosts().end());
  auto& h2 = *hit;
  ASSERT_EQ(h2.host_name(), std::string("Centreon-central_1"));
  ASSERT_TRUE(h2.obj().register_());
  ASSERT_EQ(h2.host_id(), 31);
  hit = std::find_if(cfg->hosts().begin(), cfg->hosts().end(),
                     [](const auto& h) { return h.host_id() == 32; });
  ASSERT_NE(hit, cfg->hosts().end());
  auto& h3 = *hit;
  ASSERT_EQ(h3.host_name(), std::string("Centreon-central_2"));
  ASSERT_TRUE(h3.obj().register_());
  ASSERT_EQ(h3.host_id(), 32);
  hit = std::find_if(cfg->hosts().begin(), cfg->hosts().end(),
                     [](const auto& h) { return h.host_id() == 33; });
  ASSERT_NE(hit, cfg->hosts().end());
  auto& h4 = *hit;
  ASSERT_EQ(h4.host_name(), std::string("Centreon-central_3"));
  ASSERT_TRUE(h4.obj().register_());
  ASSERT_EQ(h4.host_id(), 33);

  //  /* Service */
  //  ASSERT_EQ(state.services().size(), SERVICES);
  //  ASSERT_NE(state.services().find({30, 196}), state.services().end());
  //  auto& s1 = *state.services().at({30, 196});
  //  ASSERT_TRUE(s1.obj().register_());
  //  ASSERT_TRUE(s1.checks_active());
  //  ASSERT_EQ(s1.host_name(), std::string_view("Centreon-central"));
  //  ASSERT_EQ(s1.service_description(), std::string_view("proc-sshd"));
  //  ASSERT_EQ(s1.contactgroups().data().size(), 2u);
  //  EXPECT_EQ(s1.contactgroups().data()[0], std::string_view("Guest"));
  //  EXPECT_EQ(s1.contactgroups().data()[1], std::string_view("Supervisors"));
  //
  //  EXPECT_EQ(s1.contacts().data().size(), 1u);
  //  EXPECT_EQ(s1.contacts().data()[0], std::string("John_Doe"));
  //  EXPECT_EQ(s1.notification_options(), 0x3f);
  //  std::set<std::pair<uint64_t, uint16_t>> exp{{2, tag::servicegroup}};
  //  std::set<std::pair<uint64_t, uint16_t>> res;
  //  for (auto& t : s1.tags()) {
  //    uint16_t c;
  //    switch (t.second()) {
  //      case TagType::tag_servicegroup:
  //        c = tag::servicegroup;
  //        break;
  //      case TagType::tag_hostgroup:
  //        c = tag::hostgroup;
  //        break;
  //      case TagType::tag_servicecategory:
  //        c = tag::servicecategory;
  //        break;
  //      case TagType::tag_hostcategory:
  //        c = tag::hostcategory;
  //        break;
  //      default:
  //        assert("Should not be raised" == nullptr);
  //    }
  //    res.emplace(t.first(), c);
  //  }
  //  EXPECT_EQ(res, exp);
  //
  //  ASSERT_EQ(state.commands().size(), 15u);
  //  auto fnd_cmd = state.commands().find("App-Centreon-MySQL-Partitioning");
  //  ASSERT_TRUE(fnd_cmd != state.commands().end());
  //  ASSERT_EQ(
  //      fnd_cmd->second->command_line(),
  //      std::string_view(
  //          "$CENTREONPLUGINS$/centreon_centreon_database.pl "
  //          "--plugin=database::mysql::plugin "
  //          "--dyn-mode=apps::centreon::sql::mode::partitioning "
  //          "--host='$HOSTADDRESS$' --username='$_HOSTMYSQLUSERNAME$' "
  //          "--password='$_HOSTMYSQLPASSWORD$' --port='$_HOSTMYSQLPORT$' "
  //          "--tablename='$_SERVICETABLENAME1$' "
  //          "--tablename='$_SERVICETABLENAME2$' "
  //          "--tablename='$_SERVICETABLENAME3$' "
  //          "--tablename='$_SERVICETABLENAME4$' --warning='$_SERVICEWARNING$'
  //          "
  //          "--critical='$_SERVICECRITICAL$'"));
  //
  //  /* One command inherites from command_template */
  //  auto cit = state.commands().find("base_host_alive");
  //  ASSERT_NE(cit, state.commands().end());
  //  ASSERT_EQ(cit->second->command_name(),
  //  std::string_view("base_host_alive"));
  //  ASSERT_EQ(cit->second->command_line(),
  //            std::string_view("$USER1$/check_icmp -H $HOSTADDRESS$ -w "
  //                             "3000.0,80% -c 5000.0,100% -p 1"));
  //
  //  ASSERT_EQ(state.timeperiods().size(), TIMEPERIODS);
  //  auto tit = state.timeperiods().find("24x7");
  //  ASSERT_NE(tit, state.timeperiods().end());
  //  EXPECT_EQ(tit->second->alias(),
  //            std::string_view("24_Hours_A_Day,_7_Days_A_Week"));
  //  EXPECT_EQ(tit->second->timeranges().sunday().size(),
  //            1u);  // std::string("00:00-24:00"));
  //  EXPECT_EQ(tit->second->timeranges().sunday()[0].range_start(), 0);
  //  EXPECT_EQ(tit->second->timeranges().sunday()[0].range_end(), 3600 * 24);
  //  EXPECT_EQ(tit->second->timeranges().monday().size(), 1u);
  //  EXPECT_EQ(tit->second->timeranges().monday()[0].range_start(), 0);
  //  EXPECT_EQ(tit->second->timeranges().monday()[0].range_end(), 86400);
  //  EXPECT_EQ(tit->second->timeranges().monday().size(), 1);
  //  EXPECT_EQ(tit->second->timeranges().tuesday().size(), 1u);
  //  EXPECT_EQ(tit->second->timeranges().wednesday().size(), 1u);
  //  EXPECT_EQ(tit->second->timeranges().thursday().size(), 1u);
  //  EXPECT_EQ(tit->second->timeranges().friday().size(), 1u);
  //  EXPECT_EQ(tit->second->timeranges().saturday().size(), 1u);
  //
  //  ASSERT_EQ(state.contacts().size(), CONTACTS);
  //  const auto& ct = state.contacts().at("John_Doe");
  //  EXPECT_EQ(ct->contact_name(), std::string("John_Doe"));
  //  EXPECT_TRUE(ct->can_submit_commands());
  //  EXPECT_TRUE(ct->host_notifications_enabled());
  //  EXPECT_EQ(ct->host_notification_options(),
  //            configuration::action_hst_up | configuration::action_hst_down |
  //                configuration::action_hst_unreachable);
  //  EXPECT_TRUE(ct->retain_nonstatus_information());
  //  EXPECT_TRUE(ct->retain_status_information());
  //  EXPECT_TRUE(ct->service_notifications_enabled());
  //  EXPECT_EQ(ct->service_notification_options(),
  //            configuration::action_svc_warning |
  //                configuration::action_svc_unknown |
  //                configuration::action_svc_critical);
  //  EXPECT_EQ(ct->alias(), std::string_view("admin"));
  //  EXPECT_EQ(ct->contactgroups().data().size(), 0u);
  //
  //  ASSERT_EQ(state.hostgroups().size(), HOSTGROUPS);
  //  auto hgit = state.hostgroups().find("hg1");
  //  ASSERT_TRUE(hgit != state.hostgroups().end());
  //  const auto hg = *hgit->second;
  //  ASSERT_EQ(hg.hostgroup_id(), 3u);
  //  ASSERT_EQ(hg.hostgroup_name(), std::string_view("hg1"));
  //  ASSERT_EQ(hg.alias(), std::string_view("hg1"));
  //  ASSERT_EQ(hg.members().data().size(), 3u);
  //  {
  //    auto it = hg.members().data().begin();
  //    ASSERT_EQ(*it, std::string_view("Centreon-central_2"));
  //    ++it;
  //    ASSERT_EQ(*it, std::string_view("Centreon-central_3"));
  //    ++it;
  //    ASSERT_EQ(*it, std::string_view("Centreon-central_4"));
  //  }
  //  ASSERT_EQ(hg.notes(), std::string_view("note_hg1"));
  //  ASSERT_EQ(hg.notes_url(), std::string_view());
  //  ASSERT_EQ(hg.action_url(), std::string_view());
  //
  //  ASSERT_EQ(state.servicegroups().size(), SERVICEGROUPS);
  //  auto sgit = state.servicegroups().find("Database-MySQL");
  //  ASSERT_TRUE(sgit != state.servicegroups().end());
  //  const auto sg = *sgit->second;
  //  ASSERT_EQ(sg.servicegroup_id(), 2u);
  //  ASSERT_EQ(sg.servicegroup_name(), std::string_view("Database-MySQL"));
  //  ASSERT_EQ(sg.alias(), std::string_view("Database-MySQL"));
  //  ASSERT_EQ(sg.members().data().size(), 67u);
  //  {
  //    auto find_pair = [&data = sg.members().data()](std::string_view first,
  //                                                   std::string_view second)
  //                                                   {
  //      auto retval = std::find_if(
  //          data.begin(), data.end(),
  //          [&first, &second](const configuration::PairStringSet_Pair& m) {
  //            return m.first() == first && m.second() == second;
  //          });
  //      return retval;
  //    };
  //
  //    auto it = sg.members().data().begin();
  //    it = find_pair("Centreon-central", "Connection-Time");
  //    ASSERT_NE(it, sg.members().data().end());
  //
  //    it = find_pair("Centreon-central", "Connections-Number");
  //    ASSERT_NE(it, sg.members().data().end());
  //
  //    it = find_pair("Centreon-central", "Myisam-Keycache");
  //    ASSERT_NE(it, sg.members().data().end());
  //  }
  //  ASSERT_EQ(sg.notes(), std::string_view());
  //  ASSERT_EQ(sg.notes_url(), std::string_view());
  //  ASSERT_EQ(sg.action_url(), std::string_view());
  //
  //  // Anomalydetections
  //  ASSERT_TRUE(state.anomalydetections().empty());
  //  //  auto adit = cfg.anomalydetections().begin();
  //  //  while (adit != cfg.anomalydetections().end() &&
  //  //         (adit->service_id() != 2001 || adit->host_id() != 1))
  //  //    ++adit;
  //  //  ASSERT_TRUE(adit != cfg.anomalydetections().end());
  //  //  ASSERT_TRUE(adit->service_description() == "service_ad2");
  //  //  ASSERT_EQ(adit->dependent_service_id(), 1);
  //  //  ASSERT_TRUE(adit->metric_name() == "metric2");
  //  //  ASSERT_EQ(adit->customvariables().size(), 1);
  //  //  ASSERT_EQ(adit->customvariables().at(0).value(),
  //  //            std::string("this_is_a_test"));
  //  //  ASSERT_EQ(adit->contactgroups().data().size(), 2);
  //  //  ASSERT_EQ(adit->contacts().data().size(), 1);
  //  //  ASSERT_EQ(adit->servicegroups().data().size(), 2);
  //
  //  ASSERT_EQ(state.contactgroups().size(), 2u);
  //  auto cgit = state.contactgroups().find("Guest");
  //  ASSERT_EQ(cgit->second->contactgroup_name(), std::string_view("Guest"));
  //  ASSERT_EQ(cgit->second->alias(), std::string_view("Guests Group"));
  //  ASSERT_EQ(cgit->second->members().data().size(), 0u);
  //  ASSERT_EQ(cgit->second->contactgroup_members().data().size(), 0u);
  //
  //  cgit = state.contactgroups().find("Supervisors");
  //  ASSERT_TRUE(cgit != state.contactgroups().end());
  //  ASSERT_EQ(cgit->second->contactgroup_name(),
  //  std::string_view("Supervisors")); ASSERT_EQ(cgit->second->alias(),
  //  std::string_view("Centreon supervisors"));
  //  ASSERT_EQ(cgit->second->members().data().size(), 1u);
  //  ASSERT_EQ(*cgit->second->members().data().begin(), "John_Doe");
  //  ASSERT_EQ(cgit->second->contactgroup_members().data().size(), 0u);
  //
  //  ASSERT_EQ(state.connectors().size(), 2);
  //  auto cnit = state.connectors().find("Perl Connector");
  //  ASSERT_TRUE(cnit != state.connectors().end());
  //  ASSERT_EQ(cnit->second->connector_name(), std::string_view("Perl
  //  Connector")); ASSERT_EQ(cnit->second->connector_line(),
  //            std::string_view(
  //                "/usr/lib64/centreon-connector/centreon_connector_perl "
  //                "--log-file=/var/log/centreon-engine/connector-perl.log"));
  //  cnit = state.connectors().find("SSH Connector");
  //  ASSERT_EQ(cnit->second->connector_name(), std::string_view("SSH
  //  Connector")); ASSERT_EQ(cnit->second->connector_line(),
  //            std::string_view(
  //                "/usr/lib64/centreon-connector/centreon_connector_ssh "
  //                "--log-file=/var/log/centreon-engine/connector-ssh.log"));
  //
  //  /* Severities */
  //  ASSERT_EQ(state.severities().size(), 2);
  //  auto& sv = state.severities().at({5,
  //  configuration::SeverityType::service});
  //  //  auto svit = state.severities().begin();
  //  //  ++svit;
  //  //  ASSERT_TRUE(svit != cfg.severities().end());
  //  ASSERT_EQ(sv->severity_name(), std::string_view("severity1"));
  //  EXPECT_EQ(sv->key().id(), 5);
  //  EXPECT_EQ(sv->level(), 1);
  //  EXPECT_EQ(sv->icon_id(), 3);
  //  ASSERT_EQ(sv->key().type(), configuration::SeverityType::service);
  //
  //  /* Serviceescalations, after the expansion, we should have 137 of them. */
  //  ASSERT_EQ(state.serviceescalations().size(), 137);
  //  auto seit = state.serviceescalations().begin();
  //  while (seit != state.serviceescalations().end()) {
  //    if (*seit->second->hosts().data().begin() ==
  //            std::string_view("Centreon-central") &&
  //        *seit->second->service_description().data().begin() ==
  //            std::string_view("Cpu"))
  //      break;
  //    ++seit;
  //  }
  //  ASSERT_EQ(*seit->second->hosts().data().begin(),
  //            std::string_view("Centreon-central"));
  //  ASSERT_EQ(*seit->second->service_description().data().begin(),
  //            std::string_view("Cpu"));
  //  ASSERT_EQ(seit->second->hosts().data().size(), 1);
  //  ASSERT_EQ(seit->second->contactgroups().data().size(), 1);
  //  EXPECT_EQ(*seit->second->contactgroups().data().begin(), "Supervisors");
  //  ASSERT_EQ(seit->second->servicegroups().data().size(), 0);
  //  absl::flat_hash_set<std::string_view> se_names;
  //  absl::flat_hash_set<std::string_view> se_base{
  //      "Connection-Time",   "Cpu",           "proc-sshd", "Partitioning",
  //      "Slowqueries",       "Database-Size", "Queries",   "Myisam-Keycache",
  //      "Connections-Number"};
  //  for (auto& se : state.serviceescalations()) {
  //    ASSERT_EQ(se.second->service_description().data().size(), 1);
  //    ASSERT_EQ(se.second->hosts().data().size(), 1);
  //    if (se.second->service_description().data().size())
  //      se_names.insert(*se.second->service_description().data().begin());
  //  }
  //  ASSERT_EQ(se_names, se_base);
  //
  //  /*Hostescalations */
  //  ASSERT_EQ(state.hostescalations().size(), 9);
  //  absl::flat_hash_set<std::string_view> expected_hosts{
  //      "Centreon-central",   "Centreon-central_1",  "Centreon-central_2",
  //      "Centreon-central_3", "Centreon-central_10", "Centreon-central_4"};
  //  absl::flat_hash_set<std::string_view> he_hosts;
  //  for (auto& he : state.hostescalations()) {
  //    ASSERT_EQ(he.second->hosts().data().size(), 1);
  //    he_hosts.insert(*he.second->hosts().data().begin());
  //  }
  //  ASSERT_EQ(he_hosts, expected_hosts);
  //
  //  /*Hostdependencies */
  //  /* With hostdep1, we get 2 x 2 pairs of hosts and x 2 for execution and
  //   * notification. With hostdep2, we get 3 x 5 pairs of host (3 hosts in hg1
  //   and
  //   * 5 in hg2) x 2 for execution and notification.
  //   * We should have (2 * 2 + 3 * 5) *2 = 38 hostdependencies.
  //   */
  //  ASSERT_EQ(state.hostdependencies().size(), 38);
  //
  //  for (auto& hd_pair : state.hostdependencies()) {
  //    auto& hd = *hd_pair.second;
  //    ASSERT_EQ(hd.hosts().data().size(), 1);
  //    ASSERT_EQ(hd.dependent_hosts().data().size(), 1);
  //    ASSERT_TRUE(hd.hostgroups().data().empty());
  //    ASSERT_TRUE(hd.dependent_hostgroups().data().empty());
  //    ASSERT_TRUE(hd.execution_failure_options() == 0 ||
  //                hd.notification_failure_options() == 0);
  //    ASSERT_TRUE(hd.execution_failure_options() != 0 ||
  //                hd.notification_failure_options() != 0);
  //  }
  //
  //  /* Servicedependencies */
  //  /* With servicedep1, we get 1 pair of services x 2 for execution and
  //   * notification. With servicedep2, we get 1 pair of services x 2 for
  //   execution
  //   * and notification.
  //   * We should have 2 * 2 = 4 servicedependencies.
  //   */
  //  ASSERT_EQ(state.servicedependencies().size(), 4);
  //
  //  for (auto& sd_pair : state.servicedependencies()) {
  //    auto& sd = *sd_pair.second;
  //    ASSERT_EQ(sd.hosts().data().size(), 1);
  //    ASSERT_EQ(sd.service_description().data().size(), 1);
  //    ASSERT_EQ(sd.dependent_hosts().data().size(), 1);
  //    ASSERT_EQ(sd.dependent_service_description().data().size(), 1);
  //    ASSERT_TRUE(sd.execution_failure_options() == 0 ||
  //                sd.notification_failure_options() == 0);
  //    ASSERT_TRUE(sd.execution_failure_options() != 0 ||
  //                sd.notification_failure_options() != 0);
  //  }
  //  configuration::applier::state::instance().apply(state, err);
  //
  //  ASSERT_TRUE(
  //      std::all_of(state.hostdependencies().begin(),
  //                  state.hostdependencies().end(), [](const auto& p) {
  //                    return p.second->hostgroups().data().empty() &&
  //                           p.second->dependent_hostgroups().data().empty();
  //                  }));
  //
  //  RmConf();
  //  std::cout << "Hosts list: " << std::endl;
  //  for (auto& h : host::hosts_by_id) {
  //    std::cout << "host " << h.first << " :" << h.second->name() <<
  //    std::endl;
  //  }
  //  auto hst = host::hosts_by_id[30];
  //  ASSERT_TRUE(hst);
  //  ASSERT_TRUE(hst->custom_variables.find("CUSTOM_CV") !=
  //              hst->custom_variables.end());
  //  ASSERT_EQ(hst->custom_variables["CUSTOM_CV"].value(),
  //            std::string_view("custom_value"));
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
  CreateBadConf(ConfigurationObject::CONTACTGROUP_NE);
  auto cfg = std::make_unique<configuration::State>();
  configuration::state_helper cfg_hlp(cfg.get());
  configuration::parser p;
  configuration::error_cnt err;
  p.parse("/tmp/centengine.cfg", cfg.get(), err);
  cfg_hlp.expand(err);
  ASSERT_THROW(configuration::applier::state::instance().apply(*cfg, err),
               std::exception);
}

TEST_F(ApplierState, StateParsingContactgroupWithoutName) {
  configuration::State cfg;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP);
  configuration::error_cnt err;
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &cfg, err), std::exception);
}
