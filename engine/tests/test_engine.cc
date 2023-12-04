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

#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/configuration/state.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

/* Just a hash map helper to store host id by their name. It is useful
 * to know if the host is already declared when creating a new service. */
static absl::flat_hash_map<std::string, uint64_t> conf_hosts;

configuration::contact TestEngine::new_configuration_contact(
    const std::string& name,
    bool full,
    const std::string& notif) const {
  if (full) {
    // Add command.
    {
      configuration::command cmd;
      cmd.parse("command_name", "cmd");
      cmd.parse("command_line", "true");
      configuration::applier::command aplyr;
      aplyr.add_object(cmd);
    }
    // Add timeperiod.
    {
      configuration::timeperiod tperiod;
      tperiod.parse("timeperiod_name", "24x7");
      tperiod.parse("alias", "24x7");
      tperiod.parse("monday", "00:00-24:00");
      tperiod.parse("tuesday", "00:00-24:00");
      tperiod.parse("wednesday", "00:00-24:00");
      tperiod.parse("thursday", "00:00-24:00");
      tperiod.parse("friday", "00:00-24:00");
      tperiod.parse("saterday", "00:00-24:00");
      tperiod.parse("sunday", "00:00-24:00");
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tperiod);
    }
  }
  // Valid contact configuration
  // (will generate 0 warnings or 0 errors).
  configuration::contact ctct;
  ctct.parse("contact_name", name.c_str());
  ctct.parse("host_notification_period", "24x7");
  ctct.parse("service_notification_period", "24x7");
  ctct.parse("host_notification_commands", "cmd");
  ctct.parse("service_notification_commands", "cmd");
  ctct.parse("host_notification_options", "d,r,f,s");
  ctct.parse("service_notification_options", notif.c_str());
  ctct.parse("host_notifications_enabled", "1");
  ctct.parse("service_notifications_enabled", "1");
  return ctct;
}

configuration::contactgroup TestEngine::new_configuration_contactgroup(
    const std::string& name,
    const std::string& contactname) {
  configuration::contactgroup cg;
  cg.parse("contactgroup_name", name.c_str());
  cg.parse("alias", name.c_str());
  cg.parse("members", contactname.c_str());
  return cg;
}

configuration::serviceescalation
TestEngine::new_configuration_serviceescalation(
    const std::string& hostname,
    const std::string& svc_desc,
    const std::string& contactgroup) {
  configuration::serviceescalation se;
  se.parse("first_notification", "2");
  se.parse("last_notification", "11");
  se.parse("notification_interval", "9");
  se.parse("escalation_options", "w,u,c,r");
  se.parse("host_name", hostname.c_str());
  se.parse("service_description", svc_desc.c_str());
  se.parse("contact_groups", contactgroup.c_str());
  return se;
}

configuration::hostdependency TestEngine::new_configuration_hostdependency(
    const std::string& hostname,
    const std::string& dep_hostname) {
  configuration::hostdependency hd;
  hd.parse("master_host", hostname.c_str());
  hd.parse("dependent_host", dep_hostname.c_str());
  hd.parse("notification_failure_options", "u,d");
  hd.parse("inherits_parent", "1");
  hd.dependency_type(configuration::hostdependency::notification_dependency);
  return hd;
}

configuration::servicedependency
TestEngine::new_configuration_servicedependency(
    const std::string& hostname,
    const std::string& service,
    const std::string& dep_hostname,
    const std::string& dep_service) {
  configuration::servicedependency sd;
  sd.parse("master_host", hostname.c_str());
  sd.parse("master_description", service.c_str());
  sd.parse("dependent_host", dep_hostname.c_str());
  sd.parse("dependent_description", dep_service.c_str());
  sd.parse("notification_failure_options", "u,w,c");
  sd.dependency_type(configuration::servicedependency::notification_dependency);
  return sd;
}

configuration::host TestEngine::new_configuration_host(
    const std::string& hostname,
    const std::string& contacts,
    uint64_t hst_id) {
  configuration::host hst;
  hst.parse("host_name", hostname.c_str());
  hst.parse("address", "127.0.0.1");
  hst.parse("_HOST_ID", std::to_string(hst_id).c_str());
  hst.parse("contacts", contacts.c_str());

  configuration::command cmd("hcmd");
  cmd.parse("command_line", "echo 0");
  hst.parse("check_command", "hcmd");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);

  conf_hosts[hostname] = hst_id;
  return hst;
}

configuration::hostescalation TestEngine::new_configuration_hostescalation(
    const std::string& hostname,
    const std::string& contactgroup,
    uint32_t first_notif,
    uint32_t last_notif,
    uint32_t interval_notif) {
  configuration::hostescalation he;
  he.parse("first_notification", std::to_string(first_notif).c_str());
  he.parse("last_notification", std::to_string(last_notif).c_str());
  he.parse("notification_interval", std::to_string(interval_notif).c_str());
  he.parse("escalation_options", "d,u,r");
  he.parse("host_name", hostname.c_str());
  he.parse("contact_groups", contactgroup.c_str());
  return he;
}

configuration::service TestEngine::new_configuration_service(
    const std::string& hostname,
    const std::string& description,
    const std::string& contacts,
    uint64_t svc_id) {
  configuration::service svc;
  svc.parse("host_name", hostname.c_str());
  svc.parse("description", description.c_str());
  auto it = conf_hosts.find(hostname);
  if (it != conf_hosts.end())
    svc.parse("_HOST_ID", std::to_string(it->second).c_str());
  else
    svc.parse("_HOST_ID", "12");
  svc.parse("_SERVICE_ID", std::to_string(svc_id).c_str());
  svc.parse("contacts", contacts.c_str());

  // We fake here the expand_object on configuration::service
  if (it != conf_hosts.end())
    svc.set_host_id(it->second);
  else
    svc.set_host_id(12);

  configuration::command cmd("cmd");
  cmd.parse("command_line", "echo 'output| metric=$ARG1$;50;75'");
  svc.parse("check_command", "cmd!12");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);

  return svc;
}

configuration::anomalydetection TestEngine::new_configuration_anomalydetection(
    const std::string& hostname,
    const std::string& description,
    const std::string& contacts,
    uint64_t svc_id,
    uint64_t dependent_svc_id,
    const std::string& thresholds_file) {
  configuration::anomalydetection ad;
  ad.parse("host_name", hostname.c_str());
  ad.parse("description", description.c_str());
  ad.parse("dependent_service_id", std::to_string(dependent_svc_id).c_str());
  ad.parse("_HOST_ID", "12");
  ad.parse("_SERVICE_ID", std::to_string(svc_id).c_str());
  ad.parse("contacts", contacts.c_str());
  ad.parse("metric_name", "metric");
  ad.parse("internal_id", "1234");
  ad.parse("thresholds_file", thresholds_file.c_str());

  // We fake here the expand_object on configuration::service
  ad.set_host_id(12);

  return ad;
}
