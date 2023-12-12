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
#include "com/centreon/engine/configuration/parser.hh"
#include "com/centreon/engine/configuration/servicedependency.hh"
#include "com/centreon/engine/configuration/serviceescalation.hh"
#include "com/centreon/engine/globals.hh"
#include "tests/helper.hh"

using namespace com::centreon::engine;

extern configuration::state* config;

using TagType = com::centreon::engine::configuration::TagType;

class ApplierState : public ::testing::Test {
 public:
  void SetUp() override {
    config_errors = 0;
    config_warnings = 0;

    init_config_state();
  }

  void TearDown() override { deinit_config_state(); }
};

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

TEST_F(ApplierState, StateLegacyParsing) {
  configuration::state cfg;
  configuration::parser p;
  CreateConf(1);
  p.parse("/tmp/centengine.cfg", cfg);
  ASSERT_EQ(cfg.check_service_freshness(), false);
  ASSERT_EQ(cfg.enable_flap_detection(), false);
  ASSERT_EQ(cfg.instance_heartbeat_interval(), 30);
  ASSERT_EQ(cfg.log_level_functions(), std::string("warning"));
  ASSERT_EQ(cfg.cfg_file().size(), CFG_FILES);
  ASSERT_EQ(cfg.resource_file().size(), RES_FILES);
  ASSERT_EQ(cfg.hosts().size(), HOSTS);
  auto it = cfg.hosts().begin();
  ASSERT_EQ(it->host_name(), std::string("Centreon-central"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 30);
  ++it;
  ASSERT_EQ(it->host_name(), std::string("Centreon-central_1"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 31);
  ++it;
  ASSERT_EQ(it->host_name(), std::string("Centreon-central_2"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 32);
  ++it;
  ASSERT_EQ(it->host_name(), std::string("Centreon-central_3"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 33);

  /* Service */
  ASSERT_EQ(cfg.services().size(), SERVICES);
  auto sit = cfg.services().begin();
  ASSERT_EQ(sit->host_name(), std::string_view("Centreon-central"));
  ASSERT_EQ(sit->service_id(), 196);
  ASSERT_TRUE(sit->should_register());
  ASSERT_TRUE(sit->checks_active());
  ASSERT_EQ(sit->contactgroups().size(), 2u);
  {
    auto it = sit->contactgroups().begin();
    ASSERT_EQ(*it, std::string("Guest"));
    ++it;
    ASSERT_EQ(*it, std::string("Supervisors"));
    ++it;
    ASSERT_EQ(it, sit->contactgroups().end());
  }
  ASSERT_EQ(sit->host_name(), std::string("Centreon-central"));
  EXPECT_EQ(sit->service_description(), std::string("proc-sshd"));
  EXPECT_EQ(sit->contacts().size(), 1u);
  EXPECT_EQ(*sit->contacts().begin(), std::string("John_Doe"));
  EXPECT_EQ(sit->notification_options(), 0x3f);
  std::set<std::pair<uint64_t, uint16_t>> res{{2, tag::servicegroup}};
  EXPECT_EQ(sit->tags(), res);

  ASSERT_EQ(cfg.commands().size(), 15u);
  auto cit = cfg.commands().begin();
  ASSERT_EQ(cit->command_name(),
            std::string_view("App-Centreon-MySQL-Partitioning"));
  ASSERT_EQ(
      cit->command_line(),
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
  while (cit != cfg.commands().end() &&
         cit->command_name() != std::string_view("base_host_alive")) {
    ++cit;
  }

  ASSERT_NE(cit, cfg.commands().end());
  ASSERT_EQ(cit->command_name(), std::string_view("base_host_alive"));
  ASSERT_EQ(cit->command_line(),
            std::string_view("$USER1$/check_icmp -H $HOSTADDRESS$ -w "
                             "3000.0,80% -c 5000.0,100% -p 1"));

  ASSERT_EQ(cfg.timeperiods().size(), TIMEPERIODS);
  auto tit = cfg.timeperiods().begin();
  EXPECT_EQ(tit->timeperiod_name(), std::string_view("24x7"));
  EXPECT_EQ(tit->alias(), std::string_view("24_Hours_A_Day,_7_Days_A_Week"));
  EXPECT_EQ(tit->timeranges()[0].size(),
            1u);  // std::string("00:00-24:00"));
  EXPECT_EQ(tit->timeranges()[0].begin()->get_range_start(), 0);
  EXPECT_EQ(tit->timeranges()[0].begin()->get_range_end(), 3600 * 24);
  EXPECT_EQ(tit->timeranges()[1].size(), 1u);
  auto itt = tit->timeranges()[1].begin();
  EXPECT_EQ(itt->get_range_start(), 0);
  EXPECT_EQ(itt->get_range_end(), 86400);
  ++itt;
  EXPECT_EQ(itt, tit->timeranges()[1].end());
  EXPECT_EQ(tit->timeranges()[2].size(), 1u);  // tuesday
  EXPECT_EQ(tit->timeranges()[3].size(), 1u);  // wednesday
  EXPECT_EQ(tit->timeranges()[4].size(), 1u);  // thursday
  EXPECT_EQ(tit->timeranges()[5].size(), 1u);  // friday
  EXPECT_EQ(tit->timeranges()[6].size(), 1u);  // saturday

  ASSERT_EQ(cfg.contacts().size(), CONTACTS);
  auto ctit = cfg.contacts().begin();
  const auto ct = *ctit;
  EXPECT_EQ(ct.contact_name(), std::string_view("John_Doe"));
  EXPECT_TRUE(ct.can_submit_commands());
  EXPECT_TRUE(ct.host_notifications_enabled());
  EXPECT_EQ(ct.host_notification_options(),
            configuration::host::up | configuration::host::down |
                configuration::host::unreachable);
  EXPECT_TRUE(ct.retain_nonstatus_information());
  EXPECT_TRUE(ct.retain_status_information());
  EXPECT_TRUE(ct.service_notifications_enabled());
  EXPECT_EQ(ct.service_notification_options(),
            configuration::service::warning | configuration::service::unknown |
                configuration::service::critical);
  EXPECT_EQ(ct.alias(), std::string("admin"));
  ASSERT_EQ(ct.contactgroups().size(), 0u);

  ASSERT_EQ(cfg.hostgroups().size(), HOSTGROUPS);
  auto hgit = cfg.hostgroups().begin();
  while (hgit != cfg.hostgroups().end() && hgit->hostgroup_name() != "hg1")
    ++hgit;
  ASSERT_TRUE(hgit != cfg.hostgroups().end());
  const auto hg = *hgit;
  ASSERT_EQ(hg.hostgroup_id(), 3u);
  ASSERT_EQ(hg.hostgroup_name(), std::string_view("hg1"));
  ASSERT_EQ(hg.alias(), std::string_view("hg1"));
  ASSERT_EQ(hg.members().size(), 3u);
  {
    auto it = hg.members().begin();
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
  ASSERT_TRUE(sgit != cfg.servicegroups().end());
  const auto sg = *sgit;
  EXPECT_EQ(sg.servicegroup_id(), 2u);
  EXPECT_EQ(sg.servicegroup_name(), std::string_view("Database-MySQL"));
  EXPECT_EQ(sg.alias(), std::string_view("Database-MySQL"));
  EXPECT_EQ(sg.members().size(), 67u);
  {
    auto it = sg.members().begin();
    EXPECT_EQ(it->first, std::string_view("Centreon-central"));
    EXPECT_EQ(it->second, std::string_view("Connection-Time"));
    ++it;
    EXPECT_EQ(it->first, std::string_view("Centreon-central"));
    EXPECT_EQ(it->second, std::string_view("Connections-Number"));
    ++it;
    EXPECT_EQ(it->first, std::string_view("Centreon-central"));
    EXPECT_EQ(it->second, std::string_view("Myisam-Keycache"));
  }
  ASSERT_EQ(sg.notes(), std::string_view());
  ASSERT_EQ(sg.notes_url(), std::string_view());
  ASSERT_EQ(sg.action_url(), std::string_view());

  auto sdit = cfg.servicedependencies().begin();
  while (sdit != cfg.servicedependencies().end() &&
         std::find(sdit->servicegroups().begin(), sdit->servicegroups().end(),
                   "sg1") != sdit->servicegroups().end())
    ++sdit;
  ASSERT_TRUE(sdit != cfg.servicedependencies().end());
  ASSERT_EQ(*sdit->hosts().begin(), std::string_view("Centreon-central"));
  ASSERT_EQ(*sdit->dependent_service_description().begin(),
            std::string_view("Connections-Number"));
  ASSERT_EQ(*sdit->dependent_hosts().begin(),
            std::string_view("Centreon-central"));
  ASSERT_TRUE(sdit->inherits_parent());
  ASSERT_EQ(sdit->execution_failure_options(),
            configuration::servicedependency::ok |
                configuration::servicedependency::unknown);
  ASSERT_EQ(sdit->notification_failure_options(),
            configuration::servicedependency::warning |
                configuration::servicedependency::critical);

  // Anomalydetections
  ASSERT_TRUE(cfg.anomalydetections().empty());
  //  auto adit = cfg.anomalydetections().begin();
  //  for (auto& ad : cfg.anomalydetections())
  //    std::cout << "service_id => " << ad.service_id() << "  ; host_id => " <<
  //    ad.host_id() << std::endl;
  //
  //  while (adit != cfg.anomalydetections().end() &&
  //         adit->service_id() != 2001 && adit->host_id() != 1)
  //    ++adit;
  //  ASSERT_TRUE(adit != cfg.anomalydetections().end());
  //  ASSERT_TRUE(adit->service_description() == "service_ad2");
  //  ASSERT_EQ(adit->dependent_service_id(), 1);
  //  ASSERT_TRUE(adit->metric_name() == "metric2");
  //  ASSERT_EQ(adit->customvariables().size(), 1);
  //  ASSERT_EQ(adit->customvariables().at("ad_cv").value(),
  //            std::string("this_is_a_test"));
  //  ASSERT_EQ(adit->contactgroups().size(), 2);
  //  ASSERT_EQ(adit->contacts().size(), 1);
  //  ASSERT_EQ(adit->servicegroups().size(), 2);

  auto cgit = cfg.contactgroups().begin();
  ASSERT_EQ(cfg.contactgroups().size(), 2u);
  ASSERT_EQ(cgit->contactgroup_name(), std::string_view("Guest"));
  ASSERT_EQ(cgit->alias(), std::string_view("Guests Group"));
  ASSERT_EQ(cgit->members().size(), 0u);
  ASSERT_EQ(cgit->contactgroup_members().size(), 0u);

  ++cgit;
  ASSERT_TRUE(cgit != cfg.contactgroups().end());
  ASSERT_EQ(cgit->contactgroup_name(), std::string_view("Supervisors"));
  ASSERT_EQ(cgit->alias(), std::string_view("Centreon supervisors"));
  ASSERT_EQ(cgit->members().size(), 1u);
  ASSERT_EQ(*cgit->members().begin(), "John_Doe");
  ASSERT_EQ(cgit->contactgroup_members().size(), 0u);

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
  ASSERT_EQ(cfg.severities().size(), 2);
  auto svit = cfg.severities().begin();
  ASSERT_TRUE(svit != cfg.severities().end());
  ASSERT_EQ(svit->severity_name(), std::string_view("severity1"));
  EXPECT_EQ(svit->key().first, 5);
  EXPECT_EQ(svit->level(), 1);
  EXPECT_EQ(svit->icon_id(), 3);
  ASSERT_EQ(svit->type(), configuration::severity::service);

  /* Serviceescalations */
  ASSERT_EQ(cfg.serviceescalations().size(), 6);
  auto seit = std::find_if(
      cfg.serviceescalations().begin(), cfg.serviceescalations().end(),
      [](const configuration::serviceescalation& se) {
        return se.service_description().front() == std::string_view("Cpu");
      });
  EXPECT_TRUE(seit != cfg.serviceescalations().end());
  EXPECT_EQ(seit->hosts().front(), std::string_view("Centreon-central"));
  ASSERT_EQ(seit->service_description().front(), std::string_view("Cpu"));
  ++seit;
  ASSERT_EQ(seit->hosts().size(), 1);
  ASSERT_EQ(seit->contactgroups().size(), 1);
  EXPECT_EQ(*seit->contactgroups().begin(), "Supervisors");
  ASSERT_EQ(seit->servicegroups().size(), 0);
  std::list<std::string> se_names;
  std::list<std::string> se_base{"Connection-Time", "Cpu", "Cpu",
                                 "Database-Size"};
  for (auto& se : cfg.serviceescalations()) {
    if (se.service_description().size()) {
      ASSERT_EQ(se.service_description().size(), 1);
      se_names.push_back(se.service_description().front());
    }
  }
  se_names.sort();
  ASSERT_EQ(se_names, se_base);

  /*Hostescalations */
  auto heit = cfg.hostescalations().begin();
  ASSERT_TRUE(heit != cfg.hostescalations().end());
  std::set<std::string> cts{"Supervisors"};
  ASSERT_EQ(heit->contactgroups(), cts);
  ++heit;
  ASSERT_EQ(heit->contactgroups(), cts);
  std::set<std::string> hgs{"hg1"};
  ASSERT_EQ(heit->hostgroups(), hgs);

  /*Hostdependencies */
  ASSERT_EQ(cfg.hostdependencies().size(), HOSTDEPENDENCIES);
  std::cout << "###################### 1 #######################" << std::endl;
  configuration::applier::state::instance().apply(cfg);
  std::cout << "###################### 2 #######################" << std::endl;

  ASSERT_TRUE(std::all_of(config->hostdependencies().begin(),
                          config->hostdependencies().end(), [](const auto& hd) {
                            return hd.hostgroups().empty() &&
                                   hd.dependent_hostgroups().empty();
                          }));
  RmConf();
}

TEST_F(ApplierState, StateLegacyParsingServicegroupValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEGROUP);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingTagValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::TAG);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingServicedependencyValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::DEPENDENCY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingAnomalydetectionValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::ANOMALYDETECTION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingHostescalationWithoutHost) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::ESCALATION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingHostdependencyWithoutHost) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::DEPENDENCY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingNonexistingContactgroup) {
  configuration::state cfg;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP_NE);
  p.parse("/tmp/centengine.cfg", cfg);
  ASSERT_THROW(configuration::applier::state::instance().apply(cfg),
               std::exception);
}

TEST_F(ApplierState, StateLegacyParsingContactgroupWithoutName) {
  configuration::state cfg;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", cfg), std::exception);
}
