/**
 * Copyright 2017 - 2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/contact.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"
#include "common/notifications/contact_viability.hh"
#include "common/notifications/notification_types.hh"
#include "common/timeperiods/timezone.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::string;
using com::centreon::common::timeperiods::string_to_timezone;
namespace notifications = com::centreon::common::notifications;

contact_map contact::contacts;

/**************************************
 *                                     *
 *           Base properties           *
 *                                     *
 **************************************/

/**
 *  Get a single address.
 *
 *  @param[in] index  Address index (starting from 0).
 *
 *  @return The requested address.
 */
std::string const& contact::get_address(int index) const {
  return _addresses[index];
}

/**
 *  Get all addresses.
 *
 *  @return Array of addresses.
 */
std::vector<std::string> const& contact::get_addresses() const {
  return _addresses;
}

/**
 *  Set addresses.
 *
 *  @param[in] addresses  New addresses.
 */
void contact::set_addresses(std::vector<std::string>&& addresses) {
  _addresses = std::move(addresses);
}

/**
 *  Return the contact alias
 *
 *  @return a reference to the alias
 */
std::string const& contact::get_alias() const {
  return _alias;
}

/**
 *  Set alias
 *
 *  @param[in] alias  New alias.
 */
void contact::set_alias(std::string const& alias) {
  _alias = alias;
}

/**
 *  Check if contact can submit commands.
 *
 *  @return True if contact can submit commands.
 */
bool contact::get_can_submit_commands() const {
  return _can_submit_commands;
}

/**
 *  (Dis)Allow a contact to submit commands.
 *
 *  @param[in] can_submit  True to enable contact to send commands.
 */
void contact::set_can_submit_commands(bool can_submit) {
  _can_submit_commands = can_submit;
}

/**
 *  Return the contact email
 *
 *  @return a reference to the email
 */
std::string const& contact::get_email() const {
  return _email;
}

/**
 *  Set contact email.
 *
 *  @param[in] email  New email.
 */
void contact::set_email(std::string const& email) {
  _email = email;
}

/**
 *  Get the contact's modified attributes.
 *
 *  @return A bitmask, representing modified attributes.
 */
uint32_t contact::get_modified_attributes() const {
  return _modified_attributes;
}

/**
 *  Set the contact's modified attributes.
 *
 *  @param[in] attr  Modified attributes.
 */
void contact::set_modified_attributes(uint32_t attr) {
  _modified_attributes = attr;
}

/**
 *  Modify the contact's modified attributes with an OR operator.
 *
 *  @param[in] attr  Modified attributes to accumulate.
 */
void contact::add_modified_attributes(uint32_t attr) {
  _modified_attributes |= attr;
}

/**
 *  Return the contact name.
 *
 *  @return A reference to the name.
 */
std::string const& contact::get_name() const {
  return _name;
}

/**
 *  Set the contact name.
 *
 *  @param[in] name  New name.
 */
void contact::set_name(std::string const& name) {
  _name = name;
}

/**
 *  Return the contact pager
 *
 *  @return a reference to the pager
 */
std::string const& contact::get_pager() const {
  return _pager;
}

/**
 *  Set the pager.
 *
 *  @param[in] pager  New pager.
 */
void contact::set_pager(std::string const& pager) {
  _pager = pager;
}

/**
 *  Check if status info should be retained.
 *
 *  @return True if status info should be retained.
 */
bool contact::get_retain_status_information() const {
  return _retain_status_information;
}

/**
 *  Retain (or not) status info.
 *
 *  @param[in] retain  True to retain status info.
 */
void contact::set_retain_status_information(bool retain) {
  _retain_status_information = retain;
}

/**
 *  Check if non-status info should be retained.
 *
 *  @return True if non-status info should be retained.
 */
bool contact::get_retain_nonstatus_information() const {
  return _retain_nonstatus_information;
}

/**
 *  Retain (or not) non-status info.
 *
 *  @param[in] retain  True to retain non-status info.
 */
void contact::set_retain_nonstatus_information(bool retain) {
  _retain_nonstatus_information = retain;
}

/**
 *  Get timezone.
 *
 *  @return Contact timezone.
 */
std::string const& contact::get_timezone() const {
  return _timezone;
}

/**
 *  Set timezone.
 *
 *  @param[in] timezone  New contact timezone.
 */
void contact::set_timezone(std::string const& timezone) {
  _timezone = timezone;
}

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
// bool operator==(
//       contact const& obj1,
//       contact const& obj2) throw () {
//  return obj1.get_name() == obj2.get_name()
//         && obj1.get_alias() == obj2.get_alias()
//         && obj1.get_email() == obj2.get_email()
//         && obj1.get_pager() == obj2.get_pager()
//         && obj1.get_addresses() == obj2.get_addresses()
//         && obj1.get_host_notification_commands() ==
//         obj2.get_host_notification_commands()
//         && obj1.get_service_notification_commands() ==
//         obj2.get_service_notification_commands()
//         && obj1.notify_on(notifications::service_notification) ==
//         obj2.notify_on(notifications::service_notification)
//         && obj1.notify_on(notifications::host_notification) ==
//         obj2.notify_on(notifications::host_notification)
//         && obj1.get_host_notification_period() ==
//         obj2.get_host_notification_period()
//         && obj1.get_service_notification_period() ==
//         obj2.get_service_notification_period()
//         && obj1.get_host_notifications_enabled() ==
//         obj2.get_host_notifications_enabled()
//         && obj1.get_service_notifications_enabled() ==
//         obj2.get_service_notifications_enabled()
//         && obj1.get_can_submit_commands() == obj2.get_can_submit_commands()
//         && obj1.get_retain_status_information() ==
//         obj2.get_retain_status_information()
//         && obj1.get_retain_nonstatus_information() ==
//         obj2.get_retain_nonstatus_information()
//         && obj1.custom_variables == obj2.custom_variables
//         && obj1.get_last_host_notification() ==
//         obj2.get_last_host_notification()
//         && obj1.get_last_service_notification() ==
//         obj2.get_last_service_notification()
//         && obj1.get_modified_attributes() == obj2.get_modified_attributes()
//         && obj1.get_modified_host_attributes() ==
//         obj2.get_modified_host_attributes()
//         && obj1.get_modified_service_attributes() ==
//         obj2.get_modified_service_attributes();
//}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
// bool operator!=(
//       contact const& obj1,
//       contact const& obj2) throw () {
//  return !operator==(obj1, obj2);
//}

/**
 *  Dump contact content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The contact to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, contact const& obj) {
  std::string cg_name{obj.get_parent_groups().begin()->first};
  std::string hst_notif_str;
  if (obj.get_host_notification_period_ptr())
    hst_notif_str = obj.get_host_notification_period_ptr()->get_name();
  std::string svc_notif_str;
  if (obj.get_service_notification_period_ptr())
    svc_notif_str = obj.get_service_notification_period_ptr()->get_name();

  os << "contact {\n"
        "  name:                            "
     << obj.get_name()
     << "\n"
        "  alias:                           "
     << obj.get_alias()
     << "\n"
        "  email:                           "
     << obj.get_email()
     << "\n"
        "  pager:                           "
     << obj.get_pager()
     << "\n"
        "  address:                         ";
  std::vector<std::string> const& address(obj.get_addresses());
  for (unsigned int i(0); i < address.size(); ++i)
    os << address[i]
       << (i + 1 < address.size() && !address[i + 1].empty() ? ", " : "");
  os << (!address[0].empty() ? " \n" : "\"NULL\"\n");
  os << "  host_notification_commands:      ";
  for (std::shared_ptr<commands::command> const& cmd :
       obj.get_host_notification_commands())
    os << cmd->get_command_line() << " ; ";
  os << "\n  service_notification_commands:   ";
  for (std::shared_ptr<commands::command> const& cmd :
       obj.get_service_notification_commands())
    os << cmd->get_command_line() << " ; ";
  os << "\n"
        "  notify_on_service_unknown:         "
     << obj.notify_on(notifications::service_notification,
                      notifications::unknown)
     << "\n"
        "  notify_on_service_warning:         "
     << obj.notify_on(notifications::service_notification,
                      notifications::warning)
     << "\n"
        "  notify_on_service_critical:        "
     << obj.notify_on(notifications::service_notification,
                      notifications::critical)
     << "\n"
        "  notify_on_service_recovery:        "
     << obj.notify_on(notifications::service_notification, notifications::ok)
     << "\n"
        "  notify_on_service_flappingstart:   "
     << obj.notify_on(notifications::service_notification,
                      notifications::flappingstart)
     << "\n"
        "  notify_on_service_flappingstop:    "
     << obj.notify_on(notifications::service_notification,
                      notifications::flappingstop)
     << "\n"
        "  notify_on_service_flappingdisabled:"
     << obj.notify_on(notifications::service_notification,
                      notifications::flappingdisabled)
     << "\n"
        "  notify_on_service_downtime:        "
     << obj.notify_on(notifications::service_notification,
                      notifications::downtime)
     << "\n"
        "  notify_on_host_down:               "
     << obj.notify_on(notifications::host_notification, notifications::down)
     << "\n"
        "  notify_on_host_unreachable:        "
     << obj.notify_on(notifications::host_notification,
                      notifications::unreachable)
     << "\n"
        "  notify_on_host_recovery:           "
     << obj.notify_on(notifications::host_notification, notifications::up)
     << "\n"
        "  notify_on_host_flappingstart:      "
     << obj.notify_on(notifications::host_notification,
                      notifications::flappingstart)
     << "\n"
        "  notify_on_host_flappingstop:       "
     << obj.notify_on(notifications::host_notification,
                      notifications::flappingstop)
     << "\n"
        "  notify_on_host_flappingdisabled:   "
     << obj.notify_on(notifications::host_notification,
                      notifications::flappingdisabled)
     << "\n"
        "  notify_on_host_downtime:           "
     << obj.notify_on(notifications::host_notification, notifications::downtime)
     << "\n"
        "  host_notification_period:          "
     << obj.get_host_notification_period()
     << "\n"
        "  service_notification_period:       "
     << obj.get_service_notification_period()
     << "\n"
        "  host_notifications_enabled:        "
     << obj.get_host_notifications_enabled()
     << "\n"
        "  service_notifications_enabled:     "
     << obj.get_service_notifications_enabled()
     << "\n"
        "  can_submit_commands:               "
     << obj.get_can_submit_commands()
     << "\n"
        "  retain_status_information:         "
     << obj.get_retain_status_information()
     << "\n"
        "  retain_nonstatus_information:      "
     << obj.get_retain_nonstatus_information()
     << "\n"
        "  last_host_notification:            "
     << string::ctime(obj.get_last_host_notification())
     << "\n"
        "  last_service_notification:         "
     << string::ctime(obj.get_last_service_notification())
     << "\n"
        "  modified_attributes:               "
     << obj.get_modified_attributes()
     << "\n"
        "  modified_host_attributes:          "
     << obj.get_modified_host_attributes()
     << "\n"
        "  modified_service_attributes:       "
     << obj.get_modified_service_attributes()
     << "\n"
        "  host_notification_period_ptr:      "
     << hst_notif_str
     << "\n"
        "  service_notification_period_ptr:   "
     << svc_notif_str
     << "\n"
        "  contactgroups_ptr:                 "
     << cg_name
     << "\n"
        "  customvariables:                   ";
  for (auto const& cv : obj.get_custom_variables())
    os << cv.first << " ; ";
  os << "}\n";
  return os;
}

/**
 *  Add a new contact to the list in memory.
 *
 *  @param[in] name                          Contact name.
 *  @param[in] alias                         Contact alias.
 *  @param[in] email                         Email.
 *  @param[in] pager                         Pager.
 *  @param[in] addresses                     Contact addresses.
 *  @param[in] svc_notification_period       Service notification
 *                                           period.
 *  @param[in] host_notification_period      Host nofication period.
 *  @param[in] notify_service_ok             Contact can be notified
 *                                           when service is ok.
 *  @param[in] notify_service_critical       Contact can be notified
 *                                           when service is critical.
 *  @param[in] notify_service_warning        Contact can be notified
 *                                           when service is warning.
 *  @param[in] notify_service_unknown        Contact can be notified
 *                                           when service is unknown.
 *  @param[in] notify_service_flapping       Contact can be notified
 *                                           when service is flapping.
 *  @param[in] notify_sevice_downtime        Contact can be notified on
 *                                           service downtime.
 *  @param[in] notify_host_up                Contact can be notified
 *                                           when host is up.
 *  @param[in] notify_host_down              Contact can be notified
 *                                           when host is down.
 *  @param[in] notify_host_unreachable       Contact can be notified
 *                                           when host is unreachable.
 *  @param[in] notify_host_flapping          Contact can be notified
 *                                           when host is flapping.
 *  @param[in] notify_host_downtime          Contact can be notified on
 *                                           host downtime.
 *  @param[in] host_notifications_enabled    Are contact host
 *                                           notifications enabled ?
 *  @param[in] service_notifications_enabled Are contact service
 *                                           notifications enabled ?
 *  @param[in] can_submit_commands           Can user submit external
 *                                           commands ?
 *  @param[in] retain_status_information     Shall Engine retain contact
 *                                           status info ?
 *  @param[in] retain_nonstatus_information  Shell Engine retain contact
 *                                           non-status info ?
 *
 *  @return New contact object.
 */
std::shared_ptr<contact> add_contact(
    std::string const& name,
    std::string const& alias,
    std::string const& email,
    std::string const& pager,
    std::vector<std::string>&& addresses,
    std::string const& svc_notification_period,
    std::string const& host_notification_period,
    int notify_service_ok,
    int notify_service_critical,
    int notify_service_warning,
    int notify_service_unknown,
    int notify_service_flapping,
    int notify_service_downtime,
    int notify_host_up,
    int notify_host_down,
    int notify_host_unreachable,
    int notify_host_flapping,
    int notify_host_downtime,
    int host_notifications_enabled,
    int service_notifications_enabled,
    int can_submit_commands,
    int retain_status_information,
    int retain_nonstatus_information) {
  // Make sure we have the data we need.
  if (name.empty()) {
    config_logger->error("Error: Contact name is empty");
    return nullptr;
  }

  // Check if the contact already exist.
  if (contact::contacts.count(name)) {
    config_logger->error("Error: Contact '{}' has already been defined", name);
    return nullptr;
  }

  // Allocate memory for a new contact.
  auto obj = std::make_shared<contact>();

  try {
    // Duplicate vars.
    obj->set_name(name);
    obj->set_alias(alias.empty() ? name : alias);
    obj->set_email(email);
    obj->set_host_notification_period(host_notification_period);
    obj->set_pager(pager);
    obj->set_service_notification_period(svc_notification_period);

    obj->set_addresses(std::move(addresses));

    // Set remaining contact properties.
    obj->set_can_submit_commands(can_submit_commands > 0);
    obj->set_host_notifications_enabled(host_notifications_enabled > 0);
    obj->set_modified_attributes(MODATTR_NONE);
    obj->set_modified_host_attributes(MODATTR_NONE);
    obj->set_modified_service_attributes(MODATTR_NONE);
    obj->set_notify_on(
        notifications::host_notification,
        (notify_host_down > 0 ? notifications::down : notifications::none) |
            (notify_host_downtime > 0 ? notifications::downtime
                                      : notifications::none) |
            (notify_host_flapping > 0
                 ? (notifications::flappingstart | notifications::flappingstop |
                    notifications::flappingdisabled)
                 : notifications::none) |
            (notify_host_up > 0 ? notifications::up : notifications::none) |
            (notify_host_unreachable > 0 ? notifications::unreachable
                                         : notifications::none));
    obj->set_notify_on(
        notifications::service_notification,
        (notify_service_critical > 0 ? notifications::critical
                                     : notifications::none) |
            (notify_service_downtime > 0 ? notifications::downtime
                                         : notifications::none) |
            (notify_service_flapping > 0
                 ? (notifications::flappingstart | notifications::flappingstop |
                    notifications::flappingdisabled)
                 : notifications::none) |
            (notify_service_ok > 0 ? notifications::ok : notifications::none) |
            (notify_service_unknown > 0 ? notifications::unknown
                                        : notifications::none) |
            (notify_service_warning > 0 ? notifications::warning
                                        : notifications::none));
    obj->set_retain_nonstatus_information(retain_nonstatus_information > 0);
    obj->set_retain_status_information(retain_status_information > 0);
    obj->set_service_notifications_enabled(service_notifications_enabled > 0);
  } catch (...) {
    obj.reset();
  }

  return obj;
}

contact::contact()
    : _addresses(MAX_CONTACT_ADDRESSES),
      _can_submit_commands{false},
      _last_host_notification{0UL},
      _last_service_notification{0UL},
      _modified_attributes{0UL},
      _modified_host_attributes{0UL},
      _modified_service_attributes{0UL},
      _retain_status_information{false},
      _retain_nonstatus_information{false},
      _notify_on{0, 0},
      _host_notifications_enabled{false},
      _service_notifications_enabled{false},
      _host_notification_period_ptr{nullptr},
      _service_notification_period_ptr{nullptr} {}

contact::~contact() {}

void contact::set_notify_on(notifications::notifier_type type, uint32_t notif) {
  _notify_on[type] = notif;
}

void contact::add_notify_on(notifications::notifier_type type,
                            notifications::notification_flag notif) {
  _notify_on[type] |= notif;
}

void contact::remove_notify_on(notifications::notifier_type type,
                               notifications::notification_flag notif) {
  _notify_on[type] &= ~notif;
}

bool contact::notify_on(notifications::notifier_type type,
                        notifications::notification_flag notif) const {
  return _notify_on[type] & notif;
}

uint32_t contact::notify_on(notifications::notifier_type type) const {
  return _notify_on[type];
}

/**
 *  Get the host notification period.
 *
 *  @return A pointer to the host notification period.
 */
std::string const& contact::get_host_notification_period() const {
  return _host_notification_period;
}

/**
 *  Set the host notification period.
 *
 *  @param[in] tp  Pointer to the new host notification period.
 */
void contact::set_host_notification_period(std::string const& period) {
  _host_notification_period = period;
}

/**
 *  Get the last time a host notification was sent for this contact.
 *
 *  @return A timestamp.
 */
time_t contact::get_last_host_notification() const {
  return _last_host_notification;
}

/**
 *  Set the last time a host notification was sent.
 *
 *  @param[in] t  Timestamp.
 */
void contact::set_last_host_notification(time_t t) {
  _last_host_notification = t;
}

/**
 *  Get the modified host attributes.
 *
 *  @return A bitmask.
 */
unsigned long contact::get_modified_host_attributes() const {
  return _modified_host_attributes;
}

/**
 *  Set the modified host attributes.
 *
 *  @param[in] attr  Modified host attributes.
 */
void contact::set_modified_host_attributes(unsigned long attr) {
  _modified_host_attributes = attr;
}

/**
 *  Get the service notification period.
 *
 *  @return Pointer to the notification period.
 */
std::string const& contact::get_service_notification_period() const {
  return _service_notification_period;
}

/**
 *  Set service notification period.
 *
 *  @param[in] tp  Pointer to the new service notification period.
 */
void contact::set_service_notification_period(std::string const& period) {
  _service_notification_period = period;
}

/**
 *  Get the last time a service notification was sent.
 *
 *  @return Timestamp.
 */
time_t contact::get_last_service_notification() const {
  return _last_service_notification;
}

/**
 *  Set the last time a service notification was sent.
 *
 *  @param[in] t  Timestamp.
 */
void contact::set_last_service_notification(time_t t) {
  _last_service_notification = t;
}

/**
 *  Get modified service attributes.
 *
 *  @return A bitmask.
 */
unsigned long contact::get_modified_service_attributes() const {
  return _modified_service_attributes;
}

/**
 *  Set the service modified attributes.
 *
 *  @param[in] attr  Service modified attributes.
 */
void contact::set_modified_service_attributes(unsigned long attr) {
  _modified_service_attributes = attr;
}

bool contact::get_host_notifications_enabled() const {
  return _host_notifications_enabled;
}

void contact::set_host_notifications_enabled(bool enabled) {
  _host_notifications_enabled = enabled;
}

bool contact::get_service_notifications_enabled() const {
  return _service_notifications_enabled;
}

void contact::set_service_notifications_enabled(bool enabled) {
  _service_notifications_enabled = enabled;
}

std::list<std::shared_ptr<commands::command> > const&
contact::get_host_notification_commands() const {
  return _host_notification_commands;
}

std::list<std::shared_ptr<commands::command> >&
contact::get_host_notification_commands() {
  return _host_notification_commands;
}

std::list<std::shared_ptr<commands::command> > const&
contact::get_service_notification_commands() const {
  return _service_notification_commands;
}

std::list<std::shared_ptr<commands::command> >&
contact::get_service_notification_commands() {
  return _service_notification_commands;
}

std::ostream& operator<<(std::ostream& os, const contact_map& obj) {
  for (contact_map::const_iterator it = obj.begin(), end = obj.end(); it != end;
       ++it) {
    os << it->first;
    if (std::next(it) != end)
      os << ", ";
    else
      os << "";
  }
  return os;
}

contactgroup_map_unsafe const& contact::get_parent_groups() const {
  return _contactgroups;
}

contactgroup_map_unsafe& contact::get_parent_groups() {
  return _contactgroups;
}

/**
 *  Returns a boolean telling if this contact should be notified by a notifier
 *  with the given properties.
 *
 * @param type A service or a host.
 * @param cat The notification category
 * @param state The notifier current state
 *
 * @return true if the contact should be notified, false otherwise.
 */
bool contact::should_be_notified(notifications::notification_category cat,
                                 notifications::reason_type type,
                                 notifier const& notif) const {
  functions_logger->trace("contact::should_be_notified()");

  const notifications::notifier_type nt = notif.get_notifier_type();
  const bool is_host = nt == notifications::host_notification;

  /* Resolve the two environment-dependent inputs the shared viability function
   * cannot compute itself: whether the contact's notification period is
   * currently open (evaluated in the contact's own timezone; a missing period
   * means always-in), and, for a recovery, whether the contact was told about
   * the ongoing problem. */
  auto* tp = is_host ? get_host_notification_period_ptr()
                     : get_service_notification_period_ptr();
  const bool in_period =
      !tp || tp->check_time_against_period_for_notif(
                 std::time(nullptr), string_to_timezone(get_timezone()));

  bool already_notified = false;
  if (cat == notifications::cat_recovery) {
    const notifications::notification* normal_notif =
        notif.get_current_notifications()[notifications::cat_normal];
    already_notified = normal_notif && normal_notif->sent_to(get_name());
  }

  notifications::contact snapshot;
  snapshot.name = get_name();
  snapshot.host_notifications_enabled = _host_notifications_enabled;
  snapshot.service_notifications_enabled = _service_notifications_enabled;
  snapshot.host_notification_options = notify_on(notifications::host_notification);
  snapshot.service_notification_options =
      notify_on(notifications::service_notification);

  return notifications::should_notify_contact(
      snapshot, is_host, cat, type, notif.get_current_state_int(), in_period,
      already_notified);
}

timeperiod* contact::get_host_notification_period_ptr() const {
  return _host_notification_period_ptr;
}

void contact::set_host_notification_period_ptr(timeperiod* period) {
  _host_notification_period_ptr = period;
}

timeperiod* contact::get_service_notification_period_ptr() const {
  return _service_notification_period_ptr;
}

void contact::set_service_notification_period_ptr(timeperiod* period) {
  _service_notification_period_ptr = period;
}

map_customvar const& contact::get_custom_variables() const {
  return _custom_variables;
}

map_customvar& contact::get_custom_variables() {
  return _custom_variables;
}
