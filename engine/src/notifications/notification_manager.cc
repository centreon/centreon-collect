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


#include <cassert>

#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/notifier.hh"
#include "com/centreon/engine/timezone_locker.hh"
#include "engine/src/notifications/notification.hh"

namespace com::centreon::engine::notifications {

notification_manager* notification_manager::_instance = nullptr;

notification_manager::notification_manager() = default;

/**
 * @brief Get the unique instance of the notification manager.
 *
 * This singleton does not follow the C++ Meyers idiom on purpose: we need to
 * control when it is destroyed (see init()/deinit() and the class doc).
 *
 * @return A reference to the notification_manager singleton.
 */
notification_manager& notification_manager::instance() {
  assert(_instance);
  return *_instance;
}

/**
 * @brief Create the notification_manager singleton.
 *
 * Must be called once at startup, before any notifier is created.
 */
void notification_manager::init() {
  if (!_instance)
    _instance = new notification_manager();
}

/**
 * @brief Destroy the notification_manager singleton.
 *
 * Must be called at shutdown, after all notifiers have been destroyed.
 */
void notification_manager::deinit() {
  if (_instance) {
    delete _instance;
    _instance = nullptr;
  }
}

/**
 * @brief Get the next notification ID and increment the internal counter.
 *
 * @return The next unique notification ID.
 */
uint64_t notification_manager::next_notification_id() noexcept {
  return _next_notification_id++;
}

/**
 * @brief Get the current value of the next notification ID without incrementing
 * it.
 *
 * @return The current value of the next notification ID.
 */
uint64_t notification_manager::get_next_notification_id() const noexcept {
  return _next_notification_id;
}

/**
 * @brief Map a notification reason to its notification category.
 *
 * @param type The notification reason.
 *
 * @return The matching notification category.
 */
notification_category notification_manager::get_category(reason_type type) {
  if (type == reason_custom)
    return cat_custom;
  notification_category cat[] = {
      cat_normal,   cat_recovery, cat_acknowledgement, cat_flapping,
      cat_flapping, cat_flapping, cat_downtime,        cat_downtime,
      cat_downtime, cat_custom};
  return cat[static_cast<size_t>(type)];
}

/**
 * @brief Dispatch the viability check to the handler matching the category.
 *
 * @param n The notifier the notification is about.
 * @param cat The notification category.
 * @param type The notification reason.
 * @param options The notification options.
 *
 * @return true if the notification is viable.
 */
bool notification_manager::is_notification_viable(notifier& n,
                                                  notification_category cat,
                                                  reason_type type,
                                                  notification_option options) {
  switch (cat) {
    case cat_normal:
      return _is_notification_viable_normal(n, type, options);
    case cat_recovery:
      return _is_notification_viable_recovery(n, type, options);
    case cat_acknowledgement:
      return _is_notification_viable_acknowledgement(n, type, options);
    case cat_flapping:
      return _is_notification_viable_flapping(n, type, options);
    case cat_downtime:
      return _is_notification_viable_downtime(n, type, options);
    case cat_custom:
      return _is_notification_viable_custom(n, type, options);
  }
  return false;
}

bool notification_manager::_is_notification_viable_normal(
    notifier& n,
    reason_type type __attribute__((unused)),
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::is_notification_viable_normal()");

  /* forced notifications bust through everything */
  notification* normal_notif = current_notification(&n, cat_normal);
  uint32_t notification_interval =
      !normal_notif ? n.get_notification_interval()
                    : normal_notif->get_notification_interval();

  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  /* are notifications enabled? */
  bool enable_notifications = pb_indexed_config.state().enable_notifications();
  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  /* are notifications temporarily disabled for this notifier? */
  if (!n.get_notifications_enabled()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  /* if this notifier is currently in a scheduled downtime period, don't send
   * the notification */
  if (n.is_in_downtime()) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier is currently in a scheduled downtime, so "
        "we won't send notifications.");
    return false;
  }

  timeperiod* tp{n.get_notification_timeperiod()};
  timezone_locker lock{n.get_timezone()};
  time_t now;
  time(&now);

  if (!check_time_against_period_for_notif(now, tp)) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "This notifier shouldn't have notifications sent out "
                        "at this time.");
    return false;
  }

  /* if this notifier is flapping, don't send the notification */
  if (n.get_is_flapping()) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier is flapping, so we won't send notifications.");
    return false;
  }

  /* On volatile services notifications are always sent */
  if (n.get_is_volatile()) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This is a volatile service notification, so it is sent.");
    return true;
  }

  if (n.get_state_type() != checkable::hard) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier is in soft state, so we won't send notifications.");
    return false;
  }

  if (n.problem_has_been_acknowledged()) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier problem has been acknowledged, so we won't send "
        "notifications.");
    return false;
  }

  if (n.get_current_state_int() == 0) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "We don't send a normal notification when the state is ok/up");
    return false;
  }

  if (!n.get_notify_on_current_state()) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier is unable to notify the state {}: not configured for "
        "that or, for a service, its host may be down",
        n.get_current_state_as_string());
    return false;
  }

  uint32_t interval_length = pb_indexed_config.state().interval_length();
  if (n.get_first_notification_delay() > 0 && !normal_notif &&
      n.get_last_hard_state_change() +
              n.get_first_notification_delay() * interval_length >
          now) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier is configured with a first notification delay, we "
        "won't send notification until timestamp {}",
        n.get_first_notification_delay() * interval_length);
    return false;
  }

  if (!n.authorized_by_dependencies(dependency::notification)) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "This notifier won't send any notification since it depends on"
        " another notifier that has already sent one");
    return false;
  }

  if (normal_notif) {
    std::time_t last_notif = last_notification(&n);
    /* In the case of a state change, we don't care of the notification interval
     * and we notify as soon as we can */
    if (n.get_last_hard_state_change() <= last_notif) {
      if (notification_interval == 0) {
        SPDLOG_LOGGER_DEBUG(
            notifications_logger,
            "This notifier problem has already been sent at {} so, since the "
            "notification interval is 0, it won't be sent anymore",
            last_notif);
        return false;
      } else if (notification_interval > 0) {
        if (last_notif + notification_interval * interval_length > now) {
          SPDLOG_LOGGER_DEBUG(
              notifications_logger,
              "This notifier problem has been sent at {} so it won't be sent "
              "until {}",
              last_notif, notification_interval * interval_length);
          return false;
        }
      }
    }
  }
  return true;
}

bool notification_manager::_is_notification_viable_recovery(
    notifier& n,
    reason_type type __attribute__((unused)),
    notification_option options __attribute__((unused))) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::is_notification_viable_recovery()");
  bool retval{true};
  bool send_later{false};

  bool enable_notifications = pb_indexed_config.state().enable_notifications();
  /* are notifications enabled? */
  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    retval = false;
  }
  /* are notifications temporarily disabled for this notifier? */
  else if (!n.get_notifications_enabled()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    retval = false;
  } else {
    timeperiod* tp{n.get_notification_timeperiod()};
    timezone_locker lock{n.get_timezone()};
    std::time_t now;
    std::time(&now);

    uint32_t interval_length = pb_indexed_config.state().interval_length();
    bool use_send_recovery_notifications_anyways =
        pb_indexed_config.state().send_recovery_notifications_anyways();

    // if use_send_recovery_notifications_anyways flag is set, we don't take
    // timeperiod into account for recovery
    if (!check_time_against_period_for_notif(now, tp)) {
      if (use_send_recovery_notifications_anyways) {
        SPDLOG_LOGGER_DEBUG(notifications_logger,
                            "send_recovery_notifications_anyways flag enabled, "
                            "recovery notification is viable even if we are "
                            "out of timeperiod at this time.");
      } else {
        SPDLOG_LOGGER_DEBUG(
            notifications_logger,
            "This notifier shouldn't have notifications sent out "
            "at this time.");
        retval = false;
        send_later = true;
      }
    }

    /* if this notifier is currently in a scheduled downtime period, don't send
     * the notification */
    else if (n.is_in_downtime()) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "This notifier is currently in a scheduled downtime, so "
          "we won't send notifications.");
      retval = false;
      send_later = true;
    }
    /* if this notifier is flapping, don't send the notification */
    else if (n.get_is_flapping()) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "This notifier is flapping, so we won't send notifications.");
      retval = false;
      send_later = true;
    } else if (n.get_state_type() != checkable::hard) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "This notifier is in soft state, so we won't send notifications.");
      retval = false;
      send_later = true;
    }
    /* Recovery is sent on state OK or UP */
    else if (n.get_current_state_int() != 0) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "This notifier state is not UP/OK to send a recovery notification");
      retval = false;
      send_later = true;
    } else if (!(n.get_notify_on(up) || n.get_notify_on(ok))) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "This notifier is not configured to send a recovery notification");
      retval = false;
      send_later = false;
    } else if (n.get_last_hard_state_change() +
                   n.get_recovery_notification_delay() * interval_length >
               now) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "This notifier is configured with a recovery notification delay. "
          "It won't send any recovery notification until timestamp "
          "so it won't be sent until {}",
          n.get_last_hard_state_change() + n.get_recovery_notification_delay());
      retval = false;
      send_later = true;
    } else if (notification_number(&n) == 0) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "No notification has been sent to "
          "announce a problem. So no recovery notification will be sent");
      retval = false;
    } else if (!current_notification(&n, cat_normal)) {
      SPDLOG_LOGGER_DEBUG(notifications_logger,
                          "We should not send a notification "
                          "since no normal notification has"
                          " been sent before");
      retval = false;
    }
  }

  if (!retval) {
    if (!send_later) {
      _state(&n).events[cat_normal].reset();
      SPDLOG_LOGGER_TRACE(
          notifications_logger,
          " _notification_number _is_notification_viable_recovery: {} => 0",
          notification_number(&n));
      set_notification_number(&n, 0);
    }
  }

  return retval;
}

bool notification_manager::_is_notification_viable_acknowledgement(
    notifier& n,
    reason_type type __attribute__((unused)),
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::is_notification_viable_acknowledgement()");
  /* forced notifications bust through everything */
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  bool enable_notifications = pb_indexed_config.state().enable_notifications();
  /* are notifications enabled? */
  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  /* are notifications temporarily disabled for this notifier? */
  if (!n.get_notifications_enabled()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  if (n.get_current_state_int() == 0) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "The notifier is currently OK/UP, so we "
                        "won't send an acknowledgement.");
    return false;
  }
  return true;
}

bool notification_manager::_is_notification_viable_flapping(
    notifier& n,
    reason_type type,
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::is_notification_viable_flapping()");
  /* forced notifications bust through everything */
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  /* are notifications enabled? */
  bool enable_notifications = pb_indexed_config.state().enable_notifications();
  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  /* are notifications temporarily disabled for this notifier? */
  if (!n.get_notifications_enabled()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
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

  if (!n.get_notify_on(f)) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "We shouldn't notify about {} events for this notifier.",
        tab_notification_str[type]);
    return false;
  }

  /* Don't send a start notification if a flapping notification is already there
   */
  notification* flapping_notif = current_notification(&n, cat_flapping);
  if (type == reason_flappingstart && flapping_notif) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "A flapping notification is already running, we can not send "
        "a start notification now.");
    return false;
    /* Don't send a stop/cancel notification if the previous flapping
     * notification is not a start flapping */
  } else if (type == reason_flappingstop || type == reason_flappingdisabled) {
    if (!flapping_notif ||
        flapping_notif->get_reason() != reason_flappingstart) {
      SPDLOG_LOGGER_DEBUG(
          notifications_logger,
          "A stop or cancellation flapping notification can only be sent "
          "after a start flapping notification.");
      return false;
    }
  }

  /* Don't send a notification if the same has already been sent previously. */
  if (flapping_notif && flapping_notif->get_reason() == type) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "We shouldn't notify about a {} event: already sent.",
                        tab_notification_str[type]);
    return false;
  }

  /* Don't send notifications during scheduled downtime */
  if (n.is_in_downtime()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "We shouldn't notify about FLAPPING "
                        "events during scheduled downtime.");
    return false;
  }
  return true;
}

bool notification_manager::_is_notification_viable_downtime(
    notifier& n,
    reason_type type __attribute__((unused)),
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::is_notification_viable_downtime()");

  /* forced notifications bust through everything */
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  /* are notifications enabled? */
  bool enable_notifications = pb_indexed_config.state().enable_notifications();
  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  /* are notifications temporarily disabled for this notifier? */
  if (!n.get_notifications_enabled()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "Notifications are disabled, so notifications won't be sent out.");
    return false;
  }

  /* Don't send a notification if we are not supposed to */
  if (!n.get_notify_on(downtime)) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "We shouldn't notify about DOWNTIME events for this notifier.");
    return false;
  }

  /* Don't send notifications during scheduled downtime (in the case of a
   * service, we don't care of the host, so the use of
   * get_scheduled_downtime_depth()) */
  if (n.get_scheduled_downtime_depth() > 0) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "We shouldn't notify about DOWNTIME "
                        "events during scheduled downtime.");
    return false;
  }
  return true;
}

bool notification_manager::_is_notification_viable_custom(
    notifier& n,
    reason_type type __attribute__((unused)),
    notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::is_notification_viable_custom()");
  /* forced notifications bust through everything */
  if (options & notification_option_forced) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "This is a forced notification, so we'll send it out.");
    return true;
  }

  /* are notifications enabled? */
  bool enable_notifications = pb_indexed_config.state().enable_notifications();
  if (!enable_notifications) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are disabled, so notifications will "
                        "not be sent out.");
    return false;
  }

  /* are notifications temporarily disabled for this notifier? */
  if (!n.get_notifications_enabled()) {
    SPDLOG_LOGGER_DEBUG(notifications_logger,
                        "Notifications are temporarily disabled for "
                        "this notifier, so we won't send one out.");
    return false;
  }

  /* Don't send notifications during scheduled downtime */
  if (n.is_in_downtime()) {
    SPDLOG_LOGGER_DEBUG(
        notifications_logger,
        "We shouldn't send a CUSTOM notification during scheduled downtime.");
    return false;
  }
  return true;
}

int32_t notification_manager::notify(
    notifier& n,
    notifications::reason_type type,
    const std::string& not_author,
    const std::string& not_data,
    notifications::notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger, "notification_manager::notify({})",
                      static_cast<uint32_t>(type));
  notifications::notification_category cat = get_category(type);

  /* Has this notification got sense? */
  if (!is_notification_viable(n, cat, type, options))
    return OK;

  /* For a first notification, we store what type of notification we try to
   * send and we fix the notification number to 1. */

  if (type != notifications::reason_recovery) {
    SPDLOG_LOGGER_TRACE(notifications_logger,
                        "_notification_number notify: {} -> {}",
                        notification_number(&n), notification_number(&n) + 1);
    inc_notification_number(&n);
  }

  /* What are the contacts to notify? */
  uint32_t notification_interval;
  bool escalated;
  std::unordered_set<std::shared_ptr<contact>> to_notify =
      n.get_contacts_to_notify(cat, type, notification_interval, escalated);

  uint64_t current_id = next_notification_id();
  set_current_notification_id(&n, current_id);
  auto notif = std::make_unique<notification>(
      &n, type, not_author, not_data, options, current_id,
      notification_number(&n), notification_interval, escalated);

  /* Let's make the notification. */
  int retval{notif->execute(to_notify)};

  if (retval == OK) {
    if (!to_notify.empty())
      set_last_notification(&n, std::time(nullptr));

    notification_state& st = _state(&n);

    /* The notification has been sent.
     * Should we increment the notification number? */
    if (cat == notifications::cat_normal) {
      /* if normal notification, get contacts from the last notification for
       * notify this contact on recovery notification */
      if (st.events[cat])
        notif->add_contacts(st.events[cat]->get_contacts());

      st.events[cat] = std::move(notif);
    } else {
      st.events[cat] = std::move(notif);
      switch (cat) {
        case notifications::cat_recovery:
          st.events[notifications::cat_normal].reset();
          st.events[notifications::cat_recovery].reset();
          break;
        case notifications::cat_flapping:
          if (type == notifications::reason_flappingstop ||
              type == notifications::reason_flappingdisabled)
            st.events[notifications::cat_flapping].reset();
          break;
        case notifications::cat_downtime:
          if (type == notifications::reason_downtimeend ||
              type == notifications::reason_downtimecancelled)
            st.events[notifications::cat_downtime].reset();
          break;
        default:
          st.events[cat].reset();
      }
      /* In case of an acknowledgement, we must keep the _notification_number
       * otherwise the recovery notification won't be sent when needed. */
      if (cat != notifications::cat_acknowledgement &&
          cat != notifications::cat_downtime) {
        SPDLOG_LOGGER_TRACE(notifications_logger,
                            "_notification_number notify: {} => 0",
                            notification_number(&n));
        set_notification_number(&n, 0);
      }
    }
  }

  return retval;
}

/**
 * @brief Get the live notification event of the given category for a notifier.
 *
 * Read-only lookup: it never inserts an empty slot in the map.
 *
 * @param n The notifier.
 * @param cat The notification category.
 *
 * @return The notification pointer, or nullptr if none is stored.
 */
notification* notification_manager::current_notification(
    notifier* n,
    notification_category cat) const {
  auto it = _states.find(n);
  return it != _states.end() ? it->second.events[cat].get() : nullptr;
}

/**
 * @brief Get (creating it if needed) the runtime notification state of a
 * notifier.
 */
notification_manager::notification_state& notification_manager::_state(
    notifier* n) {
  return _states[n];
}

/**
 * @brief Get a snapshot of the six notification slots of a notifier.
 *
 * @param n The notifier.
 *
 * @return An array of (non-owning) notification pointers indexed by
 * notification_category; nullptr where no notification is stored.
 */
std::array<notification*, 6> notification_manager::current_notifications(
    const notifier* n) const {
  std::array<notification*, 6> retval{};
  for (int i = 0; i < 6; i++)
    retval[i] = current_notification(const_cast<notifier*>(n),
                                     static_cast<notification_category>(i));
  return retval;
}

/**
 * @brief Store a notification event for a notifier/category (retention
 * restore).
 *
 * @param n The notifier.
 * @param cat The notification category.
 * @param ev The notification event to store (ownership transferred).
 */
void notification_manager::set_notification(notifier* n,
                                            notification_category cat,
                                            std::unique_ptr<notification> ev) {
  _state(n).events[cat] = std::move(ev);
}

/**
 * @brief Drop every notification event attached to a notifier.
 *
 * Must be called when a notifier is destroyed, otherwise its entries would
 * leak and keep a dangling notifier pointer as key.
 *
 * @param n The notifier being forgotten.
 */
void notification_manager::forget(notifier* n) {
  /* Guarded like checker::forget: a notifier can be destroyed after the
   * singleton has been torn down by deinit(), in which case there is nothing
   * left to forget. */
  if (!_instance)
    return;
  _instance->_states.erase(n);
}

uint64_t notification_manager::notification_number(const notifier* n) const {
  auto it = _states.find(const_cast<notifier*>(n));
  return it != _states.end() ? it->second.number : 0;
}

void notification_manager::set_notification_number(notifier* n,
                                                   uint64_t number) {
  _state(n).number = number;
}

void notification_manager::inc_notification_number(notifier* n) {
  ++_state(n).number;
}

uint64_t notification_manager::current_notification_id(
    const notifier* n) const {
  auto it = _states.find(const_cast<notifier*>(n));
  return it != _states.end() ? it->second.current_id : 0;
}

void notification_manager::set_current_notification_id(notifier* n,
                                                       uint64_t id) {
  _state(n).current_id = id;
}

std::time_t notification_manager::last_notification(const notifier* n) const {
  auto it = _states.find(const_cast<notifier*>(n));
  return it != _states.end() ? it->second.last : 0;
}

void notification_manager::set_last_notification(notifier* n, std::time_t t) {
  _state(n).last = t;
}

std::time_t notification_manager::next_notification(const notifier* n) const {
  auto it = _states.find(const_cast<notifier*>(n));
  return it != _states.end() ? it->second.next : 0;
}

void notification_manager::set_next_notification(notifier* n, std::time_t t) {
  _state(n).next = t;
}

std::time_t notification_manager::initial_notif_time(const notifier* n) const {
  auto it = _states.find(const_cast<notifier*>(n));
  return it != _states.end() ? it->second.initial : 0;
}

void notification_manager::set_initial_notif_time(notifier* n, std::time_t t) {
  _state(n).initial = t;
}

}  // namespace com::centreon::engine::notifications
