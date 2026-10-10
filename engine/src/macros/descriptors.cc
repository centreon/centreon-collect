/**
 * Copyright 2026 Centreon
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

#include "com/centreon/engine/macros/descriptors.hh"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#include <fmt/format.h>

#include <array>
#include <list>
#include <string>

#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/macros/defines.hh"
#include "com/centreon/engine/macros/grab.hh"
#include "com/centreon/engine/macros/grab_value.hh"
#include "com/centreon/engine/macros/misc.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/servicegroup.hh"
#include "com/centreon/engine/shared.hh"
#include "common/timeperiods/timeperiod.hh"

namespace com::centreon::engine::macros {

namespace {

/**
 *  The state of a host in plain text.
 *
 *  @param[in] state The host state.
 *
 *  @return "UP", "DOWN" or "UNREACHABLE".
 */
std::string host_state_string(host::host_state state) {
  return std::string(host::tab_host_states[state].second);
}

/**
 *  The state of a service in plain text.
 *
 *  @param[in] state The service state.
 *
 *  @return "OK", "WARNING", "CRITICAL" or "UNKNOWN".
 */
std::string service_state_string(service::service_state state) {
  switch (state) {
    case service::state_ok:
      return "OK";
    case service::state_warning:
      return "WARNING";
    case service::state_critical:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

/**
 *  The names of the groups a host or a service belongs to.
 *
 *  @param[in] groups The groups of the object.
 *
 *  @return The names separated by commas.
 */
template <typename G>
std::string group_names(const std::list<G*>& groups) {
  std::string retval;
  for (G* g : groups) {
    if (g) {
      if (!retval.empty())
        retval.append(",");
      retval.append(g->get_group_name());
    }
  }
  return retval;
}

/**
 *  The name of the first group a host or a service belongs to.
 *
 *  @param[in] groups The groups of the object.
 *
 *  @return The name, empty if the object has no group.
 */
template <typename G>
std::string group_name(const std::list<G*>& groups) {
  if (groups.empty())
    return "";
  return groups.front()->get_group_name();
}

/**
 *  The names of the parents or of the children of a host.
 *
 *  @param[in] hosts The parents or the children.
 *
 *  @return The names separated by commas.
 */
template <typename M>
std::string host_names(const M& hosts) {
  std::string retval;
  for (const auto& [name, _] : hosts) {
    if (!retval.empty())
      retval.append(",");
    retval.append(name);
  }
  return retval;
}

/**
 *  Count the services of a host by state.
 *
 *  @param[in] hst Host object.
 *
 *  @return The totals in the order of the macros MACRO_TOTALHOSTSERVICES,
 *          MACRO_TOTALHOSTSERVICESOK, MACRO_TOTALHOSTSERVICESWARNING,
 *          MACRO_TOTALHOSTSERVICESUNKNOWN, MACRO_TOTALHOSTSERVICESCRITICAL.
 */
std::array<uint32_t, 5> count_host_services(const host& hst) {
  std::array<uint32_t, 5> retval{};
  for (auto& [key, svc] : hst.services) {
    if (svc) {
      ++retval[0];
      switch (svc->get_current_state()) {
        case service::state_ok:
          ++retval[1];
          break;
        case service::state_warning:
          ++retval[2];
          break;
        case service::state_unknown:
          ++retval[3];
          break;
        case service::state_critical:
          ++retval[4];
          break;
      }
    }
  }
  return retval;
}

/**
 *  One of the $TOTALHOSTSERVICES*$ macros.
 *
 *  @param[in] macro_id The macro id, from MACRO_TOTALHOSTSERVICES to
 *                      MACRO_TOTALHOSTSERVICESCRITICAL.
 *  @param[in] hst      Host object.
 *  @param[in] mac      Macro array.
 *
 *  @return The requested total.
 */
std::string host_total_services(uint32_t macro_id,
                                host& hst,
                                nagios_macros* mac) {
  // The cache in the macros does not know its host: it only holds the totals
  // of the context host, those of another host (on-demand macro) are
  // computed each time.
  if (&hst == mac->host_ptr) {
    if (mac->x[MACRO_TOTALHOSTSERVICES].empty()) {
      std::array<uint32_t, 5> totals = count_host_services(hst);
      for (size_t i = 0; i < totals.size(); ++i)
        mac->x[MACRO_TOTALHOSTSERVICES + i] = fmt::to_string(totals[i]);
    }
    return mac->x[macro_id];
  }
  return fmt::to_string(
      count_host_services(hst)[macro_id - MACRO_TOTALHOSTSERVICES]);
}

/**
 *  The members of a hostgroup.
 *
 *  @param[in] hg The hostgroup.
 *
 *  @return The host names separated by commas.
 */
std::string hostgroup_members(const hostgroup& hg) {
  std::string retval;
  for (auto& [name, _] : hg.members) {
    if (name.empty())
      continue;
    if (!retval.empty())
      retval.append(",");
    retval.append(name);
  }
  return retval;
}

/**
 *  The members of a servicegroup.
 *
 *  @param[in] sg The servicegroup.
 *
 *  @return The host names and service descriptions, all separated by commas.
 */
std::string servicegroup_members(const servicegroup& sg) {
  std::string retval;
  for (auto& [key, _] : sg.members) {
    if (key.first.empty() || key.second.empty())
      continue;
    if (!retval.empty())
      retval.append(",");
    absl::StrAppend(&retval, key.first, ",", key.second);
  }
  return retval;
}

/**
 *  The names of the groups a contact belongs to.
 *
 *  @param[in] cntct The contact.
 *
 *  @return The names separated by commas.
 */
std::string contact_group_names(const contact& cntct) {
  std::string retval;
  for (auto& [name, cg] : cntct.get_parent_groups()) {
    if (!cg)
      continue;
    if (!retval.empty())
      retval.append(",");
    retval.append(name);
  }
  return retval;
}

/**
 *  The name of the first group a contact belongs to.
 *
 *  @param[in] cntct The contact.
 *
 *  @return The name, empty if the contact has no group.
 */
std::string contact_group_name(const contact& cntct) {
  if (cntct.get_parent_groups().empty())
    return "";
  return cntct.get_parent_groups().begin()->first;
}

/**
 *  The members of a contactgroup.
 *
 *  @param[in] cg The contactgroup.
 *
 *  @return The contact names separated by commas.
 */
std::string contactgroup_members(const contactgroup& cg) {
  std::string retval;
  for (auto& [name, cntct] : cg.get_members()) {
    if (cntct->get_name().empty())
      continue;
    if (!retval.empty())
      retval.append(",");
    retval.append(name);
  }
  return retval;
}

/**
 *  Format a date.
 *
 *  @param[in] when The date.
 *  @param[in] type LONG_DATE_TIME, SHORT_DATE_TIME, SHORT_DATE or SHORT_TIME.
 *
 *  @return The formatted date.
 */
std::string datetime_string(time_t when, int type) {
  char buffer[MAX_DATETIME_LENGTH];
  get_datetime_string(&when, buffer, MAX_DATETIME_LENGTH, type);
  return buffer;
}

/**
 *  The next valid time of a timeperiod.
 *
 *  @param[in] tp   The timeperiod.
 *  @param[in] when The time to start from.
 *
 *  @return The next valid time, formatted as SHORT_TIME.
 */
std::string next_valid_time_string(const common::timeperiods::timeperiod& tp,
                                   time_t when) {
  time_t next_valid_time = tp.get_next_valid_time(when);
  if (next_valid_time == when && !tp.check_time_against_period(when))
    next_valid_time = 0;
  return datetime_string(next_valid_time, SHORT_TIME);
}

/**
 *  One of the $TOTAL*$ macros, computed for all of them at the first use.
 *
 *  @param[in] macro_id The macro id.
 *  @param[in] mac      Macro array, where they are cached.
 *
 *  @return The requested total.
 */
std::string summary_value(uint32_t macro_id, nagios_macros* mac) {
  generate_summary_macros(mac);
  return mac->x[macro_id];
}

/* A getter computing an expression from the target t and the macros mac. */
#define G(...)                                                  \
  [](const macro_target& t [[maybe_unused]], nagios_macros* mac \
     [[maybe_unused]]) -> std::string { return __VA_ARGS__; }

/* The macro name is built from its id, so they cannot be out of sync. */
#define D(name, fam, cleaned, ...)                            \
  descriptor {                                                \
    MACRO_##name, #name, family::fam, cleaned, G(__VA_ARGS__) \
  }

/* Notification macros: computed before the notification is sent. */
#define NOTIFICATION(name, cleaned) \
  D(name, notification, cleaned, mac->x[MACRO_##name])

/* Static macros: set when the configuration is applied. */
#define STATIC(name) \
  D(name, statics, false, get_global_macros()->x[MACRO_##name])

/* Summary macros: computed at the first use, then cached. */
#define SUMMARY(name) D(name, summary, false, summary_value(MACRO_##name, mac))

/* All the macros stored in nagios_macros::x[], sorted by id. */
// clang-format off
constexpr std::array descriptors{
    D(HOSTNAME, host, false, t.hst->name()),
    D(HOSTALIAS, host, false, t.hst->get_alias()),
    D(HOSTADDRESS, host, false, t.hst->get_address()),
    D(SERVICEDESC, service, false, t.svc->description()),
    D(SERVICESTATE, service, false,
      service_state_string(t.svc->get_current_state())),
    D(SERVICESTATEID, service, false,
      to_macro_string(t.svc->get_current_state())),
    D(SERVICEATTEMPT, service, false,
      to_macro_string(t.svc->get_current_attempt())),
    D(LONGDATETIME, datetime, false, datetime_string(t.when, LONG_DATE_TIME)),
    D(SHORTDATETIME, datetime, false, datetime_string(t.when, SHORT_DATE_TIME)),
    D(DATE, datetime, false, datetime_string(t.when, SHORT_DATE)),
    D(TIME, datetime, false, datetime_string(t.when, SHORT_TIME)),
    D(TIMET, datetime, false, fmt::to_string(t.when)),
    D(LASTHOSTCHECK, host, false, to_macro_string(t.hst->get_last_check())),
    D(LASTSERVICECHECK, service, false,
      to_macro_string(t.svc->get_last_check())),
    D(LASTHOSTSTATECHANGE, host, false,
      to_macro_string(t.hst->get_last_state_change())),
    D(LASTSERVICESTATECHANGE, service, false,
      to_macro_string(t.svc->get_last_state_change())),
    D(HOSTOUTPUT, host, true, t.hst->get_plugin_output()),
    D(SERVICEOUTPUT, service, true, t.svc->get_plugin_output()),
    D(HOSTPERFDATA, host, true, t.hst->get_perf_data()),
    D(SERVICEPERFDATA, service, true, t.svc->get_perf_data()),
    D(CONTACTNAME, contact, false, t.cntct->get_name()),
    D(CONTACTALIAS, contact, false, t.cntct->get_alias()),
    D(CONTACTEMAIL, contact, false, t.cntct->get_email()),
    D(CONTACTPAGER, contact, false, t.cntct->get_pager()),
    STATIC(ADMINEMAIL),
    STATIC(ADMINPAGER),
    D(HOSTSTATE, host, false, host_state_string(t.hst->get_current_state())),
    D(HOSTSTATEID, host, false, to_macro_string(t.hst->get_current_state())),
    D(HOSTATTEMPT, host, false, to_macro_string(t.hst->get_current_attempt())),
    NOTIFICATION(NOTIFICATIONTYPE, false),
    NOTIFICATION(NOTIFICATIONNUMBER, false),
    D(HOSTEXECUTIONTIME, host, false,
      fixed_string(t.hst->get_execution_time(), 3)),
    D(SERVICEEXECUTIONTIME, service, false,
      fixed_string(t.svc->get_execution_time(), 3)),
    D(HOSTLATENCY, host, false, fixed_string(t.hst->get_latency(), 3)),
    D(SERVICELATENCY, service, false, fixed_string(t.svc->get_latency(), 3)),
    D(HOSTDURATION, host, false, duration_string(*t.hst)),
    D(SERVICEDURATION, service, false, duration_string(*t.svc)),
    D(HOSTDURATIONSEC, host, false, duration_sec_string(*t.hst)),
    D(SERVICEDURATIONSEC, service, false, duration_sec_string(*t.svc)),
    D(HOSTDOWNTIME, host, false,
      to_macro_string(t.hst->get_scheduled_downtime_depth())),
    D(SERVICEDOWNTIME, service, false,
      to_macro_string(t.svc->get_scheduled_downtime_depth())),
    D(HOSTSTATETYPE, host, false, state_type_string(*t.hst)),
    D(SERVICESTATETYPE, service, false, state_type_string(*t.svc)),
    D(HOSTPERCENTCHANGE, host, false,
      fixed_string(t.hst->get_percent_state_change(), 2)),
    D(SERVICEPERCENTCHANGE, service, false,
      fixed_string(t.svc->get_percent_state_change(), 2)),
    D(HOSTGROUPNAME, host, false, group_name(t.hst->get_parent_groups())),
    D(HOSTGROUPALIAS, hostgroup, false, t.hg->get_alias()),
    D(SERVICEGROUPNAME, service, false, group_name(t.svc->get_parent_groups())),
    D(SERVICEGROUPALIAS, servicegroup, false, t.sg->get_alias()),
    D(LASTSERVICEOK, service, false,
      to_macro_string(t.svc->get_last_time_ok())),
    D(LASTSERVICEWARNING, service, false,
      to_macro_string(t.svc->get_last_time_warning())),
    D(LASTSERVICEUNKNOWN, service, false,
      to_macro_string(t.svc->get_last_time_unknown())),
    D(LASTSERVICECRITICAL, service, false,
      to_macro_string(t.svc->get_last_time_critical())),
    D(LASTHOSTUP, host, false, to_macro_string(t.hst->get_last_time_up())),
    D(LASTHOSTDOWN, host, false, to_macro_string(t.hst->get_last_time_down())),
    D(LASTHOSTUNREACHABLE, host, false,
      to_macro_string(t.hst->get_last_time_unreachable())),
    D(SERVICECHECKCOMMAND, service, false, t.svc->check_command()),
    D(HOSTCHECKCOMMAND, host, false, t.hst->check_command()),
    STATIC(MAINCONFIGFILE),
    STATIC(STATUSDATAFILE),
    D(HOSTDISPLAYNAME, host, false, t.hst->get_display_name()),
    D(SERVICEDISPLAYNAME, service, false, t.svc->get_display_name()),
    STATIC(RETENTIONDATAFILE),
    STATIC(TEMPFILE),
    STATIC(LOGFILE),
    STATIC(RESOURCEFILE),
    STATIC(COMMANDFILE),
    STATIC(HOSTPERFDATAFILE),
    STATIC(SERVICEPERFDATAFILE),
    D(HOSTACTIONURL, host, false,
      recursive_string(mac, t.hst->get_action_url())),
    D(HOSTNOTESURL, host, false, recursive_string(mac, t.hst->get_notes_url())),
    D(HOSTNOTES, host, false, recursive_string(mac, t.hst->get_notes())),
    D(SERVICEACTIONURL, service, false,
      recursive_string(mac, t.svc->get_action_url())),
    D(SERVICENOTESURL, service, false,
      recursive_string(mac, t.svc->get_notes_url())),
    D(SERVICENOTES, service, false, recursive_string(mac, t.svc->get_notes())),
    SUMMARY(TOTALHOSTSUP),
    SUMMARY(TOTALHOSTSDOWN),
    SUMMARY(TOTALHOSTSUNREACHABLE),
    SUMMARY(TOTALHOSTSDOWNUNHANDLED),
    SUMMARY(TOTALHOSTSUNREACHABLEUNHANDLED),
    SUMMARY(TOTALHOSTPROBLEMS),
    SUMMARY(TOTALHOSTPROBLEMSUNHANDLED),
    SUMMARY(TOTALSERVICESOK),
    SUMMARY(TOTALSERVICESWARNING),
    SUMMARY(TOTALSERVICESCRITICAL),
    SUMMARY(TOTALSERVICESUNKNOWN),
    SUMMARY(TOTALSERVICESWARNINGUNHANDLED),
    SUMMARY(TOTALSERVICESCRITICALUNHANDLED),
    SUMMARY(TOTALSERVICESUNKNOWNUNHANDLED),
    SUMMARY(TOTALSERVICEPROBLEMS),
    SUMMARY(TOTALSERVICEPROBLEMSUNHANDLED),
    STATIC(PROCESSSTARTTIME),
    D(HOSTCHECKTYPE, host, false, check_type_string(*t.hst)),
    D(SERVICECHECKTYPE, service, false, check_type_string(*t.svc)),
    D(LONGHOSTOUTPUT, host, true, t.hst->get_long_plugin_output()),
    D(LONGSERVICEOUTPUT, service, true, t.svc->get_long_plugin_output()),
    STATIC(TEMPPATH),
    D(HOSTNOTIFICATIONNUMBER, host, false,
      to_macro_string(t.hst->get_notification_number())),
    D(SERVICENOTIFICATIONNUMBER, service, false,
      to_macro_string(t.svc->get_notification_number())),
    D(HOSTNOTIFICATIONID, host, false,
      to_macro_string(t.hst->get_current_notification_id())),
    D(SERVICENOTIFICATIONID, service, false,
      to_macro_string(t.svc->get_current_notification_id())),
    D(HOSTEVENTID, host, false, to_macro_string(t.hst->get_current_event_id())),
    D(LASTHOSTEVENTID, host, false,
      to_macro_string(t.hst->get_last_event_id())),
    D(SERVICEEVENTID, service, false,
      to_macro_string(t.svc->get_current_event_id())),
    D(LASTSERVICEEVENTID, service, false,
      to_macro_string(t.svc->get_last_event_id())),
    D(HOSTGROUPNAMES, host, false, group_names(t.hst->get_parent_groups())),
    D(SERVICEGROUPNAMES, service, false,
      group_names(t.svc->get_parent_groups())),
    D(MAXHOSTATTEMPTS, host, false,
      to_macro_string(t.hst->max_check_attempts())),
    D(MAXSERVICEATTEMPTS, service, false,
      to_macro_string(t.svc->max_check_attempts())),
    D(SERVICEISVOLATILE, service, false,
      to_macro_string(t.svc->get_is_volatile())),
    D(TOTALHOSTSERVICES, host, false,
      host_total_services(MACRO_TOTALHOSTSERVICES, *t.hst, mac)),
    D(TOTALHOSTSERVICESOK, host, false,
      host_total_services(MACRO_TOTALHOSTSERVICESOK, *t.hst, mac)),
    D(TOTALHOSTSERVICESWARNING, host, false,
      host_total_services(MACRO_TOTALHOSTSERVICESWARNING, *t.hst, mac)),
    D(TOTALHOSTSERVICESUNKNOWN, host, false,
      host_total_services(MACRO_TOTALHOSTSERVICESUNKNOWN, *t.hst, mac)),
    D(TOTALHOSTSERVICESCRITICAL, host, false,
      host_total_services(MACRO_TOTALHOSTSERVICESCRITICAL, *t.hst, mac)),
    D(HOSTGROUPNOTES, hostgroup, false,
      recursive_string(mac, t.hg->get_notes())),
    D(HOSTGROUPNOTESURL, hostgroup, false,
      recursive_string(mac, t.hg->get_notes_url())),
    D(HOSTGROUPACTIONURL, hostgroup, false,
      recursive_string(mac, t.hg->get_action_url())),
    D(SERVICEGROUPNOTES, servicegroup, false,
      recursive_string(mac, t.sg->get_notes())),
    D(SERVICEGROUPNOTESURL, servicegroup, false,
      recursive_string(mac, t.sg->get_notes_url())),
    D(SERVICEGROUPACTIONURL, servicegroup, false,
      recursive_string(mac, t.sg->get_action_url())),
    D(HOSTGROUPMEMBERS, hostgroup, false, hostgroup_members(*t.hg)),
    D(SERVICEGROUPMEMBERS, servicegroup, false, servicegroup_members(*t.sg)),
    D(CONTACTGROUPNAME, contact, false, contact_group_name(*t.cntct)),
    D(CONTACTGROUPALIAS, contactgroup, false, t.cg->get_alias()),
    D(CONTACTGROUPMEMBERS, contactgroup, false, contactgroup_members(*t.cg)),
    D(CONTACTGROUPNAMES, contact, false, contact_group_names(*t.cntct)),
    NOTIFICATION(NOTIFICATIONRECIPIENTS, false),
    NOTIFICATION(NOTIFICATIONISESCALATED, false),
    NOTIFICATION(NOTIFICATIONAUTHOR, true),
    NOTIFICATION(NOTIFICATIONAUTHORNAME, true),
    NOTIFICATION(NOTIFICATIONAUTHORALIAS, true),
    NOTIFICATION(NOTIFICATIONCOMMENT, true),
    STATIC(EVENTSTARTTIME),
    D(HOSTPROBLEMID, host, false,
      to_macro_string(t.hst->get_current_problem_id())),
    D(LASTHOSTPROBLEMID, host, false,
      to_macro_string(t.hst->get_last_problem_id())),
    D(SERVICEPROBLEMID, service, false,
      to_macro_string(t.svc->get_current_problem_id())),
    D(LASTSERVICEPROBLEMID, service, false,
      to_macro_string(t.svc->get_last_problem_id())),
    D(ISVALIDTIME, datetime, false,
      to_macro_string(!t.tp->check_time_against_period(t.when))),
    D(NEXTVALIDTIME, datetime, false, next_valid_time_string(*t.tp, t.when)),
    D(LASTHOSTSTATE, host, false, host_state_string(t.hst->get_last_state())),
    D(LASTHOSTSTATEID, host, false, to_macro_string(t.hst->get_last_state())),
    D(LASTSERVICESTATE, service, false,
      service_state_string(t.svc->get_last_state())),
    D(LASTSERVICESTATEID, service, false,
      to_macro_string(t.svc->get_last_state())),
    D(HOSTPARENTS, host, false, host_names(t.hst->parent_hosts)),
    D(HOSTCHILDREN, host, false, host_names(t.hst->child_hosts)),
    D(HOSTID, host, false, fmt::to_string(t.hst->host_id())),
    D(SERVICEID, service, false, fmt::to_string(t.svc->service_id())),
    D(HOSTTIMEZONE, host, false, t.hst->get_timezone()),
    D(SERVICETIMEZONE, service, false, t.svc->get_timezone()),
    D(CONTACTTIMEZONE, contact, false, t.cntct->get_timezone()),
    STATIC(POLLERNAME),
    STATIC(POLLERID),
};
// clang-format on

#undef SUMMARY
#undef STATIC
#undef NOTIFICATION
#undef D
#undef G

constexpr bool ids_are_sorted() {
  for (size_t i = 1; i < descriptors.size(); ++i)
    if (descriptors[i - 1].id >= descriptors[i].id)
      return false;
  return descriptors[descriptors.size() - 1].id < MACRO_X_COUNT;
}
static_assert(ids_are_sorted(),
              "macro descriptors must be sorted by id, without duplicates, "
              "and their ids must be lower than MACRO_X_COUNT");

}  // namespace

/**
 * @brief Find a macro from its name.
 *
 * @param name The macro name without its dollars, case sensitive.
 *
 * @return The macro descriptor, nullptr if the name is not one of the
 * macros stored in nagios_macros::x[].
 */
const descriptor* find_descriptor(std::string_view name) {
  static const absl::flat_hash_map<std::string_view, const descriptor*>
      by_name = [] {
        absl::flat_hash_map<std::string_view, const descriptor*> retval;
        retval.reserve(descriptors.size());
        for (auto& d : descriptors)
          retval.emplace(d.name, &d);
        return retval;
      }();
  auto found = by_name.find(name);
  return found == by_name.end() ? nullptr : found->second;
}

/**
 * @brief All the macros stored in nagios_macros::x[], sorted by id.
 *
 * @return A span over the descriptors.
 */
absl::Span<const descriptor> all_descriptors() {
  return absl::MakeConstSpan(descriptors);
}

}  // namespace com::centreon::engine::macros
