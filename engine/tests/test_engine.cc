/**
 * Copyright 2019-2022 Centreon (https://www.centreon.com/)
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

#include "test_engine.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

/* Just a hash map helper to store host id by their name. It is useful
 * to know if the host is already declared when creating a new service. */
static absl::flat_hash_map<std::string, uint64_t> conf_hosts;

configuration::Contact TestEngine::new_pb_configuration_contact(
    const std::string& name,
    bool full,
    const std::string& notif) const {
  if (full) {
    // Add command.
    {
      configuration::Command cmd;
      configuration::command_helper cmd_hlp(&cmd);
      cmd.set_command_name("cmd");
      cmd.set_command_line("true");
      configuration::applier::command aplyr;
      aplyr.add_object(cmd);
    }
    // Add timeperiod.
    {
      configuration::Timeperiod tperiod;
      configuration::timeperiod_helper tperiod_hlp(&tperiod);
      tperiod.set_timeperiod_name("24x7");
      tperiod.set_alias("24x7");
      auto* r = tperiod.mutable_timeranges()->add_monday();
      r->set_range_start(0);
      r->set_range_end(86400);
      r = tperiod.mutable_timeranges()->add_tuesday();
      r->set_range_start(0);
      r->set_range_end(86400);
      r = tperiod.mutable_timeranges()->add_wednesday();
      r->set_range_start(0);
      r->set_range_end(86400);
      r = tperiod.mutable_timeranges()->add_thursday();
      r->set_range_start(0);
      r->set_range_end(86400);
      r = tperiod.mutable_timeranges()->add_friday();
      r->set_range_start(0);
      r->set_range_end(86400);
      r = tperiod.mutable_timeranges()->add_saturday();
      r->set_range_start(0);
      r->set_range_end(86400);
      r = tperiod.mutable_timeranges()->add_sunday();
      r->set_range_start(0);
      r->set_range_end(86400);
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tperiod);
    }
  }
  // Valid contact configuration
  // (will generate 0 warnings or 0 errors).
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name(name);
  ctct.set_host_notification_period("24x7");
  ctct.set_service_notification_period("24x7");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd");
  fill_string_group(ctct.mutable_service_notification_commands(), "cmd");
  ctct_hlp.hook("host_notification_options", "d,r,f,s");
  ctct_hlp.hook("service_notification_options", notif);
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);
  return ctct;
}

void TestEngine::fill_pb_configuration_contact(
    configuration::contact_helper* ctct_hlp,
    const std::string& name,
    bool full,
    const std::string& notif) const {
  auto* ctct = static_cast<configuration::Contact*>(ctct_hlp->mut_obj());
  if (full) {
    // Add command.
    {
      configuration::Command cmd;
      configuration::command_helper cmd_hlp(&cmd);
      cmd.set_command_name("cmd");
      cmd.set_command_line("true");
      configuration::applier::command aplyr;
      aplyr.add_object(cmd);
    }
    // Add timeperiod.
    {
      configuration::Timeperiod tperiod;
      configuration::timeperiod_helper tperiod_hlp(&tperiod);
      tperiod.set_timeperiod_name("24x7");
      tperiod.set_alias("24x7");
      tperiod_hlp.hook("monday", "00:00-24:00");
      tperiod_hlp.hook("tuesday", "00:00-24:00");
      tperiod_hlp.hook("wednesday", "00:00-24:00");
      tperiod_hlp.hook("thursday", "00:00-24:00");
      tperiod_hlp.hook("friday", "00:00-24:00");
      tperiod_hlp.hook("saterday", "00:00-24:00");
      tperiod_hlp.hook("sunday", "00:00-24:00");
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tperiod);
    }
  }
  // Valid contact configuration
  // (will generate 0 warnings or 0 errors).
  ctct->set_contact_name(name);
  ctct->set_host_notification_period("24x7");
  ctct->set_service_notification_period("24x7");
  fill_string_group(ctct->mutable_host_notification_commands(), "cmd");
  fill_string_group(ctct->mutable_service_notification_commands(), "cmd");
  ctct_hlp->hook("host_notification_options", "d,r,f,s");
  ctct_hlp->hook("service_notification_options", notif);
  ctct->set_host_notifications_enabled(true);
  ctct->set_service_notifications_enabled(true);
}

configuration::Contactgroup TestEngine::new_pb_configuration_contactgroup(
    const std::string& name,
    const std::string& contactname) {
  configuration::Contactgroup retval;
  configuration::contactgroup_helper retval_hlp(&retval);
  fill_pb_configuration_contactgroup(&retval_hlp, name, contactname);
  return retval;
}

void TestEngine::fill_pb_configuration_contactgroup(
    configuration::contactgroup_helper* cg_hlp,
    const std::string& name,
    const std::string& contactname) {
  configuration::Contactgroup* cg =
      static_cast<configuration::Contactgroup*>(cg_hlp->mut_obj());
  cg->set_contactgroup_name(name);
  cg->set_alias(name);
  fill_string_group(cg->mutable_members(), contactname);
}

configuration::Serviceescalation
TestEngine::new_pb_configuration_serviceescalation(
    const std::string& hostname,
    const std::string& svc_desc,
    const std::string& contactgroup) {
  configuration::Serviceescalation se;
  configuration::serviceescalation_helper se_hlp(&se);
  se.set_first_notification(2);
  se.set_last_notification(11);
  se.set_notification_interval(9);
  se_hlp.hook("escalation_options", "w,u,c,r");
  se_hlp.hook("host_name", hostname);
  se_hlp.hook("service_description", svc_desc);
  se_hlp.hook("contact_groups", contactgroup);
  return se;
}

/**
 * @brief Create a new host dependency protobuf configuration.
 *
 * @param hostname      The master host name we work with.
 * @param dep_hostname  The dependent host.
 *
 * @return the new configuration as a configuration::Hostdependency.
 */
configuration::Hostdependency TestEngine::new_pb_configuration_hostdependency(
    const std::string& hostname,
    const std::string& dep_hostname) {
  configuration::Hostdependency hd;
  configuration::hostdependency_helper hd_hlp(&hd);
  EXPECT_TRUE(hd_hlp.hook("master_host", hostname));
  EXPECT_TRUE(hd_hlp.hook("dependent_host", dep_hostname));
  EXPECT_TRUE(hd_hlp.hook("notification_failure_options", "u,d"));
  hd.set_inherits_parent(true);
  hd.set_dependency_type(
      configuration::DependencyKind::notification_dependency);
  return hd;
}

configuration::Host TestEngine::new_pb_configuration_host(
    const std::string& hostname,
    const std::string& contacts,
    uint64_t hst_id) {
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name(hostname);
  hst.set_address("127.0.0.1");
  hst.set_host_id(hst_id);
  fill_string_group(hst.mutable_contacts(), contacts);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("hcmd");
  cmd.set_command_line("echo 0");
  hst.set_check_command("hcmd");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);

  conf_hosts[hostname] = hst_id;
  return hst;
}

void TestEngine::fill_pb_configuration_host(configuration::host_helper* hst_hlp,
                                            const std::string& hostname,
                                            const std::string& contacts,
                                            uint64_t hst_id) {
  auto* hst = static_cast<configuration::Host*>(hst_hlp->mut_obj());
  hst->set_host_name(hostname);
  hst->set_address("127.0.0.1");
  hst->set_host_id(hst_id);
  fill_string_group(hst->mutable_contacts(), contacts);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("hcmd");
  cmd.set_command_line("echo 0");
  hst->set_check_command("hcmd");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);

  conf_hosts[hostname] = hst_id;
}

configuration::Hostescalation TestEngine::new_pb_configuration_hostescalation(
    const std::string& hostname,
    const std::string& contactgroup,
    uint32_t first_notif,
    uint32_t last_notif,
    uint32_t interval_notif) {
  configuration::Hostescalation he;
  configuration::hostescalation_helper he_hlp(&he);
  he.set_first_notification(first_notif);
  he.set_last_notification(last_notif);
  he.set_notification_interval(interval_notif);
  he_hlp.hook("escalation_options", "d,u,r");
  he_hlp.hook("host_name", hostname);
  he_hlp.hook("contact_groups", contactgroup);
  return he;
}

configuration::Service TestEngine::new_pb_configuration_service(
    const std::string& hostname,
    const std::string& description,
    const std::string& contacts,
    uint64_t svc_id) {
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name(hostname);
  svc.set_service_description(description);
  auto it = conf_hosts.find(hostname);
  if (it != conf_hosts.end())
    svc.set_host_id(it->second);
  else
    svc.set_host_id(12);
  svc.set_service_id(svc_id);
  fill_string_group(svc.mutable_contacts(), contacts);

  // We fake here the expand_object on configuration::service
  if (it != conf_hosts.end())
    svc.set_host_id(it->second);
  else
    svc.set_host_id(12);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 'output| metric=$ARG1$;50;75'");
  svc.set_check_command("cmd!12");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);

  return svc;
}

configuration::Anomalydetection
TestEngine::new_pb_configuration_anomalydetection(
    const std::string& hostname,
    const std::string& description,
    const std::string& contacts,
    uint64_t svc_id,
    uint64_t dependent_svc_id,
    const std::string& thresholds_file) {
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  ad.set_host_name(hostname);
  ad.set_service_description(description);
  ad.set_dependent_service_id(dependent_svc_id);
  ad.set_host_id(12);
  ad.set_service_id(svc_id);
  fill_string_group(ad.mutable_contacts(), contacts);
  ad.set_metric_name("metric");
  ad.set_internal_id(1234);
  ad.set_thresholds_file(thresholds_file);

  // We fake here the expand_object on configuration::service
  ad.set_host_id(12);

  return ad;
}
void TestEngine::fill_pb_configuration_service(
    configuration::service_helper* svc_hlp,
    const std::string& hostname,
    const std::string& description,
    const std::string& contacts,
    uint64_t svc_id) {
  auto* svc = static_cast<configuration::Service*>(svc_hlp->mut_obj());
  svc->set_host_name(hostname);
  svc->set_service_description(description);
  auto it = conf_hosts.find(hostname);
  // We fake here the expand_object on configuration::service
  if (it != conf_hosts.end())
    svc->set_host_id(it->second);
  else
    svc->set_host_id(12);
  svc->set_service_id(svc_id);
  fill_string_group(svc->mutable_contacts(), contacts);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 'output| metric=$ARG1$;50;75'");
  svc->set_check_command("cmd!12");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);
}

std::unique_ptr<timeperiod> TestEngine::new_timeperiod_with_timeranges(
    const std::string& name,
    const std::string& alias) {
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_timeperiod_name(name);
  tp.set_alias(alias);
#define add_day(day)                                \
  {                                                 \
    auto* d = tp.mutable_timeranges()->add_##day(); \
    d->set_range_start(0);                          \
    d->set_range_end(86400);                        \
  }
  add_day(sunday);
  add_day(monday);
  add_day(tuesday);
  add_day(wednesday);
  add_day(thursday);
  add_day(friday);
  add_day(saturday);

  auto tperiod = std::make_unique<timeperiod>(tp);
  return tperiod;
}
