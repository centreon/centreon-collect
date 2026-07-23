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

#include "com/centreon/broker/broker_notification_callbacks.hh"

#include <vector>

#include "bbdo/internal.hh"
#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "common/log_v2/log_v2.hh"

namespace com::centreon::broker {

using log_v2 = com::centreon::common::log_v2::log_v2;

namespace {
constexpr std::array<std::pair<uint32_t, std::string_view>, 5> service_states{
    {{notifications::ok, "OK"},
     {notifications::warning, "WARNING"},
     {notifications::critical, "CRITICAL"},
     {notifications::unknown, "UNKNOWN"},
     {notifications::none, "PENDING"}}};

constexpr std::array<std::pair<uint32_t, std::string_view>, 3> host_states{
    {{notifications::up, "UP"},
     {notifications::down, "DOWN"},
     {notifications::unreachable, "UNREACHABLE"}}};
}  // namespace

/**
 * @brief Constructor. Binds the logger to the NOTIFICATIONS channel, shared
 * with the notification library.
 */
broker_notification_callbacks::broker_notification_callbacks()
    : _logger{log_v2::instance().get(log_v2::NOTIFICATIONS)} {}

/**
 * @brief Get the notification configuration that applies to a resource.
 *
 * The poller-wide flags are read from the Broker cache instances map, the
 * poller being resolved through the host's instance_id. @c enabled combines
 * the poller-wide switch with the resource's own notify flag, so an unknown
 * resource yields @c enabled == false.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 *
 * @return The notification configuration snapshot.
 */
notifications::config broker_notification_callbacks::get_config(
    uint64_t host_id,
    uint64_t service_id) const {
  notifications::config cfg;
  auto& cache = config::applier::state::instance().cache();
  auto host = cache.host(host_id);
  if (!host) {
    SPDLOG_LOGGER_WARN(_logger,
                       "notification config requested for host {} unknown to "
                       "the broker cache: notifications stay disabled for it",
                       host_id);
    return cfg;
  }

  bool resource_notify;
  if (service_id) {
    auto svc = cache.service(host_id, service_id);
    if (!svc) {
      SPDLOG_LOGGER_WARN(
          _logger,
          "notification config requested for service ({}, {}) unknown to the "
          "broker cache: notifications stay disabled for it",
          host_id, service_id);
      return cfg;
    }
    resource_notify = svc->obj().notify();
  } else
    resource_notify = host->obj().notify();

  uint64_t poller_id = host->obj().instance_id();
  cfg.enabled = cache.notifications_enabled(poller_id) && resource_notify;
  cfg.send_recovery_notifications_anyway =
      cache.send_recovery_notifications_anyway(poller_id);
  return cfg;
}

/**
 * @brief Get the notification-relevant state of a resource.
 *
 * The snapshot is read from the Broker cache pb objects. The notification
 * timeperiod is evaluated against the cache timeperiod registry, with the
 * config-inheritance rule the Engine applier uses (an empty service value
 * inherits the host's). The notification dependencies are evaluated against the
 * cache dependency registry
 * (@c broker_cache::notification_authorized_by_dependencies).
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 *
 * @return The resource state snapshot (all-default if the resource is unknown).
 */
notifications::resource_state broker_notification_callbacks::get_state(
    uint64_t host_id,
    uint64_t service_id) const {
  notifications::resource_state retval;
  auto& cache = config::applier::state::instance().cache();

  /* The interval_length ("duration of one interval unit") is a poller-wide
   * setting: resolve the poller through the host, for services too. */
  auto hst = cache.host(host_id);
  if (!hst)
    return retval;

  const Host& oh = hst->obj();
  const std::chrono::seconds interval_length =
      cache.interval_length(oh.instance_id());

  /* Engine applies host->service inheritance for these two after receiving
   * the configuration (inherits_special_vars), so the cached pb objects may
   * carry them empty: apply the same fallback here. */
  std::string notification_period;
  std::string timezone;

  if (service_id) {
    auto svc = cache.service(host_id, service_id);
    if (!svc)
      return retval;
    const Service& o = svc->obj();

    retval.flapping = o.flapping();
    retval.is_volatile = o.is_volatile();
    retval.hard_state = o.state_type() == Service::HARD;
    retval.acknowledged = o.acknowledged();
    retval.current_state = o.state();
    retval.scheduled_downtime_depth = o.scheduled_downtime_depth();
    retval.last_hard_state_change = o.last_hard_state_change();
    retval.notify_on =
        (o.notify_on_warning() ? notifications::warning : notifications::none) |
        (o.notify_on_critical() ? notifications::critical
                                : notifications::none) |
        (o.notify_on_unknown() ? notifications::unknown : notifications::none) |
        (o.notify_on_recovery() ? notifications::ok : notifications::none) |
        (o.notify_on_flapping()
             ? (notifications::flappingstart | notifications::flappingstop |
                notifications::flappingdisabled)
             : notifications::none) |
        (o.notify_on_downtime() ? notifications::downtime
                                : notifications::none);
    const auto& [state_flag, state_str] =
        service_states[static_cast<size_t>(o.state())];
    retval.notify_on_current_state = retval.notify_on & state_flag;
    retval.current_state_as_string = state_str;
    /* The notification library reasons in seconds: convert the "interval
     * unit" values here, at the boundary. */
    retval.notification_interval =
        static_cast<int64_t>(o.notification_interval()) * interval_length;
    retval.first_notification_delay =
        static_cast<int64_t>(o.first_notification_delay()) * interval_length;
    retval.recovery_notification_delay =
        static_cast<int64_t>(o.recovery_notification_delay()) * interval_length;
    notification_period = o.notification_period().empty()
                              ? oh.notification_period()
                              : o.notification_period();
    timezone = o.timezone().empty() ? oh.timezone() : o.timezone();
  } else {
    retval.flapping = oh.flapping();
    /* is_volatile stays false: it is a service-only attribute (engine
     * hardcodes false for hosts and neb.proto has no such host field). */
    retval.hard_state = oh.state_type() == Host::HARD;
    retval.acknowledged = oh.acknowledged();
    retval.current_state = oh.state();
    retval.scheduled_downtime_depth = oh.scheduled_downtime_depth();
    retval.last_hard_state_change = oh.last_hard_state_change();
    retval.notify_on =
        (oh.notify_on_down() ? notifications::down : notifications::none) |
        (oh.notify_on_unreachable() ? notifications::unreachable
                                    : notifications::none) |
        (oh.notify_on_recovery() ? notifications::up : notifications::none) |
        (oh.notify_on_flapping()
             ? (notifications::flappingstart | notifications::flappingstop |
                notifications::flappingdisabled)
             : notifications::none) |
        (oh.notify_on_downtime() ? notifications::downtime
                                 : notifications::none);
    const auto& [state_flag, state_str] =
        host_states[static_cast<size_t>(oh.state())];
    retval.notify_on_current_state = retval.notify_on & state_flag;
    retval.current_state_as_string = state_str;
    /* The notification library reasons in seconds: convert the "interval
     * unit" values here, at the boundary. */
    retval.notification_interval =
        static_cast<int64_t>(oh.notification_interval()) * interval_length;
    retval.first_notification_delay =
        static_cast<int64_t>(oh.first_notification_delay()) * interval_length;
    retval.recovery_notification_delay =
        static_cast<int64_t>(oh.recovery_notification_delay()) *
        interval_length;
    notification_period = oh.notification_period();
    timezone = oh.timezone();
  }

  /* ISO with Engine: when neither the service nor the host carries an explicit
   * timezone, Engine evaluates timeperiods in the poller machine's local
   * timezone. Broker cannot use its own local timezone (it may live in another
   * zone), so it falls back to the timezone the poller advertised at
   * negotiation time. */
  if (timezone.empty())
    timezone =
        config::applier::state::instance().poller_timezone(oh.instance_id());

  retval.in_notification_period = cache.in_notification_period(
      notification_period, timezone, std::time(nullptr));

  retval.authorized_by_dependencies =
      cache.notification_authorized_by_dependencies(host_id, service_id);

  return retval;
}

/**
 * @brief Select the contacts and dispatch the notification execution to the
 * poller that supervises the resource (notification model C).
 *
 * On the Broker side the DECISION (contact selection + escalations) is made
 * here, then only the EXECUTION (macro expansion + notification command launch)
 * is dispatched to the poller through a pb_notification_execute event; the
 * poller holds the macro context. The event is broadcast: every poller receives
 * it but only the one supervising the resource acts on it.
 *
 * @note Brick 1 (dispatch channel) only. The real contact selection over the
 * broker cache is not ported yet (brick 3); for now a fixed contact list is
 * injected so the channel can be exercised end to end.
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
 * @return The (injected) contacts reported as notified.
 */
notifications::delivery_result broker_notification_callbacks::deliver(
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

  /* Resolve the poller supervising the resource: the execution is dispatched to
   * it (it holds the resource state and config needed for macro expansion). */
  auto& cache = config::applier::state::instance().cache();
  auto host = cache.host(host_id);
  if (!host) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "notification for host {} unknown to the broker cache: dropped",
        host_id);
    return result;
  }
  uint64_t poller_id = host->obj().instance_id();

  /* TODO(MON-187019) brick 1 placeholder: contact selection (contacts,
   * contactgroups, escalations, per-contact filtering) is not ported to the
   * broker cache yet. A fixed contact list is injected so the dispatch channel
   * can be exercised end to end; brick 3 replaces this with the real
   * selection and the escalation-adjusted interval. */
  static const std::vector<std::string> injected_contacts{"John_Doe"};

  auto evt = std::make_shared<bbdo::pb_notification_execute>();
  /* Broadcast (destination_id kept as a hint for a future targeted routing):
   * every poller receives the event, only the supervisor acts on it. */
  evt->source_id = 0;
  evt->destination_id = static_cast<uint32_t>(poller_id);
  auto& obj = evt->mut_obj();
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  obj.set_category(static_cast<uint32_t>(cat));
  obj.set_reason_type(static_cast<uint32_t>(type));
  obj.set_notification_id(notification_id);
  obj.set_notification_number(notification_number);
  obj.set_escalated(false);
  obj.set_author(author);
  obj.set_message(message);
  obj.set_options(static_cast<uint32_t>(options));
  for (const auto& c : injected_contacts)
    obj.add_contacts(c);

  multiplexing::publisher pblshr;
  pblshr.write(evt);

  SPDLOG_LOGGER_INFO(
      _logger,
      "dispatched notification execution for resource ({}, {}) to poller {} "
      "with {} contact(s)",
      host_id, service_id, poller_id, injected_contacts.size());

  /* Report the injected contacts as notified so the notification_manager
   * bookkeeping (last_notification, recovery routing) works end to end. */
  result.notified_contacts.insert(injected_contacts.begin(),
                                  injected_contacts.end());
  return result;
}

/**
 * @brief Push the new notification number of a resource to Broker.
 *
 * @note Not implemented yet: the status push to Broker still has to be wired.
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 */
void broker_notification_callbacks::on_notification_number_changed(
    uint64_t host_id,
    uint64_t service_id) {
  /* TODO(MON-187019): publish the updated notification number to Broker. */
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "notification number changed for resource ({}, {}): Broker-side status "
      "push not implemented yet",
      host_id, service_id);
}

}  // namespace com::centreon::broker
