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

#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/notification_execution.hh"
#include "common/timeperiods/timezone.hh"

using namespace com::centreon::common::timeperiods;
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
 * @brief Get the effective notification configuration for a resource.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 *
 * @return The effective notification configuration snapshot. @c enabled folds
 * in the resource's own notification-enable flag, so an unknown resource or one
 * with notifications disabled yields @c enabled == false.
 */
notifications::config engine_notification_callbacks::get_config(
    uint64_t host_id,
    uint64_t service_id) const {
  notifications::config cfg;
  notifier* n = get_resource(host_id, service_id);
  cfg.enabled = n && pb_indexed_config.state().enable_notifications() &&
                n->get_notifications_enabled();
  cfg.send_recovery_notifications_anyway =
      pb_indexed_config.state().send_recovery_notifications_anyway();
  return cfg;
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
  /* The notification library reasons in seconds: convert the engine "interval
   * unit" values here, at the boundary, by multiplying by interval_length. */
  uint32_t interval_length = pb_indexed_config.state().interval_length();
  rs.notification_interval =
      std::chrono::seconds(n->get_notification_interval() * interval_length);
  rs.first_notification_delay =
      std::chrono::seconds(n->get_first_notification_delay() * interval_length);
  rs.recovery_notification_delay = std::chrono::seconds(
      n->get_recovery_notification_delay() * interval_length);

  timeperiod* tp{n->get_notification_timeperiod()};
  // No notification period means the contact may be notified at any time.
  rs.in_notification_period =
      !tp || tp->check_time_against_period_for_notif(
                 std::time(nullptr), string_to_timezone(n->get_timezone()));

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
  uint32_t notification_interval = 0;
  absl::flat_hash_set<std::shared_ptr<contact>> to_notify =
      n->get_contacts_to_notify(cat, type, notification_interval, escalated);
  result.escalated = escalated;
  /* Convert the escalation-adjusted interval to seconds, as the notification
   * library expects absolute durations. */
  result.notification_interval = std::chrono::seconds(
      notification_interval * pb_indexed_config.state().interval_length());

  result.notified_contacts = run_notification_commands(
      n, to_notify, type, notification_id, notification_number, escalated,
      author, message, options);

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

}  // namespace com::centreon::engine
