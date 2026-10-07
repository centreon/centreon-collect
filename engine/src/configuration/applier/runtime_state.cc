/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#include "com/centreon/engine/configuration/applier/runtime_state.hh"

#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

namespace {
/**
 * @brief Whether Broker owns acknowledgements, downtimes and notifications
 * (notification_mode=broker): it then replays them itself and the snapshot
 * says nothing about them.
 */
bool broker_owns_notifications() {
  return cbm && cbm->broker_handles_notifications();
}

/**
 * @brief The fields of the snapshot a host and a service share, set the same
 * way: the check result and, when Engine owns them, the acknowledgement, the
 * downtime depth and the notification counters.
 *
 * @tparam Runtime The snapshot entry type (HostRuntime or ServiceRuntime).
 * @tparam Message The Engine object type (host or service).
 * @tparam State The state enum of @a Message (host_state or service_state).
 *
 * @param r The snapshot entry.
 * @param obj The host or service.
 * @param state The current state, already converted to @a State.
 * @param hard The last hard state, already converted to @a State.
 */
template <typename Runtime, typename Message, typename State>
void apply_common(const Runtime& r, Message& obj, State state, State hard) {
  obj.set_has_been_checked(r.checked());
  obj.set_check_type(static_cast<checkable::check_type>(r.check_type()));
  obj.set_current_state(state);
  obj.set_last_state(state);
  obj.set_state_type(static_cast<checkable::state_type>(r.state_type()));
  obj.set_last_state_change(r.last_state_change());
  obj.set_last_hard_state(hard);
  obj.set_last_hard_state_change(r.last_hard_state_change());
  /* The check output is not part of the snapshot: the macros reading it are
   * to be reworked to do without. */
  /* Broker fills the perfdata only for the services an anomalydetection
   * depends on; an empty one must not erase what a check may have set. */
  if (!r.perfdata().empty())
    obj.set_perf_data(r.perfdata());
  obj.set_is_flapping(r.flapping());
  obj.set_percent_state_change(r.percent_state_change());
  obj.set_latency(r.latency());
  obj.set_execution_time(r.execution_time());
  obj.set_last_check(r.last_check());
  obj.set_current_attempt(r.check_attempt());
  if (!broker_owns_notifications()) {
    obj.set_acknowledgement(static_cast<AckType>(r.acknowledgement_type()));
    obj.set_scheduled_downtime_depth(r.scheduled_downtime_depth());
    obj.set_notification_number(r.notification_number());
    obj.set_no_more_notifications(r.no_more_notifications());
    obj.set_last_notification(r.last_notification());
    obj.set_next_notification(r.next_notification());
  }
}

/**
 * @brief Apply the overrides of the snapshot: each attribute whose bit is set
 * in the mask, exactly as the retention applier did. An override naming a
 * timeperiod that no longer exists is dropped, with its bit. The check command
 * and the event handler are not overridable, the external commands changing
 * them being disabled in Engine.
 *
 * @tparam Message The Engine object type (host or service).
 *
 * @param r The overrides of the snapshot entry.
 * @param obj The host or service.
 *
 * @return The mask finally applied.
 */
template <typename Message>
uint32_t apply_overrides(const configuration::RuntimeOverrides& r,
                         Message& obj) {
  uint32_t mask = r.modified_attributes();
  if (mask & MODATTR_NOTIFICATIONS_ENABLED)
    obj.set_notifications_enabled(r.notify());
  if (mask & MODATTR_ACTIVE_CHECKS_ENABLED) {
    obj.set_checks_enabled(r.active_checks());
    /* What ENABLE/DISABLE_*_CHECK do with the scheduling. */
    obj.set_should_be_scheduled(r.active_checks() && obj.check_interval() > 0);
  }
  if (mask & MODATTR_PASSIVE_CHECKS_ENABLED)
    obj.set_accept_passive_checks(r.passive_checks());
  if (mask & MODATTR_EVENT_HANDLER_ENABLED)
    obj.set_event_handler_enabled(r.event_handler_enabled());
  if (mask & MODATTR_FLAP_DETECTION_ENABLED)
    obj.set_flap_detection_enabled(r.flap_detection());
  if (mask & MODATTR_OBSESSIVE_HANDLER_ENABLED)
    obj.set_obsess_over(r.obsess_over());
  if (mask & MODATTR_NORMAL_CHECK_INTERVAL)
    obj.set_check_interval(r.check_interval());
  if (mask & MODATTR_RETRY_CHECK_INTERVAL)
    obj.set_retry_interval(r.retry_interval());
  if (mask & MODATTR_MAX_CHECK_ATTEMPTS) {
    obj.set_max_attempts(r.max_check_attempts());
    /* A hard problem state fills the attempts, as the retention did. */
    if (obj.get_state_type() == notifier::hard &&
        static_cast<int>(obj.get_current_state()) != 0 &&
        obj.get_current_attempt() > 1)
      obj.set_current_attempt(obj.max_check_attempts());
  }
  if (mask & MODATTR_FRESHNESS_CHECKS_ENABLED)
    obj.set_check_freshness(r.check_freshness());
  if (mask & MODATTR_CHECK_TIMEPERIOD) {
    auto found = ::timeperiods.find(r.check_period());
    if (found != ::timeperiods.end()) {
      obj.set_check_period(r.check_period());
      obj.check_period_ptr = found->second.get();
    } else
      mask &= ~MODATTR_CHECK_TIMEPERIOD;
  }
  if (mask & MODATTR_NOTIFICATION_TIMEPERIOD) {
    auto found = ::timeperiods.find(r.notification_period());
    if (found != ::timeperiods.end()) {
      obj.set_notification_period(r.notification_period());
      obj.set_notification_period_ptr(found->second.get());
    } else
      mask &= ~MODATTR_NOTIFICATION_TIMEPERIOD;
  }
  obj.set_modified_attributes(mask);
  return mask;
}
}  // namespace

/**
 * @brief Apply the snapshot to the hosts and services it names.
 *
 * A resource the poller does not supervise is skipped, and so is one
 * configured not to retain its status (retain_status_information), as the
 * retention did. The scheduling is not touched: the scheduler keeps the next
 * check it planned.
 *
 * @param rs The snapshot.
 */
void applier::runtime_state::apply(const configuration::RuntimeState& rs) {
  size_t hosts = 0;
  size_t services = 0;
  size_t overrides = 0;
  size_t unknown = 0;
  for (const auto& r : rs.hosts()) {
    auto it = engine::host::hosts_by_id.find(r.host_id());
    if (it == engine::host::hosts_by_id.end()) {
      ++unknown;
      continue;
    }
    engine::host& h = *it->second;
    /* An entry without check state carries overrides only. */
    if (r.checked() && h.get_retain_status_information()) {
      apply_common(r, h, static_cast<engine::host::host_state>(r.state()),
                   static_cast<engine::host::host_state>(r.last_hard_state()));
      h.set_last_time_up(r.last_time_up());
      h.set_last_time_down(r.last_time_down());
      h.set_last_time_unreachable(r.last_time_unreachable());
      ++hosts;
    }
    if (r.has_overrides() && apply_overrides(r.overrides(), h))
      ++overrides;
  }
  for (const auto& r : rs.services()) {
    auto it = engine::service::services_by_id.find(
        std::make_pair(r.host_id(), r.service_id()));
    if (it == engine::service::services_by_id.end()) {
      ++unknown;
      continue;
    }
    engine::service& s = *it->second;
    if (r.checked() && s.get_retain_status_information()) {
      apply_common(r, s,
                   static_cast<engine::service::service_state>(r.state()),
                   static_cast<engine::service::service_state>(
                       r.last_hard_state()));
      s.set_last_time_ok(r.last_time_ok());
      s.set_last_time_warning(r.last_time_warning());
      s.set_last_time_critical(r.last_time_critical());
      s.set_last_time_unknown(r.last_time_unknown());
      ++services;
    }
    if (r.has_overrides() && apply_overrides(r.overrides(), s))
      ++overrides;
  }
  config_logger->info(
      "runtime state: {} hosts and {} services restored from the Broker "
      "snapshot, {} resources with overridden attributes ({} unknown "
      "resources skipped)",
      hosts, services, overrides, unknown);
}
