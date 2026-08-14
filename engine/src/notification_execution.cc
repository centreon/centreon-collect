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

#include "com/centreon/engine/notification_execution.hh"

#include <absl/algorithm/container.h>
#include <fmt/format.h>

#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/defines.hh"
#include "com/centreon/engine/neberrors.hh"

namespace notifications = com::centreon::common::notifications;

namespace com::centreon::engine {

namespace {
/**
 * @brief Attempt to get a resource by its logical id.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 *
 * @return The matching notifier, or nullptr if no such resource exists.
 */
notifier* get_resource(uint64_t host_id, uint64_t service_id) {
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    return it != host::hosts_by_id.end() ? it->second.get() : nullptr;
  }
  auto it = service::services_by_id.find({host_id, service_id});
  return it != service::services_by_id.end() ? it->second.get() : nullptr;
}
}  // namespace

/**
 * @brief Expand the notification macros and run the notification command for
 * each selected contact.
 *
 * Shared execution tail between the Engine-side deliver() (engine notification
 * mode, where this same process selected the contacts) and
 * execute_broker_notification() (broker notification mode, where Broker made
 * the decision and dispatched the execution here). The contact selection is NOT
 * done here; @p to_notify is already the exact set to reach.
 *
 * @param n The notifier the notification is about.
 * @param to_notify The contacts to notify (already selected/filtered).
 * @param type The notification reason (drives the $NOTIFICATIONTYPE$ macro).
 * @param notification_id The unique notification id (for macros).
 * @param notification_number The notification number (for macros).
 * @param escalated Whether the notification is escalated (for macros).
 * @param author The notification author.
 * @param message The notification message/comment.
 * @param options The notification options.
 *
 * @return The names of the contacts actually notified.
 */
absl::btree_set<std::string> run_notification_commands(
    notifier* n,
    const absl::flat_hash_set<std::shared_ptr<contact>>& to_notify,
    notifications::reason_type type,
    uint64_t notification_id,
    uint32_t notification_number,
    bool escalated,
    const std::string& author,
    const std::string& message,
    notifications::notification_option options) {
  absl::btree_set<std::string> notified;
  nagios_macros* mac(get_global_macros());

  /* Grab the macro variables */
  n->grab_macros_r(mac);

  /* The author is given by name; fall back to a lookup by alias. */
  contact* author_contact = nullptr;
  auto it = contact::contacts.find(author);
  if (it == contact::contacts.end())
    it = absl::c_find_if(contact::contacts, [&author](const auto& p) {
      return p.second->get_alias() == author;
    });
  if (it != contact::contacts.end())
    author_contact = it->second.get();

  /* Get author and comment macros */
  mac->x[MACRO_NOTIFICATIONAUTHOR] = author;
  mac->x[MACRO_NOTIFICATIONCOMMENT] = message;
  if (author_contact) {
    mac->x[MACRO_NOTIFICATIONAUTHORNAME] = author_contact->get_name();
    mac->x[MACRO_NOTIFICATIONAUTHORALIAS] = author_contact->get_alias();
  } else {
    mac->x[MACRO_NOTIFICATIONAUTHORNAME] = "";
    mac->x[MACRO_NOTIFICATIONAUTHORALIAS] = "";
  }

  /* set the notification type macro */
  switch (type) {
    case notifications::reason_acknowledgement:
      mac->x[MACRO_NOTIFICATIONTYPE] = "ACKNOWLEDGEMENT";
      break;
    case notifications::reason_flappingstart:
      mac->x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTART";
      break;
    case notifications::reason_flappingstop:
      mac->x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTOP";
      break;
    case notifications::reason_flappingdisabled:
      mac->x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGDISABLED";
      break;
    case notifications::reason_downtimestart:
      mac->x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMESTART";
      break;
    case notifications::reason_downtimeend:
      mac->x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMEEND";
      break;
    case notifications::reason_downtimecancelled:
      mac->x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMECANCELLED";
      break;
    case notifications::reason_custom:
      mac->x[MACRO_NOTIFICATIONTYPE] = "CUSTOM";
      break;
    case notifications::reason_recovery:
      mac->x[MACRO_NOTIFICATIONTYPE] = "RECOVERY";
      break;
    default:
      mac->x[MACRO_NOTIFICATIONTYPE] = "PROBLEM";
      break;
  }

  /* $NOTIFICATIONISESCALATED$ is historically "0"/"1"; cast the bool to int so
   * fmt does not render it as "true"/"false". */
  if (n->get_notifier_type() == notifications::host_notification) {
    mac->x[MACRO_HOSTNOTIFICATIONNUMBER] = fmt::to_string(notification_number);
    /* The $NOTIFICATIONNUMBER$ macro is maintained for backward compatibility
     */
    mac->x[MACRO_NOTIFICATIONNUMBER] = mac->x[MACRO_HOSTNOTIFICATIONNUMBER];
    mac->x[MACRO_NOTIFICATIONISESCALATED] =
        fmt::to_string(static_cast<int>(escalated));
    mac->x[MACRO_HOSTNOTIFICATIONID] = fmt::to_string(notification_id);
  } else {
    mac->x[MACRO_SERVICENOTIFICATIONNUMBER] =
        fmt::to_string(notification_number);
    mac->x[MACRO_NOTIFICATIONNUMBER] = mac->x[MACRO_SERVICENOTIFICATIONNUMBER];
    mac->x[MACRO_NOTIFICATIONISESCALATED] =
        fmt::to_string(static_cast<int>(escalated));
    mac->x[MACRO_SERVICENOTIFICATIONID] = fmt::to_string(notification_id);
  }

  for (const std::shared_ptr<contact>& ctc_ptr : to_notify) {
    contact* ctc = ctc_ptr.get();

    /* grab the macro variables for this contact */
    grab_contact_macros_r(mac, ctc);
    /* clear summary macros (they are customized for each contact) */
    clear_summary_macros_r(mac);

    if (n->notify_contact(mac, ctc, type, author, message, options,
                          escalated) == OK) {
      notified.insert(ctc->get_name());
      if (mac->x[MACRO_NOTIFICATIONRECIPIENTS].empty())
        mac->x[MACRO_NOTIFICATIONRECIPIENTS] = ctc->get_name();
      else {
        mac->x[MACRO_NOTIFICATIONRECIPIENTS].append(",");
        mac->x[MACRO_NOTIFICATIONRECIPIENTS].append(ctc->get_name());
      }
    }
  }
  return notified;
}

/**
 * @brief Execute a notification whose DECISION was made by Broker
 * (notification_mode=broker). Broker already selected the contacts and
 * dispatched the execution to this poller through a pb_notification_execute
 * event; here we only resolve the contact names to their runtime objects,
 * expand the macros and run the notification commands. A resource this poller
 * does not supervise is silently ignored.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 * @param type The notification reason.
 * @param notification_id The unique notification id (for macros).
 * @param notification_number The notification number (for macros).
 * @param escalated Whether the notification is escalated (for macros).
 * @param author The notification author.
 * @param message The notification message/comment.
 * @param options The notification options.
 * @param contacts The names of the contacts Broker selected.
 */
void execute_broker_notification(
    uint64_t host_id,
    uint64_t service_id,
    notifications::reason_type type,
    uint64_t notification_id,
    uint32_t notification_number,
    bool escalated,
    const std::string& author,
    const std::string& message,
    notifications::notification_option options,
    const ::google::protobuf::RepeatedPtrField<std::string>& contacts) {
  notifier* n = get_resource(host_id, service_id);
  if (!n)
    /* This poller does not supervise the resource: not for us. */
    return;

  absl::flat_hash_set<std::shared_ptr<contact>> to_notify;
  for (const std::string& name : contacts) {
    auto it = contact::contacts.find(name);
    if (it != contact::contacts.end())
      to_notify.insert(it->second);
  }

  run_notification_commands(n, to_notify, type, notification_id,
                            notification_number, escalated, author, message,
                            options);
}

}  // namespace com::centreon::engine
