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

#include <array>

#include "com/centreon/engine/macros/defines.hh"

namespace com::centreon::engine::macros {

namespace {

/* The macro name is built from its id, so they cannot be out of sync. */
#define D(name, fam, cleaned)                 \
  descriptor {                                \
    MACRO_##name, #name, family::fam, cleaned \
  }

/* All the macros stored in nagios_macros::x[], sorted by id. */
constexpr std::array descriptors{
    D(HOSTNAME, host, false),
    D(HOSTALIAS, host, false),
    D(HOSTADDRESS, host, false),
    D(SERVICEDESC, service, false),
    D(SERVICESTATE, service, false),
    D(SERVICESTATEID, service, false),
    D(SERVICEATTEMPT, service, false),
    D(LONGDATETIME, datetime, false),
    D(SHORTDATETIME, datetime, false),
    D(DATE, datetime, false),
    D(TIME, datetime, false),
    D(TIMET, datetime, false),
    D(LASTHOSTCHECK, host, false),
    D(LASTSERVICECHECK, service, false),
    D(LASTHOSTSTATECHANGE, host, false),
    D(LASTSERVICESTATECHANGE, service, false),
    D(HOSTOUTPUT, host, true),
    D(SERVICEOUTPUT, service, true),
    D(HOSTPERFDATA, host, true),
    D(SERVICEPERFDATA, service, true),
    D(CONTACTNAME, contact, false),
    D(CONTACTALIAS, contact, false),
    D(CONTACTEMAIL, contact, false),
    D(CONTACTPAGER, contact, false),
    D(ADMINEMAIL, statics, false),
    D(ADMINPAGER, statics, false),
    D(HOSTSTATE, host, false),
    D(HOSTSTATEID, host, false),
    D(HOSTATTEMPT, host, false),
    D(NOTIFICATIONTYPE, notification, false),
    D(NOTIFICATIONNUMBER, notification, false),
    D(HOSTEXECUTIONTIME, host, false),
    D(SERVICEEXECUTIONTIME, service, false),
    D(HOSTLATENCY, host, false),
    D(SERVICELATENCY, service, false),
    D(HOSTDURATION, host, false),
    D(SERVICEDURATION, service, false),
    D(HOSTDURATIONSEC, host, false),
    D(SERVICEDURATIONSEC, service, false),
    D(HOSTDOWNTIME, host, false),
    D(SERVICEDOWNTIME, service, false),
    D(HOSTSTATETYPE, host, false),
    D(SERVICESTATETYPE, service, false),
    D(HOSTPERCENTCHANGE, host, false),
    D(SERVICEPERCENTCHANGE, service, false),
    D(HOSTGROUPNAME, host, false),
    D(HOSTGROUPALIAS, hostgroup, false),
    D(SERVICEGROUPNAME, service, false),
    D(SERVICEGROUPALIAS, servicegroup, false),
    D(LASTSERVICEOK, service, false),
    D(LASTSERVICEWARNING, service, false),
    D(LASTSERVICEUNKNOWN, service, false),
    D(LASTSERVICECRITICAL, service, false),
    D(LASTHOSTUP, host, false),
    D(LASTHOSTDOWN, host, false),
    D(LASTHOSTUNREACHABLE, host, false),
    D(SERVICECHECKCOMMAND, service, false),
    D(HOSTCHECKCOMMAND, host, false),
    D(MAINCONFIGFILE, statics, false),
    D(STATUSDATAFILE, statics, false),
    D(HOSTDISPLAYNAME, host, false),
    D(SERVICEDISPLAYNAME, service, false),
    D(RETENTIONDATAFILE, statics, false),
    D(TEMPFILE, statics, false),
    D(LOGFILE, statics, false),
    D(RESOURCEFILE, statics, false),
    D(COMMANDFILE, statics, false),
    D(HOSTPERFDATAFILE, statics, false),
    D(SERVICEPERFDATAFILE, statics, false),
    D(HOSTACTIONURL, host, false),
    D(HOSTNOTESURL, host, false),
    D(HOSTNOTES, host, false),
    D(SERVICEACTIONURL, service, false),
    D(SERVICENOTESURL, service, false),
    D(SERVICENOTES, service, false),
    D(TOTALHOSTSUP, summary, false),
    D(TOTALHOSTSDOWN, summary, false),
    D(TOTALHOSTSUNREACHABLE, summary, false),
    D(TOTALHOSTSDOWNUNHANDLED, summary, false),
    D(TOTALHOSTSUNREACHABLEUNHANDLED, summary, false),
    D(TOTALHOSTPROBLEMS, summary, false),
    D(TOTALHOSTPROBLEMSUNHANDLED, summary, false),
    D(TOTALSERVICESOK, summary, false),
    D(TOTALSERVICESWARNING, summary, false),
    D(TOTALSERVICESCRITICAL, summary, false),
    D(TOTALSERVICESUNKNOWN, summary, false),
    D(TOTALSERVICESWARNINGUNHANDLED, summary, false),
    D(TOTALSERVICESCRITICALUNHANDLED, summary, false),
    D(TOTALSERVICESUNKNOWNUNHANDLED, summary, false),
    D(TOTALSERVICEPROBLEMS, summary, false),
    D(TOTALSERVICEPROBLEMSUNHANDLED, summary, false),
    D(PROCESSSTARTTIME, statics, false),
    D(HOSTCHECKTYPE, host, false),
    D(SERVICECHECKTYPE, service, false),
    D(LONGHOSTOUTPUT, host, true),
    D(LONGSERVICEOUTPUT, service, true),
    D(TEMPPATH, statics, false),
    D(HOSTNOTIFICATIONNUMBER, host, false),
    D(SERVICENOTIFICATIONNUMBER, service, false),
    D(HOSTNOTIFICATIONID, host, false),
    D(SERVICENOTIFICATIONID, service, false),
    D(HOSTEVENTID, host, false),
    D(LASTHOSTEVENTID, host, false),
    D(SERVICEEVENTID, service, false),
    D(LASTSERVICEEVENTID, service, false),
    D(HOSTGROUPNAMES, host, false),
    D(SERVICEGROUPNAMES, service, false),
    D(MAXHOSTATTEMPTS, host, false),
    D(MAXSERVICEATTEMPTS, service, false),
    D(SERVICEISVOLATILE, service, false),
    D(TOTALHOSTSERVICES, host, false),
    D(TOTALHOSTSERVICESOK, host, false),
    D(TOTALHOSTSERVICESWARNING, host, false),
    D(TOTALHOSTSERVICESUNKNOWN, host, false),
    D(TOTALHOSTSERVICESCRITICAL, host, false),
    D(HOSTGROUPNOTES, hostgroup, false),
    D(HOSTGROUPNOTESURL, hostgroup, false),
    D(HOSTGROUPACTIONURL, hostgroup, false),
    D(SERVICEGROUPNOTES, servicegroup, false),
    D(SERVICEGROUPNOTESURL, servicegroup, false),
    D(SERVICEGROUPACTIONURL, servicegroup, false),
    D(HOSTGROUPMEMBERS, hostgroup, false),
    D(SERVICEGROUPMEMBERS, servicegroup, false),
    D(CONTACTGROUPNAME, contact, false),
    D(CONTACTGROUPALIAS, contactgroup, false),
    D(CONTACTGROUPMEMBERS, contactgroup, false),
    D(CONTACTGROUPNAMES, contact, false),
    D(NOTIFICATIONRECIPIENTS, notification, false),
    D(NOTIFICATIONISESCALATED, notification, false),
    D(NOTIFICATIONAUTHOR, notification, true),
    D(NOTIFICATIONAUTHORNAME, notification, true),
    D(NOTIFICATIONAUTHORALIAS, notification, true),
    D(NOTIFICATIONCOMMENT, notification, true),
    D(EVENTSTARTTIME, statics, false),
    D(HOSTPROBLEMID, host, false),
    D(LASTHOSTPROBLEMID, host, false),
    D(SERVICEPROBLEMID, service, false),
    D(LASTSERVICEPROBLEMID, service, false),
    D(ISVALIDTIME, datetime, false),
    D(NEXTVALIDTIME, datetime, false),
    D(LASTHOSTSTATE, host, false),
    D(LASTHOSTSTATEID, host, false),
    D(LASTSERVICESTATE, service, false),
    D(LASTSERVICESTATEID, service, false),
    D(HOSTPARENTS, host, false),
    D(HOSTCHILDREN, host, false),
    D(HOSTID, host, false),
    D(SERVICEID, service, false),
    D(HOSTTIMEZONE, host, false),
    D(SERVICETIMEZONE, service, false),
    D(CONTACTTIMEZONE, contact, false),
    D(POLLERNAME, statics, false),
    D(POLLERID, statics, false),
};

#undef D

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
