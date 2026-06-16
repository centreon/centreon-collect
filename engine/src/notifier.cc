/**
 * Copyright 2011-2024 Centreon
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

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/notification.hh"
#include "com/centreon/engine/timezone_locker.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;

notifier::notifier(notifications::notifier_type notifier_type,
                   const std::string& name,
                   std::string const& display_name,
                   std::string const& check_command,
                   bool checks_enabled,
                   bool accept_passive_checks,
                   uint32_t check_interval,
                   uint32_t retry_interval,
                   uint32_t notification_interval,
                   int max_attempts,
                   int32_t notify,
                   int32_t stalk,
                   uint32_t first_notification_delay,
                   uint32_t recovery_notification_delay,
                   std::string const& notification_period,
                   bool notifications_enabled,
                   std::string const& check_period,
                   std::string const& event_handler,
                   bool event_handler_enabled,
                   std::string const& notes,
                   std::string const& notes_url,
                   std::string const& action_url,
                   std::string const& icon_image,
                   std::string const& icon_image_alt,
                   bool flap_detection_enabled,
                   double low_flap_threshold,
                   double high_flap_threshold,
                   bool check_freshness,
                   int freshness_threshold,
                   bool obsess_over,
                   std::string const& timezone,
                   bool retain_status_information,
                   bool retain_nonstatus_information,
                   bool is_volatile,
                   uint64_t icon_id)
    : checkable{name,
                display_name,
                check_command,
                checks_enabled,
                accept_passive_checks,
                check_interval,
                retry_interval,
                max_attempts,
                check_period,
                event_handler,
                event_handler_enabled,
                notes,
                notes_url,
                action_url,
                icon_image,
                icon_image_alt,
                flap_detection_enabled,
                low_flap_threshold,
                high_flap_threshold,
                check_freshness,
                freshness_threshold,
                obsess_over,
                timezone,
                is_volatile,
                icon_id},
      _notifier_type{notifier_type},
      _stalk_type{stalk},
      _flap_type{0},
      _initial_notif_time{0},
      _acknowledgement_timeout{0},
      _last_acknowledgement{0},
      _out_notification_type{notify},
      _current_notifications{0},
      _notification_interval{notification_interval},
      _modified_attributes{0},
      _current_notification_id{0UL},
      _next_notification{0UL},
      _last_notification{0UL},
      _notification_period{notification_period},
      _notification_period_ptr{nullptr},
      _first_notification_delay{first_notification_delay},
      _recovery_notification_delay{recovery_notification_delay},
      _notifications_enabled{notifications_enabled},
      _no_more_notifications{false},
      _flapping_comment_id{0},
      _acknowledgement_comment_id{0},
      _check_options{CHECK_OPTION_NONE},
      _acknowledgement_type{AckType::NONE},
      _retain_status_information{retain_status_information},
      _retain_nonstatus_information{retain_nonstatus_information},
      _is_being_freshened{false},
      _notification_to_interval_on_timeperiod_in{false},
      _notification_number{0},
      _notification{{}},
      _state_history{{}},
      _pending_flex_downtime{0} {
  if (retry_interval <= 0) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Invalid notification_interval value for notifier '{}'",
        display_name);
    throw engine_error() << "Could not register notifier '" << display_name
                         << "'";
  }
}

notifier::~notifier() {
  checks::checker::forget(this);
}

/**
 * @brief Set the current notification number and send this update to Broker.
 *
 * @param num The notification number.
 */
void notifier::set_notification_number(int num) {
  SPDLOG_LOGGER_TRACE(notifications_logger,
                      "_notification_number set_notification_number: {} => {}",
                      _notification_number, num);
  /* set the notification number */
  _notification_number = num;

  /* update the status log with the notifier info */
  update_status(notifications::STATUS_NOTIFICATION_NUMBER);
}

/**
 * @brief Select contacts to notify. Also return the notification_interval and
 * a boolean value escalated to know if an escalation is active.
 *
 * @param cat
 * @param type
 * @param[out] notification_interval
 * @param[out] escalated
 *
 * @return A set of contacts to notify.
 */
std::unordered_set<std::shared_ptr<contact>> notifier::get_contacts_to_notify(
    notifications::notification_category cat,
    notifications::reason_type type,
    uint32_t& notification_interval,
    bool& escalated) {
  std::unordered_set<std::shared_ptr<contact>> retval;
  escalated = false;
  uint32_t notif_interv{_notification_interval};

  /* Let's start looking at escalations */
  for (auto* e : _escalations) {
    if (e->is_viable(get_current_state_int(), _notification_number)) {
      /* Among escalations, we choose the smallest notification interval. */
      if (escalated) {
        if (e->get_notification_interval() < notif_interv)
          notif_interv = e->get_notification_interval();
      } else {
        /* Here is the first escalation, so we take its notification_interval.
         */
        escalated = true;
        notif_interv = e->get_notification_interval();
      }

      /* For each contact group, we also add its contacts. */
      for (contactgroup_map::const_iterator
               cgit = e->get_contactgroups().begin(),
               cgend = e->get_contactgroups().end();
           cgit != cgend; ++cgit) {
        for (contact_map::const_iterator
                 cit = cgit->second->get_members().begin(),
                 cend = cgit->second->get_members().end();
             cit != cend; ++cit) {
          assert(cit->second);
          if (cit->second->should_be_notified(cat, type, *this))
            retval.insert(cit->second);
        }
      }
    }
  }

  if (!escalated) {
    /* Construction of the set containing contacts to notify. We don't know
     * for the moment if those contacts accept notification. */
    for (auto it = contacts().begin(), end = contacts().end(); it != end;
         ++it) {
      assert(it->second);
      if (it->second->should_be_notified(cat, type, *this))
        retval.insert(it->second);
    }

    /* For each contact group, we also add its contacts. */
    for (contactgroup_map::const_iterator it = get_contactgroups().begin(),
                                          end = get_contactgroups().end();
         it != end; ++it) {
      for (contact_map::const_iterator cit = it->second->get_members().begin(),
                                       cend = it->second->get_members().end();
           cit != cend; ++cit) {
        assert(cit->second);
        if (cit->second->should_be_notified(cat, type, *this))
          retval.insert(cit->second);
      }
    }
  }
  notification_interval = notif_interv;
  return retval;
}

notifications::notification_category notifier::get_category(
    notifications::reason_type type) {
  if (type == 99)
    return notifications::cat_custom;
  notifications::notification_category cat[] = {
      notifications::cat_normal,          notifications::cat_recovery,
      notifications::cat_acknowledgement, notifications::cat_flapping,
      notifications::cat_flapping,        notifications::cat_flapping,
      notifications::cat_downtime,        notifications::cat_downtime,
      notifications::cat_downtime,        notifications::cat_custom};
  return cat[static_cast<size_t>(type)];
}

bool notifier::is_notification_viable(
    notifications::notification_category cat,
    notifications::reason_type type,
    notifications::notification_option options) {
  return notifications::notification_manager::instance().is_notification_viable(
      *this, cat, type, options);
}

int notifier::notify(notifications::reason_type type,
                     std::string const& not_author,
                     std::string const& not_data,
                     notifications::notification_option options) {
  SPDLOG_LOGGER_TRACE(functions_logger, "notifier::notify({})",
                      static_cast<uint32_t>(type));
  notifications::notification_category cat{get_category(type)};

  /* Has this notification got sense? */
  if (!is_notification_viable(cat, type, options))
    return OK;

  /* For a first notification, we store what type of notification we try to
   * send and we fix the notification number to 1. */
  if (type != notifications::reason_recovery) {
    SPDLOG_LOGGER_TRACE(notifications_logger,
                        "_notification_number notify: {} -> {}",
                        _notification_number, _notification_number + 1);
    ++_notification_number;
  }

  /* What are the contacts to notify? */
  uint32_t notification_interval;
  bool escalated;
  std::unordered_set<std::shared_ptr<contact>> to_notify =
      get_contacts_to_notify(cat, type, notification_interval, escalated);

  _current_notification_id =
      notifications::notification_manager::instance().next_notification_id();
  auto notif = std::make_unique<notification_ev>(
      this, type, not_author, not_data, options, _current_notification_id,
      _notification_number, notification_interval, escalated);

  /* Let's make the notification. */
  int retval{notif->execute(to_notify)};

  if (retval == OK) {
    if (!to_notify.empty())
      _last_notification = std::time(nullptr);

    /* The notification has been sent.
     * Should we increment the notification number? */
    if (cat == notifications::cat_normal) {
      /* if normal notification, get contacts from the last notification for
       * notify this contact on recovery notification */
      notification_ev* normal_notif =
          _notification[notifications::cat_normal].get();
      if (normal_notif)
        notif->add_contacts(normal_notif->get_contacts());

      _notification[cat] = std::move(notif);
    } else {
      _notification[cat] = std::move(notif);
      switch (cat) {
        case notifications::cat_recovery:
          _notification[notifications::cat_normal].reset();
          _notification[notifications::cat_recovery].reset();
          break;
        case notifications::cat_flapping:
          if (type == notifications::reason_flappingstop ||
              type == notifications::reason_flappingdisabled)
            _notification[notifications::cat_flapping].reset();
          break;
        case notifications::cat_downtime:
          if (type == notifications::reason_downtimeend ||
              type == notifications::reason_downtimecancelled)
            _notification[notifications::cat_downtime].reset();
          break;
        default:
          _notification[cat].reset();
      }
      /* In case of an acknowledgement, we must keep the _notification_number
       * otherwise the recovery notification won't be sent when needed. */
      if (cat != notifications::cat_acknowledgement &&
          cat != notifications::cat_downtime) {
        SPDLOG_LOGGER_TRACE(notifications_logger,
                            "_notification_number notify: {} => 0",
                            _notification_number);
        _notification_number = 0;
      }
    }
  }

  return retval;
}

void notifier::set_current_notification_id(uint64_t id) noexcept {
  _current_notification_id = id;
}

uint64_t notifier::get_current_notification_id() const noexcept {
  return _current_notification_id;
}

time_t notifier::get_next_notification() const noexcept {
  return _next_notification;
}

void notifier::set_next_notification(time_t next_notification) noexcept {
  _next_notification = next_notification;
}

time_t notifier::get_last_notification() const noexcept {
  return _last_notification;
}

void notifier::set_last_notification(time_t last_notification) noexcept {
  _last_notification = last_notification;
}

void notifier::set_initial_notif_time(time_t notif_time) noexcept {
  _initial_notif_time = notif_time;
}

time_t notifier::get_initial_notif_time() const noexcept {
  return _initial_notif_time;
}

void notifier::set_acknowledgement_timeout(int timeout) noexcept {
  _acknowledgement_timeout = timeout;
}

void notifier::set_last_acknowledgement(time_t ack) noexcept {
  _last_acknowledgement = ack;
}

time_t notifier::last_acknowledgement() const noexcept {
  return _last_acknowledgement;
}

uint32_t notifier::get_notification_interval(void) const noexcept {
  return _notification_interval;
}

void notifier::set_notification_interval(
    uint32_t notification_interval) noexcept {
  _notification_interval = notification_interval;
}

std::string const& notifier::notification_period() const noexcept {
  return _notification_period;
}

void notifier::set_notification_period(
    std::string const& notification_period) noexcept {
  _notification_period = notification_period;
}

bool notifier::get_notify_on(
    notifications::notification_flag type) const noexcept {
  return _out_notification_type & type;
}

uint32_t notifier::get_notify_on() const noexcept {
  return _out_notification_type;
}

void notifier::add_notify_on(notifications::notification_flag type) noexcept {
  _out_notification_type |= type;
}

void notifier::set_notify_on(uint32_t type) noexcept {
  _out_notification_type = type;
}

void notifier::remove_notify_on(
    notifications::notification_flag type) noexcept {
  _out_notification_type &= ~type;
}

uint32_t notifier::get_first_notification_delay(void) const noexcept {
  return _first_notification_delay;
}

void notifier::set_first_notification_delay(
    uint32_t first_notification_delay) noexcept {
  _first_notification_delay = first_notification_delay;
}

uint32_t notifier::get_recovery_notification_delay(void) const noexcept {
  return _recovery_notification_delay;
}

void notifier::set_recovery_notification_delay(
    uint32_t recovery_notification_delay) noexcept {
  _recovery_notification_delay = recovery_notification_delay;
}

bool notifier::get_notifications_enabled() const noexcept {
  return _notifications_enabled;
}

void notifier::set_notifications_enabled(bool notifications_enabled) noexcept {
  _notifications_enabled = notifications_enabled;
}

bool notifier::get_notified_on(
    notifications::notification_flag type) const noexcept {
  return _current_notifications & type;
}

uint32_t notifier::get_notified_on() const noexcept {
  return _current_notifications;
}

void notifier::add_notified_on(notifications::notification_flag type) noexcept {
  _current_notifications |= type;
}

void notifier::set_notified_on(uint32_t type) noexcept {
  _current_notifications = type;
}

void notifier::remove_notified_on(
    notifications::notification_flag type) noexcept {
  _current_notifications &= ~type;
}

bool notifier::get_flap_detection_on(
    notifications::notification_flag type) const noexcept {
  return _flap_type & type;
}

uint32_t notifier::get_flap_detection_on() const noexcept {
  return _flap_type;
}

void notifier::set_flap_detection_on(uint32_t type) noexcept {
  _flap_type = type;
}

void notifier::add_flap_detection_on(
    notifications::notification_flag type) noexcept {
  _flap_type |= type;
}

bool notifier::get_stalk_on(
    notifications::notification_flag type) const noexcept {
  return _stalk_type & type;
}

uint32_t notifier::get_stalk_on() const noexcept {
  return _stalk_type;
}

void notifier::set_stalk_on(uint32_t type) noexcept {
  _stalk_type = type;
}

void notifier::add_stalk_on(notifications::notification_flag type) noexcept {
  _stalk_type |= type;
}

uint32_t notifier::get_modified_attributes() const noexcept {
  return _modified_attributes;
}

void notifier::set_modified_attributes(uint32_t modified_attributes) noexcept {
  _modified_attributes = modified_attributes;
}

void notifier::add_modified_attributes(uint32_t attr) noexcept {
  _modified_attributes |= attr;
}

std::list<escalation*>& notifier::get_escalations() noexcept {
  return _escalations;
}

std::list<escalation*> const& notifier::get_escalations() const noexcept {
  return _escalations;
}

uint64_t notifier::get_flapping_comment_id(void) const noexcept {
  return _flapping_comment_id;
}

void notifier::set_flapping_comment_id(uint64_t comment_id) noexcept {
  _flapping_comment_id = comment_id;
}

uint64_t notifier::get_acknowledgement_comment_id(void) const noexcept {
  return _acknowledgement_comment_id;
}

void notifier::set_acknowledgement_comment_id(uint64_t comment_id) noexcept {
  _acknowledgement_comment_id = comment_id;
}

/**
 * @brief Delete the non-persistent acknowledgement comment owned by this
 * notifier, if any. Persistent acknowledgement comments are never tracked here
 * (their id stays 0), so they survive an acknowledgement removal.
 */
void notifier::delete_acknowledgement_comment() noexcept {
  if (_acknowledgement_comment_id != 0) {
    comment::delete_comment(_acknowledgement_comment_id);
    _acknowledgement_comment_id = 0;
  }
}

int notifier::get_check_options(void) const noexcept {
  return _check_options;
}

void notifier::set_check_options(int option) noexcept {
  _check_options = option;
}

/**
 * @brief Tell if an acknowledgement is active on the notifier by returning its
 * type NONE (no acknowledgement), NORMAL or STICKY.
 *
 * @return A notifier::acknowledgement_type.
 */
AckType notifier::get_acknowledgement() const noexcept {
  return _acknowledgement_type;
}

/**
 * @brief Acknowledgement setter. The acknowledgement can be set to NONE,
 * STICKY or NORMAL.
 *
 * @param acknowledge_type The acknowledgement type.
 */
void notifier::set_acknowledgement(AckType acknowledge_type) noexcept {
  _acknowledgement_type = acknowledge_type;
}

int notifier::get_retain_status_information() const noexcept {
  return _retain_status_information;
}

void notifier::set_retain_status_information(
    bool retain_status_informations) noexcept {
  _retain_status_information = retain_status_informations;
}

bool notifier::get_retain_nonstatus_information(void) const noexcept {
  return _retain_nonstatus_information;
}

void notifier::set_retain_nonstatus_information(
    bool retain_non_status_informations) noexcept {
  _retain_nonstatus_information = retain_non_status_informations;
}

bool notifier::get_is_being_freshened(void) const noexcept {
  return _is_being_freshened;
}

void notifier::set_is_being_freshened(bool freshened) noexcept {
  _is_being_freshened = freshened;
}

/**
 * @brief Return if the notifier has been acknowledged.
 *
 * @return True if acknowledged, False otherwise.
 */
bool notifier::problem_has_been_acknowledged() const noexcept {
  return _acknowledgement_type != AckType::NONE;
}

bool notifier::get_no_more_notifications() const noexcept {
  return _no_more_notifications;
}

void notifier::set_no_more_notifications(bool no_more_notifications) noexcept {
  _no_more_notifications = no_more_notifications;
}

int notifier::get_notification_number() const noexcept {
  return _notification_number;
}

notifications::notifier_type notifier::get_notifier_type() const noexcept {
  return _notifier_type;
}

absl::flat_hash_map<std::string, std::shared_ptr<contact>>&
notifier::mut_contacts() noexcept {
  return _contacts;
}

const absl::flat_hash_map<std::string, std::shared_ptr<contact>>&
notifier::contacts() const noexcept {
  return _contacts;
}

contactgroup_map& notifier::get_contactgroups() noexcept {
  return _contact_groups;
}

const contactgroup_map& notifier::get_contactgroups() const noexcept {
  return _contact_groups;
}

/**
 *  Tests whether a contact is a contact for a particular notifier.
 *
 *  @param[in] notif Target notifier.
 *  @param[in] cntct Target contact.
 *
 *  @return true or false.
 */
bool is_contact_for_notifier(com::centreon::engine::notifier* notif,
                             contact* cntct) {
  if (!notif || !cntct)
    return false;

  // Search all individual contacts of this host.
  for (contact_map::const_iterator it = notif->contacts().begin(),
                                   end = notif->contacts().end();
       it != end; ++it)
    if (it->second.get() == cntct)
      return true;

  for (contactgroup_map::const_iterator it = notif->get_contactgroups().begin(),
                                        end = notif->get_contactgroups().end();
       it != end; ++it) {
    assert(it->second);
    if (it->second->get_members().find(cntct->get_name()) ==
        it->second->get_members().end())
      return true;
  }

  return false;
}

/**
 *  This method resolves pointers involved in this notifier life. If a pointer
 *  cannot be resolved, an exception is thrown.
 *
 * @param w Warnings given by the method.
 * @param e Errors given by the method. An exception is thrown is at less an
 * error is rised.
 */
void notifier::resolve(uint32_t& w, uint32_t& e) {
  uint32_t warnings = 0, errors = 0;

  /* This list will be filled in {hostescalation,serviceescalation}::resolve */
  _escalations.clear();

  /* check the event handler command */
  if (!event_handler().empty()) {
    size_t pos{event_handler().find_first_of('!')};
    std::string cmd_name{event_handler().substr(0, pos)};

    command_map::iterator cmd_found{commands::command::commands.find(cmd_name)};

    if (cmd_found == commands::command::commands.end() || !cmd_found->second) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "Error: Event handler command '{}' specified for host '{}' not "
          "defined anywhere",
          cmd_name, get_display_name());
      errors++;
    } else
      /* save the pointer to the event handler command for later */
      set_event_handler_ptr(cmd_found->second.get());
  }

  /* hosts that don't have check commands defined shouldn't ever be checked...
   */
  if (!check_command().empty()) {
    size_t pos{check_command().find_first_of('!')};
    std::string cmd_name{check_command().substr(0, pos)};

    command_map::iterator cmd_found{commands::command::commands.find(cmd_name)};

    if (cmd_found == commands::command::commands.end() || !cmd_found->second) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "Error: Notifier check command '{}' specified for host '{}' is not "
          "defined anywhere!",
          cmd_name, get_display_name());
      errors++;
    } else
      /* save the pointer to the check command for later */
      set_check_command_ptr(cmd_found->second);
  }

  if (check_period().empty()) {
    SPDLOG_LOGGER_WARN(
        config_logger,
        "Warning: Notifier '{}' has no check time period defined!",
        get_display_name());
    warnings++;
    check_period_ptr = nullptr;
  } else {
    timeperiod_map::const_iterator found_it{
        timeperiod::timeperiods.find(check_period())};

    if (found_it == timeperiod::timeperiods.end() || !found_it->second) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "Error: Check period '{}' specified for host '{}' is not defined "
          "anywhere!",
          check_period(), get_display_name());
      errors++;
      check_period_ptr = nullptr;
    } else
      /* save the pointer to the check timeperiod for later */
      check_period_ptr = found_it->second.get();
  }

  /* check all contacts */
  for (contact_map::iterator it = mut_contacts().begin(),
                             end = mut_contacts().end();
       it != end; ++it) {
    contact_map::const_iterator found_it{contact::contacts.find(it->first)};
    if (found_it == contact::contacts.end() || !found_it->second.get()) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "Error: Contact '{}' specified in notifier '{}' is not defined "
          "anywhere!",
          it->first, get_display_name());
      errors++;
    } else
      /* save the pointer to the contact */
      it->second = found_it->second;
  }

  /* check all contact groups */
  for (contactgroup_map::iterator it = get_contactgroups().begin(),
                                  end = get_contactgroups().end();
       it != end; ++it) {
    // Find the contact group.
    contactgroup_map::const_iterator found_it{
        contactgroup::contactgroups.find(it->first)};

    if (found_it == contactgroup::contactgroups.end()) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "Error: Contact group '{}' specified in host '{}' is not defined "
          "anywhere!",
          it->first, get_display_name());
      errors++;
    } else
      it->second = found_it->second;
  }

  // Check notification timeperiod.
  if (!notification_period().empty()) {
    timeperiod_map::const_iterator found_it{
        timeperiod::timeperiods.find(notification_period())};

    if (found_it == timeperiod::timeperiods.end() || !found_it->second.get()) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "Error: Notification period '{}' specified for notifier '{}' is not "
          "defined anywhere!",
          notification_period(), get_display_name());
      errors++;
      _notification_period_ptr = nullptr;
    } else
      // Save the pointer to the notification timeperiod for later.
      _notification_period_ptr = found_it->second.get();
  } else if (get_notifications_enabled()) {
    SPDLOG_LOGGER_WARN(
        config_logger,
        "Warning: Notifier '{}' has no notification time period defined!",
        get_display_name());
    warnings++;
    _notification_period_ptr = nullptr;
  }

  w += warnings;
  e += errors;

  if (e)
    throw engine_error() << "Cannot resolve host '" << get_display_name()
                         << "'";
}

std::array<int, MAX_STATE_HISTORY_ENTRIES> const& notifier::get_state_history()
    const {
  return _state_history;
}

std::array<int, MAX_STATE_HISTORY_ENTRIES>& notifier::get_state_history() {
  return _state_history;
}

std::array<std::unique_ptr<notification_ev>, 6> const&
notifier::get_current_notifications() const {
  return _notification;
}

int notifier::get_pending_flex_downtime() const {
  return _pending_flex_downtime;
}

void notifier::inc_pending_flex_downtime() noexcept {
  ++_pending_flex_downtime;
}

void notifier::dec_pending_flex_downtime() noexcept {
  --_pending_flex_downtime;
}

/**
 * @brief Calculates next acceptable re-notification time for this notifier.
 *
 * @param offset
 *
 * @return a timestamp
 */
time_t notifier::get_next_notification_time(time_t offset) {
  bool have_escalated_interval{false};

  SPDLOG_LOGGER_TRACE(functions_logger,
                      "notifier::get_next_notification_time()");
  SPDLOG_LOGGER_INFO(notifications_logger,
                     "Calculating next valid notification time...");

  /* default notification interval */
  uint32_t interval_to_use{_notification_interval};

  SPDLOG_LOGGER_INFO(notifications_logger, "Default interval: {}",
                     interval_to_use);

  /*
   * search all the escalation entries for valid matches for this service (at
   * its current notification number)
   */
  for (escalation const* e : get_escalations()) {
    /* interval < 0 means to use non-escalated interval */
    if (e->get_notification_interval() < 0.0)
      continue;

    /* skip this entry if it isn't appropriate */
    if (!is_valid_escalation_for_notification(
            e, notifications::notification_option_none))
      continue;

    SPDLOG_LOGGER_INFO(notifications_logger,
                       "Found a valid escalation w/ interval of {}",
                       e->get_notification_interval());

    /*
     * if we haven't used a notification interval from an escalation yet,
     * use this one
     */
    if (!have_escalated_interval) {
      have_escalated_interval = true;
      interval_to_use = e->get_notification_interval();
    }
    /* else use the shortest of all valid escalation intervals */
    else if (e->get_notification_interval() < interval_to_use)
      interval_to_use = e->get_notification_interval();

    SPDLOG_LOGGER_INFO(notifications_logger, "New interval: {}",
                       interval_to_use);
  }

  /*
   * if notification interval is 0, we shouldn't send any more problem
   * notifications (unless service is volatile)
   */
  if (interval_to_use == 0.0 && !get_is_volatile())
    set_no_more_notifications(true);
  else
    set_no_more_notifications(false);

  SPDLOG_LOGGER_INFO(notifications_logger,
                     "Interval used for calculating next valid "
                     "notification time: {}",
                     interval_to_use);

  /* calculate next notification time */
  uint32_t interval_length = pb_indexed_config.state().interval_length();
  time_t next_notification{
      offset + static_cast<time_t>(interval_to_use * interval_length)};

  return next_notification;
}

void notifier::set_flap_type(uint32_t type) noexcept {
  _flap_type = type;
}

timeperiod* notifier::get_notification_period_ptr() const noexcept {
  return _notification_period_ptr;
}

int notifier::acknowledgement_timeout() const noexcept {
  return _acknowledgement_timeout;
}

void notifier::set_notification_period_ptr(timeperiod* tp) noexcept {
  _notification_period_ptr = tp;
}

/**
 *  This method is called by the retention to restitute a notification.
 *
 * @param idx The index of the notification
 * @param value The notification under the form of a string
 */
void notifier::set_notification(int32_t idx, std::string const& value) {
  if (value.empty())
    return;

  char const* v = value.c_str();
  if (strncmp(v, "type: ", 6)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the line should start "
        "with 'type: '");
    return;
  }

  v += 6;
  char* next;
  notifications::reason_type type =
      static_cast<notifications::reason_type>(strtol(v, &next, 10));
  if (next == v || *next != ',' || next[1] != ' ') {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the separator between two "
        "fields is ', '");
    return;
  }

  v = next + 2;
  if (strncmp(v, "author: ", 8)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the expected field after "
        "'type' is 'author'");
    return;
  }

  v += 8;
  for (next = const_cast<char*>(v); *next && *next != ','; next++)
    ;
  std::string author(v, next - v);

  v = next + 2;
  if (strncmp(v, "options: ", 9)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the expected field after "
        "'author' is 'options'");
    return;
  }

  v += 9;
  int options = strtol(v, &next, 10);
  if (next == v || *next != ',' || next[1] != ' ') {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the separator between two "
        "fields is ', '");
    return;
  }

  v = next + 2;
  if (strncmp(v, "escalated: ", 11)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the expected field "
        " after 'options' is 'escalated'");
    return;
  }

  v += 11;
  bool escalated = static_cast<bool>(strtol(v, &next, 10));
  if (next == v || *next != ',' || next[1] != ' ') {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the separator between two "
        "fields is ', '");
    return;
  }

  v = next + 2;
  if (strncmp(v, "id: ", 4)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the expected field "
        " after 'escalated' is 'id'");
    return;
  }

  v += 4;
  int id = strtol(v, &next, 10);
  if (next == v || *next != ',' || next[1] != ' ') {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the separator between two "
        "fields is ', '");
    return;
  }

  v = next + 2;
  if (strncmp(v, "number: ", 8)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the expected field "
        " after 'id' is 'number'");
    return;
  }

  v += 8;
  int number = strtol(v, &next, 10);
  if (next == v || *next != ',' || next[1] != ' ') {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the separator between two "
        "fields is ', '");
    return;
  }

  v = next + 2;
  if (strncmp(v, "interval: ", 10)) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the expected field "
        " after 'number' is 'interval'");
    return;
  }

  v += 10;
  int interval = strtol(v, &next, 10);
  if (next == v) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "Error: Bad format in the notification part, the 'interval' value "
        "should be an integer");
    return;
  }

  v = next + 2;
  std::set<std::string> contacts;
  if (!strncmp(v, "contacts: ", 10)) {
    v += 10;
    for (const char* s = v; *s; ++s) {
      if ((*s == ',' || *s == '\n') && v != s) {
        contacts.emplace(v, s);
        v = s + 1;
      }
    }
  }
  _notification[idx] =
      std::make_unique<notification_ev>(this, type, author, "", options, id,
                                        number, interval, escalated, contacts);
}
