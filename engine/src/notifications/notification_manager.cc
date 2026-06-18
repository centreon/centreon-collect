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

#include "engine/src/notifications/notification_manager.hh"

#include <cassert>

#include "common/log_v2/log_v2.hh"
#include "engine/src/notifications/notification_callbacks.hh"
#include "engine/src/notifications/notification_types.hh"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::engine::notifications {

notification_manager* notification_manager::_instance = nullptr;

namespace {
/* The notification library logs through common/log_v2, not through engine
 * globals — one less dependency on the host application. */
std::shared_ptr<spdlog::logger> functions_logger() {
  return log_v2::instance().get(log_v2::FUNCTIONS);
}
std::shared_ptr<spdlog::logger> notifications_logger() {
  return log_v2::instance().get(log_v2::NOTIFICATIONS);
}

/* Success return value (matches engine's OK == 0 without depending on it). */
constexpr int32_t k_ok = 0;
}  // namespace

notification_manager::notification_manager() = default;
notification_manager::~notification_manager() = default;

/**
 * @brief Get the unique instance of the notification manager.
 *
 * This singleton does not follow the C++ Meyers idiom on purpose: we need to
 * control when it is destroyed (see load()/unload() and the class doc).
 *
 * @return A reference to the notification_manager singleton.
 */
notification_manager& notification_manager::instance() {
  assert(_instance);
  return *_instance;
}

/**
 * @brief Create the singleton (if needed) and inject the host-application
 * backend.
 *
 * Must be called once at startup, before any notifier is created.
 */
void notification_manager::load(
    std::unique_ptr<notification_callbacks> callbacks) {
  if (!_instance)
    _instance = new notification_manager();
  _instance->_callbacks = std::move(callbacks);
}

/**
 * @brief Destroy the singleton.
 *
 * Must be called at shutdown. Late ~notifier() calls to forget() afterwards
 * are safe: forget() is a no-op once the instance is gone.
 */
void notification_manager::unload() {
  if (_instance) {
    delete _instance;
    _instance = nullptr;
  }
}

uint64_t notification_manager::next_notification_id() noexcept {
  return _next_notification_id++;
}

uint64_t notification_manager::get_next_notification_id() const noexcept {
  return _next_notification_id;
}

notification_category notification_manager::get_category(reason_type type) {
  if (type == reason_custom)
    return cat_custom;
  notification_category cat[] = {
      cat_normal,   cat_recovery, cat_acknowledgement, cat_flapping,
      cat_flapping, cat_flapping, cat_downtime,        cat_downtime,
      cat_downtime, cat_custom};
  return cat[static_cast<size_t>(type)];
}

bool notification_manager::is_notification_viable(uint64_t host_id,
                                                  uint64_t service_id,
                                                  notification_category cat,
                                                  reason_type type,
                                                  notification_option options) {
  global_config gc = _callbacks->get_global_config();
  std::time_t now;
  std::time(&now);
  resource_state rs = _callbacks->get_state(host_id, service_id);
  switch (cat) {
    case cat_normal:
      return _is_notification_viable_normal(host_id, service_id, rs, gc, now,
                                            type, options);
    case cat_recovery:
      return _is_notification_viable_recovery(host_id, service_id, rs, gc, now,
                                              type, options);
    case cat_acknowledgement:
      return _is_notification_viable_acknowledgement(rs, gc, type, options);
    case cat_flapping:
      return _is_notification_viable_flapping(host_id, service_id, rs, gc, type,
                                              options);
    case cat_downtime:
      return _is_notification_viable_downtime(rs, gc, type, options);
    case cat_custom:
      return _is_notification_viable_custom(rs, gc, type, options);
  }
  return false;
}

bool notification_manager::_is_notification_viable_normal(
    uint64_t host_id,
    uint64_t service_id,
    const resource_state& rs,
    const global_config& gc,
    std::time_t now,
    reason_type type [[maybe_unused]],
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger(),
                      "notification::is_notification_viable_normal()");

  notification* normal_notif =
      current_notification(host_id, service_id, cat_normal);
  uint32_t notification_interval =
      !normal_notif ? rs.notification_interval : normal_notif->interval;

  /* forced notifications bust through everything */
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  if (!gc.enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  if (!rs.notifications_enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  if (rs.in_downtime) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier is currently in a scheduled downtime, so "
        "we won't send notifications.");
    return false;
  }

  if (!rs.in_notification_period) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "This notifier shouldn't have notifications sent out "
                        "at this time.");
    return false;
  }

  if (rs.flapping) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier is flapping, so we won't send notifications.");
    return false;
  }

  if (rs.is_volatile) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This is a volatile service notification, so it is sent.");
    return true;
  }

  if (!rs.hard_state) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier is in soft state, so we won't send notifications.");
    return false;
  }

  if (rs.acknowledged) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier problem has been acknowledged, so we won't send "
        "notifications.");
    return false;
  }

  if (rs.current_state == 0) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "We don't send a normal notification when the state is ok/up");
    return false;
  }

  if (!rs.notify_on_current_state) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier is unable to notify the state {}: not configured for "
        "that or, for a service, its host may be down",
        rs.current_state_as_string);
    return false;
  }

  if (rs.first_notification_delay > 0 && !normal_notif &&
      rs.last_hard_state_change +
              rs.first_notification_delay * gc.interval_length >
          now) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier is configured with a first notification delay, we "
        "won't send notification until timestamp {}",
        rs.first_notification_delay * gc.interval_length);
    return false;
  }

  if (!rs.authorized_by_dependencies) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "This notifier won't send any notification since it depends on"
        " another notifier that has already sent one");
    return false;
  }

  if (normal_notif) {
    std::time_t last_notif = last_notification(host_id, service_id);
    /* In the case of a state change, we don't care of the notification interval
     * and we notify as soon as we can */
    if (rs.last_hard_state_change <= last_notif) {
      if (notification_interval == 0) {
        SPDLOG_LOGGER_DEBUG(
            notifications_logger(),
            "This notifier problem has already been sent at {} so, since the "
            "notification interval is 0, it won't be sent anymore",
            last_notif);
        return false;
      } else if (notification_interval > 0) {
        if (last_notif + notification_interval * gc.interval_length > now) {
          SPDLOG_LOGGER_DEBUG(
              notifications_logger(),
              "This notifier problem has been sent at {} so it won't be sent "
              "until {}",
              last_notif, notification_interval * gc.interval_length);
          return false;
        }
      }
    }
  }
  return true;
}

bool notification_manager::_is_notification_viable_recovery(
    uint64_t host_id,
    uint64_t service_id,
    const resource_state& rs,
    const global_config& gc,
    std::time_t now,
    reason_type type [[maybe_unused]],
    notification_option options [[maybe_unused]]) {
  SPDLOG_LOGGER_TRACE(functions_logger(),
                      "notification::is_notification_viable_recovery()");
  bool retval{true};
  bool send_later{false};

  if (!gc.enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    retval = false;
  } else if (!rs.notifications_enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    retval = false;
  } else {
    // if send_recovery_notifications_anyway flag is set, we don't take
    // timeperiod into account for recovery
    if (!rs.in_notification_period) {
      if (gc.send_recovery_notifications_anyway) {
        SPDLOG_LOGGER_DEBUG(notifications_logger(),
                            "send_recovery_notifications_anyway flag enabled, "
                            "recovery notification is viable even if we are "
                            "out of timeperiod at this time.");
      } else {
        SPDLOG_LOGGER_DEBUG(notifications_logger(),
                            "This notifier shouldn't have notifications sent "
                            "out at this time.");
        retval = false;
        send_later = true;
      }
    } else if (rs.in_downtime) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "This notifier is currently in a scheduled downtime, so "
          "we won't send notifications.");
      retval = false;
      send_later = true;
    } else if (rs.flapping) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "This notifier is flapping, so we won't send notifications.");
      retval = false;
      send_later = true;
    } else if (!rs.hard_state) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "This notifier is in soft state, so we won't send notifications.");
      retval = false;
      send_later = true;
    } else if (rs.current_state != 0) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "This notifier state is not UP/OK to send a recovery notification");
      retval = false;
      send_later = true;
    } else if (!((rs.notify_on & up) || (rs.notify_on & ok))) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "This notifier is not configured to send a recovery notification");
      retval = false;
      send_later = false;
    } else if (rs.last_hard_state_change +
                   rs.recovery_notification_delay * gc.interval_length >
               now) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "This notifier is configured with a recovery notification delay. "
          "It won't send any recovery notification until timestamp "
          "so it won't be sent until {}",
          rs.last_hard_state_change + rs.recovery_notification_delay);
      retval = false;
      send_later = true;
    } else if (notification_number(host_id, service_id) == 0) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "No notification has been sent to "
          "announce a problem. So no recovery notification will be sent");
      retval = false;
    } else if (!current_notification(host_id, service_id, cat_normal)) {
      SPDLOG_LOGGER_DEBUG(notifications_logger(),
                          "We should not send a notification "
                          "since no normal notification has"
                          " been sent before");
      retval = false;
    }
  }

  if (!retval) {
    if (!send_later) {
      _state(host_id, service_id).events[cat_normal].reset();
      SPDLOG_LOGGER_TRACE(
          notifications_logger(),
          " _notification_number _is_notification_viable_recovery: {} => 0",
          notification_number(host_id, service_id));
      set_notification_number(host_id, service_id, 0);
    }
  }

  return retval;
}

bool notification_manager::_is_notification_viable_acknowledgement(
    const resource_state& rs,
    const global_config& gc,
    reason_type type [[maybe_unused]],
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger(),
                      "notification::is_notification_viable_acknowledgement()");
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  if (!gc.enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  if (!rs.notifications_enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  if (rs.current_state == 0) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "The notifier is currently OK/UP, so we "
                        "won't send an acknowledgement.");
    return false;
  }
  return true;
}

bool notification_manager::_is_notification_viable_flapping(
    uint64_t host_id,
    uint64_t service_id,
    const resource_state& rs,
    const global_config& gc,
    reason_type type,
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger(),
                      "notification::is_notification_viable_flapping()");
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  if (!gc.enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  if (!rs.notifications_enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  /* Don't send a notification if we are not supposed to */
  notification_flag f;
  if (type == reason_flappingstart)
    f = flappingstart;
  else if (type == reason_flappingstop)
    f = flappingstop;
  else
    f = flappingdisabled;

  if (!(rs.notify_on & f)) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "We shouldn't notify about {} events for this notifier.",
        tab_notification_str[type]);
    return false;
  }

  notification* flapping_notif =
      current_notification(host_id, service_id, cat_flapping);
  /* Don't send a start notification if a flapping notification is already there
   */
  if (type == reason_flappingstart && flapping_notif) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "A flapping notification is already running, we can not send "
        "a start notification now.");
    return false;
  } else if (type == reason_flappingstop || type == reason_flappingdisabled) {
    if (!flapping_notif || flapping_notif->type != reason_flappingstart) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger(),
          "A stop or cancellation flapping notification can only be sent "
          "after a start flapping notification.");
      return false;
    }
  }

  /* Don't send a notification if the same has already been sent previously. */
  if (flapping_notif && flapping_notif->type == type) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "We shouldn't notify about a {} event: already sent.",
                        tab_notification_str[type]);
    return false;
  }

  /* Don't send notifications during scheduled downtime */
  if (rs.in_downtime) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "We shouldn't notify about FLAPPING "
                        "events during scheduled downtime.");
    return false;
  }
  return true;
}

bool notification_manager::_is_notification_viable_downtime(
    const resource_state& rs,
    const global_config& gc,
    reason_type type [[maybe_unused]],
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger(),
                      "notification::is_notification_viable_downtime()");
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  if (!gc.enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  if (!rs.notifications_enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  if (!(rs.notify_on & downtime)) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "We shouldn't notify about DOWNTIME events for this notifier.");
    return false;
  }

  if (rs.scheduled_downtime_depth > 0) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "We shouldn't notify about DOWNTIME "
                        "events during scheduled downtime.");
    return false;
  }
  return true;
}

bool notification_manager::_is_notification_viable_custom(
    const resource_state& rs,
    const global_config& gc,
    reason_type type [[maybe_unused]],
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger(),
                      "notification::is_notification_viable_custom()");
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  if (!gc.enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  if (!rs.notifications_enabled) {
    SPDLOG_LOGGER_DEBUG(notifications_logger(),
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  if (rs.in_downtime) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger(),
        "We shouldn't send a CUSTOM notification during scheduled downtime.");
    return false;
  }
  return true;
}

int32_t notification_manager::notify(uint64_t host_id,
                                     uint64_t service_id,
                                     reason_type type,
                                     const std::string& not_author,
                                     const std::string& not_data,
                                     notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger(), "notification_manager::notify({})",
                      static_cast<uint32_t>(type));
  notification_category cat = get_category(type);

  /* Has this notification got sense? */
  if (!is_notification_viable(host_id, service_id, cat, type, options))
    return k_ok;

  /* For a first notification, we store what type of notification we try to
   * send and we fix the notification number to 1. */
  if (type != reason_recovery) {
    SPDLOG_LOGGER_TRACE(notifications_logger(),
                        "_notification_number notify: {} -> {}",
                        notification_number(host_id, service_id),
                        notification_number(host_id, service_id) + 1);
    inc_notification_number(host_id, service_id);
  }

  uint64_t current_id = next_notification_id();
  set_current_notification_id(host_id, service_id, current_id);
  uint32_t number = notification_number(host_id, service_id);

  /* The contacts already told about the ongoing problem (carried forward on a
   * normal notification). */
  absl::btree_set<std::string> already;
  if (notification* prev =
          current_notification(host_id, service_id, cat_normal))
    already = prev->notified_contacts;

  delivery_result res =
      _callbacks->deliver(host_id, service_id, cat, type, current_id, number,
                          not_author, not_data, options);

  auto notif = std::unique_ptr<notification>(new notification{
      type, res.notification_interval, std::move(res.notified_contacts)});

  if (!notif->notified_contacts.empty())
    set_last_notification(host_id, service_id, std::time(nullptr));

  notification_state& st = _state(host_id, service_id);
  if (cat == cat_normal) {
    /* carry forward contacts of the previous normal notification, so they get
     * the recovery notification too */
    if (!already.empty())
      notif->add_contacts(already);
    st.events[cat] = std::move(notif);
  } else {
    st.events[cat] = std::move(notif);
    switch (cat) {
      case cat_recovery:
        st.events[cat_normal].reset();
        st.events[cat_recovery].reset();
        break;
      case cat_flapping:
        if (type == reason_flappingstop || type == reason_flappingdisabled)
          st.events[cat_flapping].reset();
        break;
      case cat_downtime:
        if (type == reason_downtimeend || type == reason_downtimecancelled)
          st.events[cat_downtime].reset();
        break;
      default:
        st.events[cat].reset();
    }
    /* In case of an acknowledgement, we must keep the _notification_number
     * otherwise the recovery notification won't be sent when needed. */
    if (cat != cat_acknowledgement && cat != cat_downtime) {
      SPDLOG_LOGGER_TRACE(notifications_logger(),
                          "_notification_number notify: {} => 0",
                          notification_number(host_id, service_id));
      set_notification_number(host_id, service_id, 0);
    }
  }

  return k_ok;
}

notification_manager::notification_state& notification_manager::_state(
    uint64_t host_id,
    uint64_t service_id) {
  return _states[{host_id, service_id}];
}

notification* notification_manager::current_notification(
    uint64_t host_id,
    uint64_t service_id,
    notification_category cat) const {
  auto it = _states.find({host_id, service_id});
  return it != _states.end() ? it->second.events[cat].get() : nullptr;
}

std::array<notification*, 6> notification_manager::current_notifications(
    uint64_t host_id,
    uint64_t service_id) const {
  std::array<notification*, 6> retval{};
  for (int i = 0; i < 6; i++)
    retval[i] = current_notification(host_id, service_id,
                                     static_cast<notification_category>(i));
  return retval;
}

void notification_manager::set_notification(uint64_t host_id,
                                            uint64_t service_id,
                                            notification_category cat,
                                            std::unique_ptr<notification> ev) {
  _state(host_id, service_id).events[cat] = std::move(ev);
}

/**
 * @brief Drop the notification state attached to a resource.
 *
 * Must be called when a notifier is destroyed, otherwise its entry would leak.
 *
 * @param host_id The host id.
 * @param service_id The service id (0 for a host).
 */
void notification_manager::forget(uint64_t host_id, uint64_t service_id) {
  /* Guarded like checker::forget: a notifier can be destroyed after the
   * singleton has been torn down by unload(), in which case there is nothing
   * left to forget. */
  if (!_instance)
    return;
  _instance->_states.erase({host_id, service_id});
}

uint64_t notification_manager::notification_number(uint64_t host_id,
                                                   uint64_t service_id) const {
  auto it = _states.find({host_id, service_id});
  return it != _states.end() ? it->second.number : 0;
}

void notification_manager::set_notification_number(uint64_t host_id,
                                                   uint64_t service_id,
                                                   uint64_t number) {
  auto& st = _state(host_id, service_id);
  /* Only notify the backend when the number actually changes. */
  if (st.number == number)
    return;
  st.number = number;
  if (_callbacks)
    _callbacks->on_notification_number_changed(host_id, service_id);
}

void notification_manager::inc_notification_number(uint64_t host_id,
                                                   uint64_t service_id) {
  ++_state(host_id, service_id).number;
}

uint64_t notification_manager::current_notification_id(
    uint64_t host_id,
    uint64_t service_id) const {
  auto it = _states.find({host_id, service_id});
  return it != _states.end() ? it->second.current_id : 0;
}

void notification_manager::set_current_notification_id(uint64_t host_id,
                                                       uint64_t service_id,
                                                       uint64_t id) {
  _state(host_id, service_id).current_id = id;
}

std::time_t notification_manager::last_notification(uint64_t host_id,
                                                    uint64_t service_id) const {
  auto it = _states.find({host_id, service_id});
  return it != _states.end() ? it->second.last : 0;
}

void notification_manager::set_last_notification(uint64_t host_id,
                                                 uint64_t service_id,
                                                 std::time_t t) {
  _state(host_id, service_id).last = t;
}

std::time_t notification_manager::next_notification(uint64_t host_id,
                                                    uint64_t service_id) const {
  auto it = _states.find({host_id, service_id});
  return it != _states.end() ? it->second.next : 0;
}

void notification_manager::set_next_notification(uint64_t host_id,
                                                 uint64_t service_id,
                                                 std::time_t t) {
  _state(host_id, service_id).next = t;
}

std::time_t notification_manager::initial_notif_time(
    uint64_t host_id,
    uint64_t service_id) const {
  auto it = _states.find({host_id, service_id});
  return it != _states.end() ? it->second.initial : 0;
}

void notification_manager::set_initial_notif_time(uint64_t host_id,
                                                  uint64_t service_id,
                                                  std::time_t t) {
  _state(host_id, service_id).initial = t;
}

}  // namespace com::centreon::engine::notifications
