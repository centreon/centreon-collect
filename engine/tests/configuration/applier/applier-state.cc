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

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/parser.hh"
#include "com/centreon/engine/globals.hh"
#include "configuration/contact_helper.hh"
#include "configuration/state.pb.h"

using namespace com::centreon::engine;

using TagType = com::centreon::engine::configuration::TagType;

class ApplierState : public ::testing::Test {
 protected:
  configuration::State pb_config;

 public:
  void SetUp() override {
    config_errors = 0;
    config_warnings = 0;

    auto tps = pb_config.mutable_timeperiods();
    for (int i = 0; i < 10; i++) {
      auto* tp = tps->Add();
      tp->set_alias(fmt::format("timeperiod {}", i));
      tp->set_timeperiod_name(fmt::format("Timeperiod {}", i));
    }
    for (int i = 0; i < 5; i++) {
      configuration::Contact* ct = pb_config.add_contacts();
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

  void TearDown() override {}
};

using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;

static void CreateFile(const std::string& filename,
                       const std::string& content) {
  std::ofstream oss(filename);
  oss << content;
}

static void RmConf() {
  std::remove("/tmp/ad.cfg");
  std::remove("/tmp/centengine.cfg");
  std::remove("/tmp/commands.cfg");
  std::remove("/tmp/connectors.cfg");
  std::remove("/tmp/contactgroups.cfg");
  std::remove("/tmp/contacts.cfg");
  std::remove("/tmp/hostdependencies.cfg");
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
  HOSTDEPENDENCY = 2,
  HOSTESCALATION = 3,
  SERVICEDEPENDENCY = 4,
  SERVICEESCALATION = 5,
  SERVICEGROUP = 6,
  SEVERITY = 7,
  TAG = 8,
};

static void CreateConf() {
  CreateFile("/tmp/hostescalations.cfg",
             "define hostescalation {\n"
             "    host_name                      host_1\n"
             "    contact_groups                 cg1,cg2\n"
             "    name                           he_tmpl\n"
             "}\n"
             "define hostescalation {\n"
             "    contact_groups                 +cg1\n"
             "    hostgroup_name                 hg1,hg2\n"
             "    use                            he_tmpl\n"
             "}\n");
  CreateFile("/tmp/serviceescalations.cfg",
             "define serviceescalation {\n"
             "    host_name                      host_1\n"
             "    description                    service_2\n"
             "    name                           se_tmpl\n"
             "}\n"
             "define serviceescalation {\n"
             "    contact_groups                 cg1\n"
             "    servicegroups                  sg1\n"
             "    use                            se_tmpl\n"
             "}\n");
  CreateFile("/tmp/severities.cfg",
             "define severity {\n"
             "   severity_name                   sev1\n"
             "   id                              3\n"
             "   level                           14\n"
             "   icon_id                         123\n"
             "   type                            service\n"
             "}\n");
  CreateFile("/tmp/connectors.cfg",
             "define connector {\n"
             "   connector_name                  conn_name1\n"
             "   connector_line                  conn_line1\n"
             "}\n"
             "define connector {\n"
             "   connector_name                  conn_name2\n"
             "   connector_line                  conn_line2\n"
             "}\n");
  CreateFile("/tmp/ad.cfg",
             "define anomalydetection {\n"
             "   name                            ad_tmpl\n"
             "   host_id                         1\n"
             "   register                        0\n"
             "   host_name                       host_1\n"
             "   notification_options            u,w,c\n"
             "   thresholds_file                 /tmp/toto\n"
             "   contact_groups                  cg1\n"
             "   contacts                        contact1\n"
             "   service_groups                  sg1\n"
             "}\n"
             "define anomalydetection {\n"
             "   service_description             service_ad1\n"
             "   service_id                      2000\n"
             "   dependent_service_id            1\n"
             "   metric_name                     metric1\n"
             "   register                        1\n"
             "   use                             ad_tmpl\n"
             "}\n"
             "define anomalydetection {\n"
             "   service_description             service_ad2\n"
             "   service_id                      2001\n"
             "   dependent_service_id            1\n"
             "   metric_name                     metric2\n"
             "   thresholds_file                 /tmp/titi\n"
             "   register                        1\n"
             "   use                             ad_tmpl\n"
             "   _ad_cv                          this_is_a_test\n"
             "   contact_groups                  +cg2\n"
             "   contacts                        contact2\n"
             "   service_groups                  +sg2\n"
             "}\n");
  CreateFile("/tmp/hostdependencies.cfg",
             "define hostdependency {\n"
             "    host_name                      host_1\n"
             "    dependent_hostgroup_name       hg1,hg2\n"
             "    dependent_host_name            host_2\n"
             "    name                           hd_tmpl\n"
             "}\n"
             "define hostdependency {\n"
             "    host                           host_2\n"
             "    dependent_hostgroup_name       host_3\n"
             "    dependent_host_name            host_2\n"
             "    use                            hd_tmpl\n"
             "}\n");
  CreateFile("/tmp/servicedependencies.cfg",
             "define servicedependency {\n"
             "    service_description            service_2\n"
             "    host                           host_1\n"
             "    dependent_description          service_1\n"
             "    dependent_host_name            host_1\n"
             "}\n"
             "define servicedependency {\n"
             "    servicegroup_name              sg1\n"
             "    host                           host_1\n"
             "    dependent_description          service_1\n"
             "    dependent_host_name            host_1\n"
             "}\n");
  CreateFile("/tmp/tags.cfg",
             "define tag {\n"
             "    tag_id                         1\n"
             "    tag_name                       tag1\n"
             "    type                           hostcategory\n"
             "}\n");
  CreateFile("/tmp/servicegroups.cfg",
             "define servicegroup {\n"
             "    servicegroup_id                1000\n"
             "    name                           sg_tpl\n"
             "    servicegroup_name              sg_tpl\n"
             "    members                        "
             "host_1,service_1,host_1,service_2,host_1,service_4\n"
             "    notes                          notes for sg template\n"
             "}\n"
             "define servicegroup {\n"
             "    servicegroup_id                1\n"
             "    servicegroup_name              sg1\n"
             "    alias                          sg1\n"
             "    members                        "
             "host_1,service_1,host_1,service_2,host_1,service_3\n"
             "    notes                          notes for sg1\n"
             "    notes_url                      notes url for sg1\n"
             "    action_url                     action url for sg1\n"
             "    use                            sg_tpl\n"
             "}\n"
             "define servicegroup {\n"
             "    servicegroup_id                2\n"
             "    servicegroup_name              sg2\n"
             "    alias                          sg2\n"
             "    members                        "
             "host_1,service_2,host_1,service_3,host_1,service_4\n"
             "    notes                          notes for sg2\n"
             "    notes_url                      notes url for sg2\n"
             "    action_url                     action url for sg2\n"
             "}\n");
  CreateFile("/tmp/hostgroups.cfg",
             "define hostgroup {\n"
             "    hostgroup_id                   1\n"
             "    hostgroup_name                 hg1\n"
             "    alias                          hg1\n"
             "    members                        host_1,host_2,host_3\n"
             "    notes                          notes for hg1\n"
             "    notes_url                      notes url for hg1\n"
             "    action_url                     action url for hg1\n"
             "}\n");
  CreateFile("/tmp/hosts.cfg",
             "define host {\n"
             "    host_name                      host_1\n"
             "    register                       1\n"
             "    alias                          host_1\n"
             "    address                        1.0.0.0\n"
             "    check_command                  checkh1\n"
             "    check_period                   24x7\n"
             "    register                       1\n"
             "    _KEY1                      VAL1\n"
             "    _SNMPCOMMUNITY                 public\n"
             "    _SNMPVERSION                   2c\n"
             "    _HOST_ID                       1\n"
             "}\n"
             "define host {\n"
             "    host_name                      host_2\n"
             "    register                       1\n"
             "    alias                          host_2\n"
             "    address                        2.0.0.0\n"
             "    check_command                  checkh2\n"
             "    check_period                   24x7\n"
             "    register                       1\n"
             "    _KEY2                      VAL2\n"
             "    _SNMPCOMMUNITY                 public\n"
             "    _SNMPVERSION                   2c\n"
             "    _HOST_ID                       2\n"
             "}\n"
             "define host {\n"
             "    host_name                      host_3\n"
             "    alias                          host_3\n"
             "    register                       1\n"
             "    address                        3.0.0.0\n"
             "    check_command                  checkh3\n"
             "    check_period                   24x7\n"
             "    register                       1\n"
             "    _KEY3                      VAL3\n"
             "    _SNMPCOMMUNITY                 public\n"
             "    _SNMPVERSION                   2c\n"
             "    _HOST_ID                       3\n"
             "}\n"
             "define host {\n"
             "    host_name                      host_4\n"
             "    alias                          host_4\n"
             "    register                       1\n"
             "    address                        4.0.0.0\n"
             "    check_command                  checkh4\n"
             "    check_period                   24x7\n"
             "    register                       1\n"
             "    _KEY4                      VAL4\n"
             "    _SNMPCOMMUNITY                 public\n"
             "    _SNMPVERSION                   2c\n"
             "    _HOST_ID                       4\n"
             "}\n");

  CreateFile("/tmp/services.cfg",
             "# comment 1\n"
             "# comment 2\n"
             "define service {\n"
             "    host_name                       host_1\n"
             "    name                            service_template\n"
             "    hostgroups                      hg1,hg2\n"
             "    contacts                        contact1\n"
             "    _SERVICE_ID                     1001\n"
             "    check_command                   command_19\n"
             "    check_period                    24x7\n"
             "    max_check_attempts              3\n"
             "    check_interval                  5\n"
             "    retry_interval                  5\n"
             "    register                        0\n"
             "    active_checks_enabled           1\n"
             "    passive_checks_enabled          1\n"
             "    notification_options            u,w,c\n"
             "    contact_groups                  cg1,cg2\n"
             "    group_tags                      1,3\n"
             "    category_tags                   11,13\n"
             "}\n"
             "define service {\n"
             "    host_name                       host_1\n"
             "    service_description             service_1\n"
             "    _SERVICE_ID                     1\n"
             "    check_command                   command_19\n"
             "    check_period                    24x7\n"
             "    contacts                        contact1,contact2\n"
             "    contact_groups                  +cg1,cg3\n"
             "    max_check_attempts              3\n"
             "    check_interval                  5\n"
             "    retry_interval                  5\n"
             "    register                        1\n"
             "    active_checks_enabled           1\n"
             "    passive_checks_enabled          1\n"
             "    group_tags                      2,3,5\n"
             "    category_tags                   12,13\n"
             "    use                             service_template\n"
             "}\n"
             "define service {\n"
             "    host_name                       host_1\n"
             "    service_description             service_2\n"
             "    _SERVICE_ID                     2\n"
             "    check_command                   command_47\n"
             "    check_period                    24x7\n"
             "    max_check_attempts              3\n"
             "    check_interval                  5\n"
             "    retry_interval                  5\n"
             "    register                        1\n"
             "    active_checks_enabled           1\n"
             "    passive_checks_enabled          1\n"
             "}\n"
             "define service {\n"
             "    host_name                       host_1\n"
             "    service_description             service_3\n"
             "    _SERVICE_ID                     3\n"
             "    check_command                   command_21\n"
             "    check_period                    24x7\n"
             "    max_check_attempts              3\n"
             "    check_interval                  5\n"
             "    retry_interval                  5\n"
             "    register                        1\n"
             "    active_checks_enabled           1\n"
             "    passive_checks_enabled          1\n"
             "}\n"
             "define service {\n"
             "    host_name                       host_1\n"
             "    service_description             service_4\n"
             "    _SERVICE_ID                     4\n"
             "    check_command                   command_30\n"
             "    check_period                    24x7\n"
             "    max_check_attempts              3\n"
             "    check_interval                  5\n"
             "    retry_interval                  5\n"
             "    register                        1\n"
             "    active_checks_enabled           1\n"
             "    passive_checks_enabled          1\n"
             "}\n");

  CreateFile("/tmp/commands.cfg",
             "define command {\n"
             "    command_name                    command_1\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 1\n"
             "}\n"
             "define command {\n"
             "    command_name                    command_2\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 2\n"
             "    connector                       Perl Connector\n"
             "}\n"
             "define command {\n"
             "    command_name                    command_3\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 3\n"
             "}\n"
             "define command {\n"
             "    command_name                    command_4\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 4\n"
             "    connector                       Perl Connector\n"
             "}\n"
             "define command {\n"
             "    command_name                    command_5\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 5\n"
             "}\n"
             "define command {\n"
             "    name                            command_template\n"
             "    command_name                    command_template\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 6\n"
             "    connector                       Perl Connector\n"
             "}\n"
             "define command {\n"
             "    command_name                    command_6\n"
             "    use                             command_template\n"
             "}\n"
             "define command {\n"
             "    command_name                    command_7\n"
             "    command_line                    "
             "/tmp/var/lib/centreon-engine/check.pl 7\n"
             "}\n");

  CreateFile(
      "/tmp/timeperiods.cfg",
      "define timeperiod {\n"
      "    name                           24x7\n"
      "    timeperiod_name                24x7\n"
      "    alias                          24_Hours_A_Day,_7_Days_A_Week\n"
      "    sunday                         00:00-24:00\n"
      "    monday                         00:00-24:00\n"
      "    tuesday                        00:00-24:00\n"
      "    wednesday                      00:00-24:00\n"
      "    thursday                       00:00-24:00\n"
      "    friday                         00:00-24:00\n"
      "    saturday                       00:00-24:00\n"
      "}\n"
      "define timeperiod {\n"
      "    name                           24x6\n"
      "    timeperiod_name                24x6\n"
      "    alias                          24_Hours_A_Day,_7_Days_A_Week\n"
      "    sunday                         00:00-24:00\n"
      "    monday                         00:00-8:00,18:00-24:00\n"
      "    tuesday                        00:00-24:00\n"
      "    wednesday                      00:00-24:00\n"
      "    thursday                       00:00-24:00\n"
      "    friday                         00:00-24:00\n"
      "    saturday                       00:00-24:00\n"
      "}\n");

  CreateFile("/tmp/resource.cfg",
             "# comment 3\n"
             "$USER1$=/usr/lib64/nagios/plugins\n"
             "$CENTREONPLUGINS$=/usr/lib/centreon/plugins/\n");

  CreateFile("/tmp/contacts.cfg",
             "define contact {\n"
             "    contact_name                   contact1\n"
             "    use                            template_contact\n"
             "    contact_groups                 super_cgroup1,super_cgroup2\n"
             "    host_notification_commands     host_command1\n"
             "    service_notification_commands  svc_command1\n"
             "}\n"
             "define contact {\n"
             "    name                           template_contact\n"
             "    contact_name                   template_contact\n"
             "    alias                          contact1\n"
             "    contact_groups                 cgroup1,cgroup3\n"
             "    host_notification_commands     host_command2\n"
             "    service_notification_commands  +svc_command1,svc_command2\n"
             "}\n");

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
  CreateFile(
      "/tmp/centengine.cfg",
      "# comment 4\n"
      "# comment 5\n"
      "# comment 6\n"
      "cfg_file=/tmp/severities.cfg\n"
      "cfg_file=/tmp/connectors.cfg\n"
      "cfg_file=/tmp/hosts.cfg\n"
      "cfg_file=/tmp/services.cfg\n"
      "cfg_file=/tmp/commands.cfg\n"
      "cfg_file=/tmp/contacts.cfg\n"
      "cfg_file=/tmp/hostgroups.cfg\n"
      "cfg_file=/tmp/servicegroups.cfg\n"
      "cfg_file=/tmp/timeperiods.cfg\n"
      "cfg_file=/tmp/tags.cfg\n"
      "cfg_file=/tmp/hostdependencies.cfg\n"
      "cfg_file=/tmp/servicedependencies.cfg\n"
      "cfg_file=/tmp/ad.cfg\n"
      "cfg_file=/tmp/contactgroups.cfg\n"
      "cfg_file=/tmp/hostescalations.cfg\n"
      "cfg_file=/tmp/serviceescalations.cfg\n"
      //"cfg_file=/tmp/etc/centreon-engine/config0/connectors.cfg\n"
      //      "broker_module=/usr/lib64/centreon-engine/externalcmd.so\n"
      //      "broker_module=/usr/lib64/nagios/cbmod.so "
      //      "/tmp/etc/centreon-broker/central-module0.json\n"
      //      "interval_length=60\n"
      //      "use_timezone=:Europe/Paris\n"
      "resource_file=/tmp/resource.cfg\n"
      //      "log_file=/tmp/var/log/centreon-engine/config0/centengine.log\n"
      //      "status_file=/tmp/var/log/centreon-engine/config0/status.dat\n"
      //      "command_check_interval=1s\n"
      //      "command_file=/tmp/var/lib/centreon-engine/config0/rw/centengine.cmd\n"
      //      "state_retention_file=/tmp/var/log/centreon-engine/config0/"
      //      "retention.dat\n"
      //      "retention_update_interval=60\n"
      //      "sleep_time=0.2\n"
      //      "service_inter_check_delay_method=s\n"
      //      "service_interleave_factor=s\n"
      //      "max_concurrent_checks=400\n"
      //      "max_service_check_spread=5\n"
      //      "check_result_reaper_frequency=5\n"
      //      "low_service_flap_threshold=25.0\n"
      //      "high_service_flap_threshold=50.0\n"
      //      "low_host_flap_threshold=25.0\n"
      //      "high_host_flap_threshold=50.0\n"
      //      "service_check_timeout=10\n"
      //      "host_check_timeout=12\n"
      //      "event_handler_timeout=30\n"
      //      "notification_timeout=30\n"
      //      "ocsp_timeout=5\n"
      //      "ochp_timeout=5\n"
      //      "perfdata_timeout=5\n"
      //      "date_format=euro\n"
      //      "illegal_object_name_chars=~!$%^&*\"|'<>?,()=\n"
      //      "illegal_macro_output_chars=`~$^&\"|'<>\n"
      //      "admin_email=titus@bidibule.com\n"
      //      "admin_pager=admin\n"
      //      "event_broker_options=-1\n"
      //      "cached_host_check_horizon=60\n"
      //      "debug_file=/tmp/var/log/centreon-engine/config0/centengine.debug\n"
      //      "debug_level=0\n"
      //      "debug_verbosity=2\n"
      //      "log_pid=1\n"
      //      "macros_filter=KEY80,KEY81,KEY82,KEY83,KEY84\n"
      //      "enable_macros_filter=0\n"
      //      "rpc_port=50001\n"
      //      "postpone_notification_to_timeperiod=0\n"
      "instance_heartbeat_interval=30\n"
      //      "enable_notifications=1\n"
      //      "execute_service_checks=1\n"
      //      "accept_passive_service_checks=1\n"
      //      "enable_event_handlers=1\n"
      //      "check_external_commands=1\n"
      //      "use_retained_program_state=1\n"
      //      "use_retained_scheduling_info=1\n"
      //      "use_syslog=0\n"
      //      "log_notifications=1\n"
      //      "log_service_retries=1\n"
      //      "log_host_retries=1\n"
      //      "log_event_handlers=1\n"
      //      "log_external_commands=1\n"
      //      "log_v2_enabled=1\n"
      //      "log_legacy_enabled=0\n"
      //      "log_v2_logger=file\n"
      "log_level_functions=trace\n"
      //      "log_level_config=info\n"
      //      "log_level_events=info\n"
      //      "log_level_checks=info\n"
      //      "log_level_notifications=info\n"
      //      "log_level_eventbroker=info\n"
      //      "log_level_external_command=trace\n"
      //      "log_level_commands=info\n"
      //      "log_level_downtimes=info\n"
      //      "log_level_comments=info\n"
      //      "log_level_macros=info\n"
      //      "log_level_process=info\n"
      //      "log_level_runtime=info\n"
      //      "log_flush_period=0\n"
      //      "soft_state_dependencies=0\n"
      //      "obsess_over_services=0\n"
      //      "process_performance_data=0\n"
      //      "check_for_orphaned_services=0\n"
      //      "check_for_orphaned_hosts=0\n"
      "check_service_freshness=1\n"
      "enable_flap_detection=0\n");
}

static void CreateBadConf(ConfigurationObject obj) {
  CreateConf();
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
    case ConfigurationObject::SERVICEDEPENDENCY:
      CreateFile("/tmp/servicedependencies.cfg",
                 "define servicedependency {\n"
                 "    service_description            service_2\n"
                 "    dependent_description          service_1\n"
                 "}\n");
      break;
    case ConfigurationObject::ANOMALYDETECTION:
      CreateFile("/tmp/ad.cfg",
                 "define anomalydetection {\n"
                 "   service_description             service_ad\n"
                 "   host_name                       host_1\n"
                 "   service_id                      2000\n"
                 "   register                        1\n"
                 "   dependent_service_id            1\n"
                 "   thresholds_file                 /tmp/toto\n"
                 "}\n");
      break;
    case ConfigurationObject::CONTACTGROUP:
      CreateFile("/tmp/contactgroups.cfg",
                 "define contactgroup {\n"
                 "    name                           cg_tpl\n"
                 "    members                        user1,user2,user3\n"
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
    case ConfigurationObject::SERVICEESCALATION:
      CreateFile("/tmp/serviceescalations.cfg",
                 "define serviceescalation {\n"
                 "    host_name                      host_1\n"
                 "    description                    service_2\n"
                 "}\n"
                 "define serviceescalation {\n"
                 "    host_name                      host_1\n"
                 "    contact_groups                 cg1\n"
                 "}\n");
      break;

    case ConfigurationObject::HOSTESCALATION:
      CreateFile("/tmp/hostescalations.cfg",
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
    case ConfigurationObject::HOSTDEPENDENCY:
      CreateFile("/tmp/hostdependencies.cfg",
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
                 "}\n");
      break;
    default:
      break;
  }
}

constexpr size_t CFG_FILES = 16u;
constexpr size_t RES_FILES = 1u;
constexpr size_t HOSTS = 4u;
constexpr size_t SERVICES = 4u;
constexpr size_t TIMEPERIODS = 2u;
constexpr size_t CONTACTS = 2u;
constexpr size_t HOSTGROUPS = 1u;
constexpr size_t SERVICEGROUPS = 3u;

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
  // Number 130 is to remove.
  ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 130);
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
  tp->set_timeperiod_name("Timeperiod 11");

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
  ASSERT_EQ(new_tp.timeperiod_name(), std::string("Timeperiod 11"));
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
  // number 130 => timeperiods
  ASSERT_EQ(path.path().key()[0].i32(), 130);
  // index 7 => timeperiods[7]
  ASSERT_EQ(path.path().key()[1].i32(), 7);
  // number 2 => timeperiods.alias
  ASSERT_EQ(path.path().key()[2].i32(), 2);
  // No more key...
  ASSERT_EQ(path.path().key()[3].i32(), -1);
  ASSERT_TRUE(path.val().has_value_str());
  // The new value of timeperiods[7].alias is "timeperiod changed"
  ASSERT_EQ(path.val().value_str(), std::string("timeperiod changed"));
}

TEST_F(ApplierState, DiffOnContactOneRemoved) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  new_config.mutable_contacts()->DeleteSubrange(4, 1);

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
  // number 119 => for contacts
  ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 119);
  // "name 4" => contacts["name 4"]
  ASSERT_EQ(dstate.to_remove()[0].key()[1].i32(), 4);

  ASSERT_TRUE(dstate.to_add().empty());
  ASSERT_TRUE(dstate.to_modify().empty());
}

TEST_F(ApplierState, DiffOnContactOneAdded) {
  configuration::State new_config;
  new_config.CopyFrom(pb_config);
  pb_config.mutable_contacts()->DeleteSubrange(4, 1);

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
  // Contact -> number 119
  ASSERT_EQ(to_add.path().key()[0].i32(), 119);
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
  auto& ct = new_config.mutable_contacts()->at(3);
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
  ASSERT_EQ(dstate.to_add().size(), 1u);
  ASSERT_TRUE(dstate.to_modify().empty());
  ASSERT_TRUE(dstate.to_remove().empty());
  ASSERT_EQ(dstate.to_add()[0].path().key().size(), 4u);
  // Number of Contacts in State
  ASSERT_EQ(dstate.to_add()[0].path().key()[0].i32(), 119);
  // Key to the context to change
  ASSERT_EQ(dstate.to_add()[0].path().key()[1].i32(), 3);
  // Number of the object to modify
  ASSERT_EQ(dstate.to_add()[0].path().key()[2].i32(), 2);
  // Index of the new object to add.
  ASSERT_EQ(dstate.to_add()[0].path().key()[3].i32(), 3);

  ASSERT_EQ(dstate.to_add()[0].val().value_str(), std::string("new address"));
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
  auto& ct = new_config.mutable_contacts()->at(3);
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
  ASSERT_TRUE(dstate.to_add().empty());
  ASSERT_EQ(dstate.to_modify().size(), 2u);
  ASSERT_EQ(dstate.to_remove().size(), 1u);
  ASSERT_EQ(dstate.to_modify()[0].path().key().size(), 4u);
  // Number of contacts in State
  ASSERT_EQ(dstate.to_modify()[0].path().key()[0].i32(), 119);
  // Key "name 3" to the good contact
  ASSERT_EQ(dstate.to_modify()[0].path().key()[1].i32(), 3);
  // Number of addresses in Contact
  ASSERT_EQ(dstate.to_modify()[0].path().key()[2].i32(), 2);
  // Index of the address to modify
  ASSERT_EQ(dstate.to_modify()[0].path().key()[3].i32(), 0);
  // New value of the address
  ASSERT_EQ(dstate.to_modify()[0].val().value_str(), std::string("address 1"));

  ASSERT_EQ(dstate.to_remove()[0].key().size(), 4u);
  // Number of contacts in State
  ASSERT_EQ(dstate.to_remove()[0].key()[0].i32(), 119);
  // Key "name 3" to the good contact
  ASSERT_EQ(dstate.to_remove()[0].key()[1].i32(), 3);
  // Number of addresses in Contact
  ASSERT_EQ(dstate.to_remove()[0].key()[2].i32(), 2);
  // Index of the address to remove
  ASSERT_EQ(dstate.to_remove()[0].key()[3].i32(), 2);
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
  auto& ct = new_config.mutable_contacts()->at(3);
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
  auto& ct = new_config.mutable_contacts()->at(3);
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

TEST_F(ApplierState, StateLegacyParsing) {
  configuration::state config;
  configuration::parser p;
  CreateConf();
  p.parse("/tmp/centengine.cfg", config);
  ASSERT_EQ(config.check_service_freshness(), true);
  ASSERT_EQ(config.enable_flap_detection(), false);
  ASSERT_EQ(config.instance_heartbeat_interval(), 30);
  ASSERT_EQ(config.log_level_functions(), std::string("trace"));
  ASSERT_EQ(config.cfg_file().size(), CFG_FILES);
  ASSERT_EQ(config.resource_file().size(), RES_FILES);
  ASSERT_EQ(config.hosts().size(), HOSTS);
  auto it = config.hosts().begin();
  ASSERT_EQ(it->host_name(), std::string("host_1"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 1);
  ++it;
  ASSERT_EQ(it->host_name(), std::string("host_2"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 2);
  ++it;
  ASSERT_EQ(it->host_name(), std::string("host_3"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 3);
  ++it;
  ASSERT_EQ(it->host_name(), std::string("host_4"));
  ASSERT_TRUE(it->should_register());
  ASSERT_EQ(it->host_id(), 4);

  /* Service */
  ASSERT_EQ(config.services().size(), SERVICES);
  auto sit = config.services().begin();
  ASSERT_EQ(sit->hosts().size(), 1u);
  ASSERT_EQ(sit->service_id(), 1);
  ASSERT_TRUE(sit->should_register());
  ASSERT_TRUE(sit->checks_active());
  ASSERT_EQ(sit->contactgroups().size(), 3u);
  {
    auto it = sit->contactgroups().begin();
    ASSERT_EQ(*it, std::string("cg1"));
    ++it;
    ASSERT_EQ(*it, std::string("cg2"));
    ++it;
    ASSERT_EQ(*it, std::string("cg3"));
  }
  ASSERT_EQ(*sit->hosts().begin(), std::string("host_1"));
  ASSERT_EQ(sit->service_description(), std::string("service_1"));
  EXPECT_EQ(sit->hostgroups().size(), 2u);
  EXPECT_EQ(*sit->hostgroups().begin(), std::string("hg1"));
  EXPECT_EQ(sit->contacts().size(), 2u);
  EXPECT_EQ(*sit->contacts().begin(), std::string("contact1"));
  EXPECT_EQ(sit->notification_options(), configuration::service::warning |
                                             configuration::service::unknown |
                                             configuration::service::critical);
  std::set<std::pair<uint64_t, uint16_t>> res{
      {1, tag::servicegroup},     {2, tag::servicegroup},
      {3, tag::servicegroup},     {5, tag::servicegroup},
      {11, tag::servicecategory}, {12, tag::servicecategory},
      {13, tag::servicecategory}};
  EXPECT_EQ(sit->tags(), res);

  ASSERT_EQ(config.commands().size(), 8u);
  auto cit = config.commands().begin();
  ASSERT_EQ(cit->command_name(), std::string("command_1"));
  ASSERT_EQ(cit->command_line(),
            std::string("/tmp/var/lib/centreon-engine/check.pl 1"));

  /* One command inherites from command_template */
  while (cit != config.commands().end() &&
         cit->command_name() != std::string("command_6"))
    ++cit;
  ASSERT_EQ(cit->command_name(), std::string("command_6"));
  ASSERT_EQ(cit->command_line(),
            std::string("/tmp/var/lib/centreon-engine/check.pl 6"));

  ASSERT_EQ(config.timeperiods().size(), TIMEPERIODS);
  auto tit = config.timeperiods().begin();
  EXPECT_EQ(tit->timeperiod_name(), std::string("24x6"));
  EXPECT_EQ(tit->alias(), std::string("24_Hours_A_Day,_7_Days_A_Week"));
  EXPECT_EQ(tit->timeranges()[0].size(),
            1u);  // std::string("00:00-24:00"));
  EXPECT_EQ(tit->timeranges()[0].begin()->get_range_start(), 0);
  EXPECT_EQ(tit->timeranges()[0].begin()->get_range_end(), 3600 * 24);
  EXPECT_EQ(tit->timeranges()[1].size(), 2u);
  auto itt = tit->timeranges()[1].begin();
  EXPECT_EQ(itt->get_range_start(), 0);  // 00:00-08:00
  EXPECT_EQ(itt->get_range_end(), 3600 * 8);
  ++itt;
  EXPECT_EQ(itt->get_range_start(), 3600 * 18);  // 18:00-24:00
  ASSERT_EQ(itt->get_range_end(), 3600 * 24);
  EXPECT_EQ(tit->timeranges()[2].size(), 1u);  // tuesday
  EXPECT_EQ(tit->timeranges()[3].size(), 1u);  // wednesday
  EXPECT_EQ(tit->timeranges()[4].size(), 1u);  // thursday
  EXPECT_EQ(tit->timeranges()[5].size(), 1u);  // friday
  EXPECT_EQ(tit->timeranges()[6].size(), 1u);  // saturday

  ASSERT_EQ(config.contacts().size(), CONTACTS);
  auto ctit = config.contacts().begin();
  while (ctit != config.contacts().end() && ctit->contact_name() != "contact1")
    ++ctit;
  ASSERT_TRUE(ctit != config.contacts().end());
  const auto ct = *ctit;
  EXPECT_EQ(ct.contact_name(), std::string("contact1"));
  EXPECT_TRUE(ct.can_submit_commands());
  EXPECT_TRUE(ct.host_notifications_enabled());
  EXPECT_EQ(ct.host_notification_options(), configuration::host::none);
  EXPECT_TRUE(ct.retain_nonstatus_information());
  EXPECT_TRUE(ct.retain_status_information());
  EXPECT_TRUE(ct.service_notifications_enabled());
  EXPECT_EQ(ct.service_notification_options(), configuration::service::none);
  EXPECT_EQ(ct.alias(), std::string("contact1"));
  EXPECT_EQ(ct.contactgroups().size(), 2u);
  auto ctgit = ct.contactgroups().begin();
  EXPECT_EQ(*ctgit, std::string("super_cgroup1"));
  ++ctgit;
  EXPECT_EQ(*ctgit, std::string("super_cgroup2"));

  ASSERT_EQ(config.hostgroups().size(), HOSTGROUPS);
  auto hgit = config.hostgroups().begin();
  while (hgit != config.hostgroups().end() && hgit->hostgroup_name() != "hg1")
    ++hgit;
  ASSERT_TRUE(hgit != config.hostgroups().end());
  const auto hg = *hgit;
  ASSERT_EQ(hg.hostgroup_id(), 1u);
  ASSERT_EQ(hg.hostgroup_name(), std::string("hg1"));
  ASSERT_EQ(hg.alias(), std::string("hg1"));
  ASSERT_EQ(hg.members().size(), 3u);
  {
    auto it = hg.members().begin();
    ASSERT_EQ(*it, std::string("host_1"));
    ++it;
    ASSERT_EQ(*it, std::string("host_2"));
    ++it;
    ASSERT_EQ(*it, std::string("host_3"));
  }
  ASSERT_EQ(hg.notes(), std::string("notes for hg1"));
  ASSERT_EQ(hg.notes_url(), std::string("notes url for hg1"));
  ASSERT_EQ(hg.action_url(), std::string("action url for hg1"));

  ASSERT_EQ(config.servicegroups().size(), SERVICEGROUPS);
  auto sgit = config.servicegroups().begin();
  while (sgit != config.servicegroups().end() &&
         sgit->servicegroup_name() != "sg2")
    ++sgit;
  ASSERT_TRUE(sgit != config.servicegroups().end());
  const auto sg = *sgit;
  ASSERT_EQ(sg.servicegroup_id(), 2u);
  ASSERT_EQ(sg.servicegroup_name(), std::string("sg2"));
  ASSERT_EQ(sg.alias(), std::string("sg2"));
  ASSERT_EQ(sg.members().size(), 3u);
  {
    auto it = sg.members().begin();
    ASSERT_EQ(it->first, std::string("host_1"));
    ASSERT_EQ(it->second, std::string("service_2"));
    ++it;
    ASSERT_EQ(it->first, std::string("host_1"));
    ASSERT_EQ(it->second, std::string("service_3"));
    ++it;
    ASSERT_EQ(it->first, std::string("host_1"));
    ASSERT_EQ(it->second, std::string("service_4"));
  }
  ASSERT_EQ(sg.notes(), std::string("notes for sg2"));
  ASSERT_EQ(sg.notes_url(), std::string("notes url for sg2"));
  ASSERT_EQ(sg.action_url(), std::string("action url for sg2"));

  auto sdit = config.servicedependencies().begin();
  while (sdit != config.servicedependencies().end() &&
         std::find(sdit->servicegroups().begin(), sdit->servicegroups().end(),
                   "sg1") != sdit->servicegroups().end())
    ++sdit;
  ASSERT_TRUE(sdit != config.servicedependencies().end());
  ASSERT_TRUE(*sdit->hosts().begin() == std::string("host_1"));
  ASSERT_TRUE(*sdit->dependent_service_description().begin() ==
              std::string("service_1"));
  ASSERT_TRUE(*sdit->dependent_hosts().begin() == std::string("host_1"));
  ASSERT_FALSE(sdit->inherits_parent());
  ASSERT_EQ(sdit->execution_failure_options(),
            configuration::servicedependency::none);
  ASSERT_EQ(sdit->notification_failure_options(),
            configuration::servicedependency::none);

  // Anomalydetections
  auto adit = config.anomalydetections().begin();
  while (adit != config.anomalydetections().end() &&
         adit->service_id() != 2001 && adit->host_id() != 1)
    ++adit;
  ASSERT_TRUE(adit != config.anomalydetections().end());
  ASSERT_TRUE(adit->service_description() == "service_ad2");
  ASSERT_EQ(adit->dependent_service_id(), 1);
  ASSERT_TRUE(adit->metric_name() == "metric2");
  ASSERT_EQ(adit->customvariables().size(), 1);
  ASSERT_EQ(adit->customvariables().at("ad_cv").value(),
            std::string("this_is_a_test"));
  ASSERT_EQ(adit->contactgroups().size(), 2);
  ASSERT_EQ(adit->contacts().size(), 1);
  ASSERT_EQ(adit->servicegroups().size(), 2);

  auto cgit = config.contactgroups().begin();
  ASSERT_TRUE(cgit != config.contactgroups().end());
  ASSERT_TRUE(cgit->contactgroup_name() == "cg1");
  ASSERT_TRUE(cgit->alias() == "cg1_a");
  ASSERT_EQ(cgit->members().size(), 2u);
  ASSERT_EQ(cgit->contactgroup_members().size(), 1u);

  ++cgit;
  ASSERT_TRUE(cgit != config.contactgroups().end());
  ASSERT_TRUE(cgit->contactgroup_name() == "cg2");
  ASSERT_TRUE(cgit->alias() == "cg2_a");
  ASSERT_EQ(cgit->members().size(), 2u);
  ASSERT_EQ(*cgit->members().begin(), "user1");
  ASSERT_EQ(cgit->contactgroup_members().size(), 1u);

  ++cgit;
  ASSERT_TRUE(cgit != config.contactgroups().end());
  ASSERT_TRUE(cgit->contactgroup_name() == "cg3");
  ASSERT_TRUE(cgit->alias() == "cg3_a");
  ASSERT_EQ(cgit->members().size(), 3u);
  {
    auto it = cgit->members().begin();
    ASSERT_EQ(*it, "user1");
    ++it;
    ASSERT_EQ(*it, "user2");
    ++it;
    ASSERT_EQ(*it, "user3");
  }
  ASSERT_EQ(cgit->contactgroup_members().size(), 1u);
  ASSERT_EQ(*cgit->contactgroup_members().begin(), "cg3");

  auto cnit = config.connectors().begin();
  ASSERT_TRUE(cnit != config.connectors().end());
  ASSERT_TRUE(cnit->connector_name() == "conn_name1");
  ASSERT_TRUE(cnit->connector_line() == "conn_line1");
  ++cnit;
  ASSERT_TRUE(cnit->connector_name() == "conn_name2");
  ASSERT_TRUE(cnit->connector_line() == "conn_line2");

  /* Severities */
  auto svit = config.severities().begin();
  ASSERT_TRUE(svit != config.severities().end());
  ASSERT_TRUE(svit->severity_name() == "sev1");
  ASSERT_TRUE(svit->key().first == 3);
  ASSERT_TRUE(svit->level() == 14);
  ASSERT_TRUE(svit->icon_id() == 123);
  ASSERT_TRUE(svit->type() == configuration::severity::service);

  /* Serviceescalations */
  auto seit = config.serviceescalations().begin();
  ASSERT_TRUE(seit != config.serviceescalations().end());
  ASSERT_TRUE(seit->hosts().front() == "host_1");
  ASSERT_TRUE(seit->service_description().front() == "service_2");
  ++seit;
  ASSERT_TRUE(seit->hosts().begin() != seit->hosts().end());
  ASSERT_TRUE(seit->hosts().front() == "host_1");
  ASSERT_TRUE(*seit->contactgroups().begin() == "cg1");
  ASSERT_TRUE(seit->servicegroups().front() == "sg1");
  ASSERT_TRUE(seit->service_description().front() == "service_2");

  /*Hostescalations */
  auto heit = config.hostescalations().begin();
  ASSERT_TRUE(heit != config.hostescalations().end());
  std::set<std::string> cts{"cg1", "cg2"};
  ASSERT_TRUE(heit->contactgroups() == cts);
  ++heit;
  ASSERT_TRUE(heit->contactgroups() == cts);
  std::set<std::string> hgs{"hg1", "hg2"};
  ASSERT_TRUE(heit->hostgroups() == hgs);

  RmConf();
}

TEST_F(ApplierState, StateParsing) {
  configuration::State config;
  configuration::parser p;
  CreateConf();
  p.parse("/tmp/centengine.cfg", &config);
  ASSERT_EQ(config.check_service_freshness(), true);
  ASSERT_EQ(config.enable_flap_detection(), false);
  ASSERT_EQ(config.instance_heartbeat_interval(), 30);
  ASSERT_EQ(config.log_level_functions(), std::string("trace"));
  ASSERT_EQ(config.cfg_file().size(), CFG_FILES);
  ASSERT_EQ(config.resource_file().size(), RES_FILES);
  ASSERT_EQ(config.hosts().size(), HOSTS);
  ASSERT_EQ(config.hosts()[0].host_name(), std::string("host_1"));
  ASSERT_TRUE(config.hosts()[0].obj().register_());
  ASSERT_EQ(config.hosts()[0].host_id(), 1);
  ASSERT_EQ(config.hosts()[1].host_name(), std::string("host_2"));
  ASSERT_TRUE(config.hosts()[1].obj().register_());
  ASSERT_EQ(config.hosts()[1].host_id(), 2);
  ASSERT_EQ(config.hosts()[2].host_name(), std::string("host_3"));
  ASSERT_TRUE(config.hosts()[2].obj().register_());
  ASSERT_EQ(config.hosts()[2].host_id(), 3);
  ASSERT_EQ(config.hosts()[3].host_name(), std::string("host_4"));
  ASSERT_TRUE(config.hosts()[3].obj().register_());
  ASSERT_EQ(config.hosts()[3].host_id(), 4);

  ASSERT_EQ(config.services().size(), SERVICES);
  ASSERT_EQ(config.services()[0].hosts().data().size(), 1u);
  ASSERT_EQ(config.services()[0].service_id(), 1);
  ASSERT_TRUE(config.services()[0].obj().register_());
  ASSERT_TRUE(config.services()[0].checks_active());
  ASSERT_EQ(config.services()[0].hosts().data()[0], std::string("host_1"));
  ASSERT_EQ(config.services()[0].service_description(),
            std::string("service_1"));
  ASSERT_EQ(config.services()[0].contactgroups().data().size(), 3u);
  EXPECT_EQ(config.services()[0].contactgroups().data()[0], std::string("cg1"));
  EXPECT_EQ(config.services()[0].contactgroups().data()[1], std::string("cg3"));
  EXPECT_EQ(config.services()[0].contactgroups().data()[2], std::string("cg2"));

  EXPECT_EQ(config.services()[0].hostgroups().data().size(), 2u);
  EXPECT_EQ(config.services()[0].hostgroups().data()[0], std::string("hg1"));
  EXPECT_EQ(config.services()[0].contacts().data().size(), 2u);
  EXPECT_EQ(config.services()[0].contacts().data()[0], std::string("contact1"));
  EXPECT_EQ(config.services()[0].notification_options(),
            configuration::action_svc_warning |
                configuration::action_svc_unknown |
                configuration::action_svc_critical);
  std::set<std::pair<uint64_t, uint16_t>> exp{
      {1, tag::servicegroup},     {2, tag::servicegroup},
      {3, tag::servicegroup},     {5, tag::servicegroup},
      {11, tag::servicecategory}, {12, tag::servicecategory},
      {13, tag::servicecategory}};
  std::set<std::pair<uint64_t, uint16_t>> res;
  for (auto& t : config.services()[0].tags()) {
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

  ASSERT_EQ(config.commands().size(), 8u);
  ASSERT_EQ(config.commands()[0].command_name(), std::string("command_1"));
  ASSERT_EQ(config.commands()[0].command_line(),
            std::string("/tmp/var/lib/centreon-engine/check.pl 1"));

  /* One command inherites from command_template */
  ASSERT_EQ(config.commands()[6].command_name(), std::string("command_6"));
  ASSERT_EQ(config.commands()[6].command_line(),
            std::string("/tmp/var/lib/centreon-engine/check.pl 6"));

  ASSERT_EQ(config.timeperiods().size(), TIMEPERIODS);
  EXPECT_EQ(config.timeperiods()[1].timeperiod_name(), std::string("24x6"));
  EXPECT_EQ(config.timeperiods()[1].alias(),
            std::string("24_Hours_A_Day,_7_Days_A_Week"));
  EXPECT_EQ(config.timeperiods()[1].timeranges().sunday().size(),
            1u);  // std::string("00:00-24:00"));
  EXPECT_EQ(config.timeperiods()[1].timeranges().sunday()[0].range_start(), 0);
  EXPECT_EQ(config.timeperiods()[1].timeranges().sunday()[0].range_end(),
            3600 * 24);
  EXPECT_EQ(config.timeperiods()[1].timeranges().monday().size(), 2u);
  EXPECT_EQ(config.timeperiods()[1].timeranges().monday()[0].range_start(),
            0);  // 00:00-08:00
  EXPECT_EQ(config.timeperiods()[1].timeranges().monday()[0].range_end(),
            3600 * 8);
  EXPECT_EQ(config.timeperiods()[1].timeranges().monday()[1].range_start(),
            3600 * 18);  // 18:00-24:00
  EXPECT_EQ(config.timeperiods()[1].timeranges().monday()[1].range_end(),
            3600 * 24);
  EXPECT_EQ(config.timeperiods()[1].timeranges().tuesday().size(), 1u);
  EXPECT_EQ(config.timeperiods()[1].timeranges().wednesday().size(), 1u);
  EXPECT_EQ(config.timeperiods()[1].timeranges().thursday().size(), 1u);
  EXPECT_EQ(config.timeperiods()[1].timeranges().friday().size(), 1u);
  EXPECT_EQ(config.timeperiods()[1].timeranges().saturday().size(), 1u);

  ASSERT_EQ(config.contacts().size(), CONTACTS);
  const auto& ct = config.contacts().at(0);
  EXPECT_EQ(ct.contact_name(), std::string("contact1"));
  EXPECT_TRUE(ct.can_submit_commands());
  EXPECT_TRUE(ct.host_notifications_enabled());
  EXPECT_EQ(ct.host_notification_options(), configuration::action_hst_none);
  EXPECT_TRUE(ct.retain_nonstatus_information());
  EXPECT_TRUE(ct.retain_status_information());
  EXPECT_TRUE(ct.service_notifications_enabled());
  EXPECT_EQ(ct.service_notification_options(), configuration::action_svc_none);
  EXPECT_EQ(ct.alias(), std::string("contact1"));
  EXPECT_EQ(ct.contactgroups().data().size(), 2u);
  EXPECT_EQ(ct.contactgroups().data().at(0), std::string("super_cgroup1"));
  EXPECT_EQ(ct.contactgroups().data().at(1), std::string("super_cgroup2"));

  ASSERT_EQ(config.hostgroups().size(), HOSTGROUPS);
  auto hgit = config.hostgroups().begin();
  while (hgit != config.hostgroups().end() && hgit->hostgroup_name() != "hg1")
    ++hgit;
  ASSERT_TRUE(hgit != config.hostgroups().end());
  const auto hg = *hgit;
  ASSERT_EQ(hg.hostgroup_id(), 1u);
  ASSERT_EQ(hg.hostgroup_name(), std::string("hg1"));
  ASSERT_EQ(hg.alias(), std::string("hg1"));
  ASSERT_EQ(hg.members().data().size(), 3u);
  {
    auto it = hg.members().data().begin();
    ASSERT_EQ(*it, std::string("host_1"));
    ++it;
    ASSERT_EQ(*it, std::string("host_2"));
    ++it;
    ASSERT_EQ(*it, std::string("host_3"));
  }
  ASSERT_EQ(hg.notes(), std::string("notes for hg1"));
  ASSERT_EQ(hg.notes_url(), std::string("notes url for hg1"));
  ASSERT_EQ(hg.action_url(), std::string("action url for hg1"));

  ASSERT_EQ(config.servicegroups().size(), SERVICEGROUPS);
  auto sgit = config.servicegroups().begin();
  while (sgit != config.servicegroups().end() &&
         sgit->servicegroup_name() != "sg2")
    ++sgit;
  ASSERT_TRUE(sgit != config.servicegroups().end());
  const auto sg = *sgit;
  ASSERT_EQ(sg.servicegroup_id(), 2u);
  ASSERT_EQ(sg.servicegroup_name(), std::string("sg2"));
  ASSERT_EQ(sg.alias(), std::string("sg2"));
  ASSERT_EQ(sg.members().data().size(), 3u);
  {
    auto it = sg.members().data().begin();
    ASSERT_EQ(it->first(), std::string("host_1"));
    ASSERT_EQ(it->second(), std::string("service_2"));
    ++it;
    ASSERT_EQ(it->first(), std::string("host_1"));
    ASSERT_EQ(it->second(), std::string("service_3"));
    ++it;
    ASSERT_EQ(it->first(), std::string("host_1"));
    ASSERT_EQ(it->second(), std::string("service_4"));
  }
  ASSERT_EQ(sg.notes(), std::string("notes for sg2"));
  ASSERT_EQ(sg.notes_url(), std::string("notes url for sg2"));
  ASSERT_EQ(sg.action_url(), std::string("action url for sg2"));

  auto sdit = config.servicedependencies().begin();
  while (sdit != config.servicedependencies().end() &&
         std::find(sdit->servicegroups().data().begin(),
                   sdit->servicegroups().data().end(),
                   "sg1") != sdit->servicegroups().data().end())
    ++sdit;
  ASSERT_TRUE(sdit != config.servicedependencies().end());
  ASSERT_TRUE(*sdit->hosts().data().begin() == std::string("host_1"));
  ASSERT_TRUE(*sdit->dependent_service_description().data().begin() ==
              std::string("service_1"));
  ASSERT_TRUE(*sdit->dependent_hosts().data().begin() == std::string("host_1"));
  ASSERT_FALSE(sdit->inherits_parent());
  ASSERT_EQ(sdit->execution_failure_options(),
            configuration::servicedependency::none);
  ASSERT_EQ(sdit->notification_failure_options(),
            configuration::servicedependency::none);

  // Anomalydetections
  auto adit = config.anomalydetections().begin();
  while (adit != config.anomalydetections().end() &&
         (adit->service_id() != 2001 || adit->host_id() != 1))
    ++adit;
  ASSERT_TRUE(adit != config.anomalydetections().end());
  ASSERT_TRUE(adit->service_description() == "service_ad2");
  ASSERT_EQ(adit->dependent_service_id(), 1);
  ASSERT_TRUE(adit->metric_name() == "metric2");
  ASSERT_EQ(adit->customvariables().size(), 1);
  ASSERT_EQ(adit->customvariables().at(0).value(),
            std::string("this_is_a_test"));
  ASSERT_EQ(adit->contactgroups().data().size(), 2);
  ASSERT_EQ(adit->contacts().data().size(), 1);
  ASSERT_EQ(adit->servicegroups().data().size(), 2);

  auto cgit = config.contactgroups().begin();
  ASSERT_TRUE(cgit != config.contactgroups().end());
  ASSERT_TRUE(cgit->contactgroup_name() == "cg1");
  ASSERT_TRUE(cgit->alias() == "cg1_a");
  ASSERT_EQ(cgit->members().data().size(), 2u);
  ASSERT_EQ(cgit->contactgroup_members().data().size(), 1u);

  ++cgit;
  ASSERT_TRUE(cgit != config.contactgroups().end());
  ASSERT_TRUE(cgit->contactgroup_name() == "cg2");
  ASSERT_TRUE(cgit->alias() == "cg2_a");
  ASSERT_EQ(cgit->members().data().size(), 2u);
  ASSERT_EQ(*cgit->members().data().begin(), "user1");
  ASSERT_EQ(cgit->contactgroup_members().data().size(), 1u);

  ++cgit;
  ASSERT_TRUE(cgit != config.contactgroups().end());
  ASSERT_TRUE(cgit->contactgroup_name() == "cg3");
  ASSERT_TRUE(cgit->alias() == "cg3_a");
  ASSERT_EQ(cgit->members().data().size(), 3u);
  {
    auto it = cgit->members().data().begin();
    ASSERT_EQ(*it, "user3");
    ++it;
    ASSERT_EQ(*it, "user1");
    ++it;
    ASSERT_EQ(*it, "user2");
  }
  ASSERT_EQ(cgit->contactgroup_members().data().size(), 1u);
  ASSERT_EQ(*cgit->contactgroup_members().data().begin(), "cg3");

  auto cnit = config.connectors().begin();
  ASSERT_TRUE(cnit != config.connectors().end());
  ASSERT_TRUE(cnit->connector_name() == "conn_name1");
  ASSERT_TRUE(cnit->connector_line() == "conn_line1");
  ++cnit;
  ASSERT_TRUE(cnit->connector_name() == "conn_name2");
  ASSERT_TRUE(cnit->connector_line() == "conn_line2");

  /* Severities */
  auto svit = config.severities().begin();
  ASSERT_TRUE(svit != config.severities().end());
  ASSERT_TRUE(svit->severity_name() == "sev1");
  ASSERT_TRUE(svit->key().id() == 3);
  ASSERT_TRUE(svit->level() == 14);
  ASSERT_TRUE(svit->icon_id() == 123);
  ASSERT_TRUE(svit->key().type() == configuration::severity::service);

  /* Serviceescalations */
  auto seit = config.serviceescalations().begin();
  ASSERT_TRUE(seit != config.serviceescalations().end());
  ASSERT_TRUE(*seit->hosts().data().begin() == "host_1");
  ASSERT_TRUE(*seit->service_description().data().begin() == "service_2");
  ++seit;
  ASSERT_TRUE(seit->hosts().data().begin() != seit->hosts().data().end());
  ASSERT_TRUE(*seit->hosts().data().begin() == "host_1");
  ASSERT_TRUE(*seit->contactgroups().data().begin() == "cg1");
  ASSERT_TRUE(*seit->servicegroups().data().begin() == "sg1");
  ASSERT_TRUE(*seit->service_description().data().begin() == "service_2");

  /*Hostescalations */
  auto heit = config.hostescalations().begin();
  ASSERT_TRUE(heit != config.hostescalations().end());
  std::set<std::string> cts{"cg1", "cg2"};
  std::set<std::string> he_cts;
  for (auto& cg : heit->contactgroups().data())
    he_cts.insert(cg);
  ASSERT_TRUE(he_cts == cts);
  ++heit;

  he_cts.clear();
  for (auto& cg : heit->contactgroups().data())
    he_cts.insert(cg);
  ASSERT_TRUE(he_cts == cts);

  std::set<std::string> hgs{"hg1", "hg2"};
  std::set<std::string> he_hgs;
  for (auto& hg : heit->hostgroups().data())
    he_hgs.insert(hg);
  ASSERT_TRUE(he_hgs == hgs);

  RmConf();
}

TEST_F(ApplierState, StateLegacyParsingServicegroupValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEGROUP);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingServicegroupValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEGROUP);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingTagValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::TAG);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingTagValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::TAG);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingServicedependencyValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEDEPENDENCY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingServicedependencyValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEDEPENDENCY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingAnomalydetectionValidityFailed) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::ANOMALYDETECTION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingAnomalydetectionValidityFailed) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::ANOMALYDETECTION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingContactgroupWithoutName) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingContactgroupWithoutName) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::CONTACTGROUP);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateParsingSeverityWithoutType) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SEVERITY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateParsingServiceescalationWithoutService) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::SERVICEESCALATION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingHostescalationWithoutHost) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::HOSTESCALATION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingHostescalationWithoutHost) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::HOSTESCALATION);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}

TEST_F(ApplierState, StateLegacyParsingHostdependencyWithoutHost) {
  configuration::state config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::HOSTDEPENDENCY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", config), std::exception);
}

TEST_F(ApplierState, StateParsingHostdependencyWithoutHost) {
  configuration::State config;
  configuration::parser p;
  CreateBadConf(ConfigurationObject::HOSTDEPENDENCY);
  ASSERT_THROW(p.parse("/tmp/centengine.cfg", &config), std::exception);
}
