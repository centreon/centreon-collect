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

#include "com/centreon/engine/engine_notification_callbacks.hh"

#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/defines.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/notifier.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "com/centreon/engine/timezone_locker.hh"

using namespace com::centreon::engine;

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
 * @brief Get the program-wide notification configuration.
 *
 * @return The global notification configuration snapshot.
 */
notifications::global_config engine_notification_callbacks::get_global_config()
    const {
  notifications::global_config gc;
  gc.enabled = pb_indexed_config.state().enable_notifications();
  gc.interval_length = pb_indexed_config.state().interval_length();
  gc.send_recovery_notifications_anyway =
      pb_indexed_config.state().send_recovery_notifications_anyway();
  return gc;
}

/**
 * @brief Get the notification-relevant state of a resource.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 *
 * @return The resource state snapshot (all-default if the resource is unknown).
 */
notifications::resource_state engine_notification_callbacks::get_state(
    uint64_t host_id,
    uint64_t service_id) const {
  notifications::resource_state rs;
  notifier* n = get_resource(host_id, service_id);
  if (!n)
    return rs;

  rs.notifications_enabled = n->get_notifications_enabled();
  rs.in_downtime = n->is_in_downtime();
  rs.flapping = n->get_is_flapping();
  rs.is_volatile = n->get_is_volatile();
  rs.hard_state = n->get_state_type() == checkable::hard;
  rs.acknowledged = n->problem_has_been_acknowledged();
  rs.notify_on_current_state = n->get_notify_on_current_state();
  rs.authorized_by_dependencies =
      n->authorized_by_dependencies(dependency::notification);
  rs.current_state = n->get_current_state_int();
  rs.scheduled_downtime_depth = n->get_scheduled_downtime_depth();
  rs.last_hard_state_change = n->get_last_hard_state_change();
  rs.current_state_as_string = n->get_current_state_as_string();
  rs.notify_on = n->get_notify_on();
  rs.notification_interval = n->get_notification_interval();
  rs.first_notification_delay = n->get_first_notification_delay();
  rs.recovery_notification_delay = n->get_recovery_notification_delay();

  timeperiod* tp{n->get_notification_timeperiod()};
  timezone_locker lock{n->get_timezone()};
  rs.in_notification_period =
      check_time_against_period_for_notif(std::time(nullptr), tp);

  return rs;
}

/**
 * @brief Select the contacts and actually send the notification.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 * @param cat The notification category.
 * @param type The notification reason.
 * @param notification_id The unique notification id (for macros).
 * @param notification_number The notification number (for macros).
 * @param author The notification author.
 * @param message The notification message/comment.
 * @param options The notification options.
 *
 * @return Who was notified, the escalation-adjusted interval and the escalated
 * flag.
 */
notifications::delivery_result engine_notification_callbacks::deliver(
    uint64_t host_id,
    uint64_t service_id,
    notifications::notification_category cat,
    notifications::reason_type type,
    uint64_t notification_id,
    uint32_t notification_number,
    const std::string& author,
    const std::string& message,
    notifications::notification_option options) {
  notifications::delivery_result result;
  notifier* n = get_resource(host_id, service_id);
  if (!n)
    return result;

  /* Select the contacts to notify (escalations included). The recovery routing
   * consults the manager through the notifier's accessors. */
  bool escalated;
  std::unordered_set<std::shared_ptr<contact>> to_notify =
      n->get_contacts_to_notify(cat, type, result.notification_interval,
                                escalated);
  result.escalated = escalated;

  nagios_macros* mac(get_global_macros());

  /* Grab the macro variables */
  n->grab_macros_r(mac);

  contact* author_contact{nullptr};
  contact_map::const_iterator it{contact::contacts.find(author)};
  if (it != contact::contacts.end())
    author_contact = it->second.get();
  else {
    for (contact_map::const_iterator cit{contact::contacts.begin()},
         cend{contact::contacts.end()};
         cit != cend; ++cit) {
      if (cit->second->get_alias() == author) {
        author_contact = cit->second.get();
        break;
      }
    }
  }

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

  if (n->get_notifier_type() == notifications::host_notification) {
    mac->x[MACRO_HOSTNOTIFICATIONNUMBER] = std::to_string(notification_number);
    /* The $NOTIFICATIONNUMBER$ macro is maintained for backward compatibility
     */
    mac->x[MACRO_NOTIFICATIONNUMBER] = mac->x[MACRO_HOSTNOTIFICATIONNUMBER];
    mac->x[MACRO_NOTIFICATIONISESCALATED] = std::to_string(escalated);
    mac->x[MACRO_HOSTNOTIFICATIONID] = std::to_string(notification_id);
  } else {
    mac->x[MACRO_SERVICENOTIFICATIONNUMBER] =
        std::to_string(notification_number);
    mac->x[MACRO_NOTIFICATIONNUMBER] = mac->x[MACRO_SERVICENOTIFICATIONNUMBER];
    mac->x[MACRO_NOTIFICATIONISESCALATED] = std::to_string(escalated);
    mac->x[MACRO_SERVICENOTIFICATIONID] = std::to_string(notification_id);
  }

  for (const std::shared_ptr<contact>& ctc_ptr : to_notify) {
    contact* ctc = ctc_ptr.get();

    /* grab the macro variables for this contact */
    grab_contact_macros_r(mac, ctc);
    /* clear summary macros (they are customized for each contact) */
    clear_summary_macros_r(mac);

    if (n->notify_contact(mac, ctc, type, author.c_str(), message.c_str(),
                          options, escalated) == OK) {
      result.notified_contacts.insert(ctc->get_name());
      if (mac->x[MACRO_NOTIFICATIONRECIPIENTS].empty())
        mac->x[MACRO_NOTIFICATIONRECIPIENTS] = ctc->get_name();
      else {
        mac->x[MACRO_NOTIFICATIONRECIPIENTS].append(",");
        mac->x[MACRO_NOTIFICATIONRECIPIENTS].append(ctc->get_name());
      }
    }
  }

  return result;
}

/**
 * @brief Push the new notification number of a resource to Broker.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 */
void engine_notification_callbacks::on_notification_number_changed(
    uint64_t host_id,
    uint64_t service_id) {
  notifier* n = get_resource(host_id, service_id);
  if (n)
    n->update_status(notifications::STATUS_NOTIFICATION_NUMBER);
}
