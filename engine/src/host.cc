/**
 * Copyright 2024 Centreon
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

#include <cassert>

#include <fmt/chrono.h>

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/notification.hh"
#include "com/centreon/engine/objects.hh"
#include "com/centreon/engine/sehandlers.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timezone_locker.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

std::array<std::pair<uint32_t, std::string>, 3> const host::tab_host_states{
    {{NSLOG_HOST_UP, "UP"},
     {NSLOG_HOST_DOWN, "DOWN"},
     {NSLOG_HOST_UNREACHABLE, "UNREACHABLE"}}};

host_map host::hosts;
host_id_map host::hosts_by_id;

/*
 *  @param[in] name                          Host name.
 *  @param[in] display_name                  Display name.
 *  @param[in] alias                         Host alias.
 *  @param[in] address                       Host address.
 *  @param[in] check_period                  Check period.
 *  @param[in] check_interval                Normal check interval.
 *  @param[in] retry_interval                Retry check interval.
 *  @param[in] max_attempts                  Max check attempts.
 *  @param[in] notify_up                     Does this host notify when
 *                                           up ?
 *  @param[in] notify_down                   Does this host notify when
 *                                           down ?
 *  @param[in] notify_unreachable            Does this host notify when
 *                                           unreachable ?
 *  @param[in] notify_flapping               Does this host notify for
 *                                           flapping ?
 *  @param[in] notify_downtime               Does this host notify for
 *                                           downtimes ?
 *  @param[in] notification_interval         Notification interval.
 *  @param[in] first_notification_delay      First notification delay.
 *  @param[in] notification_period           Notification period.
 *  @param[in] notifications_enabled         Whether notifications are
 *                                           enabled for this host.
 *  @param[in] check_command                 Active check command name.
 *  @param[in] checks_enabled                Are active checks enabled ?
 *  @param[in] accept_passive_checks         Can we submit passive check
 *                                           results ?
 *  @param[in] event_handler                 Event handler command name.
 *  @param[in] event_handler_enabled         Whether event handler is
 *                                           enabled or not.
 *  @param[in] flap_detection_enabled        Whether flap detection is
 *                                           enabled or not.
 *  @param[in] low_flap_threshold            Low flap threshold.
 *  @param[in] high_flap_threshold           High flap threshold.
 *  @param[in] flap_detection_on_up          Is flap detection enabled
 *                                           for up state ?
 *  @param[in] flap_detection_on_down        Is flap detection enabled
 *                                           for down state ?
 *  @param[in] flap_detection_on_unreachable Is flap detection enabled
 *                                           for unreachable state ?
 *  @param[in] stalk_on_up                   Stalk on up ?
 *  @param[in] stalk_on_down                 Stalk on down ?
 *  @param[in] stalk_on_unreachable          Stalk on unreachable ?
 *  @param[in] process_perfdata              Should host perfdata be
 *                                           processed ?
 *  @param[in] check_freshness               Whether or not freshness
 *                                           check is enabled.
 *  @param[in] freshness_threshold           Freshness threshold.
 *  @param[in] notes                         Notes.
 *  @param[in] notes_url                     URL.
 *  @param[in] action_url                    Action URL.
 *  @param[in] icon_image                    Icon image.
 *  @param[in] icon_image_alt                Alternative icon image.
 *  @param[in] vrml_image                    VRML image.
 *  @param[in] statusmap_image               Status-map image.
 *  @param[in] x_2d                          2D x-coord.
 *  @param[in] y_2d                          2D y-coord.
 *  @param[in] have_2d_coords                Whether host has 2D coords.
 *  @param[in] x_3d                          3D x-coord.
 *  @param[in] y_3d                          3D y-coord.
 *  @param[in] z_3d                          3D z-coord.
 *  @param[in] have_3d_coords                Whether host has 3D coords.
 *  @param[in] should_be_drawn               Whether this host should be
 *                                           drawn.
 *  @param[in] retain_status_information     Should Engine retain status
 *                                           information of this host ?
 *  @param[in] retain_nonstatus_information  Should Engine retain
 *                                           non-status information of
 *                                           this host ?
 *  @param[in] obsess_over_host              Should we obsess over this
 *                                           host ?
 *  @param[in] timezone                      The timezone to apply to the host
 */
host::host(uint64_t host_id,
           const std::string& name,
           const std::string& display_name,
           const std::string& alias,
           const std::string& address,
           const std::string& check_period,
           uint32_t check_interval,
           uint32_t retry_interval,
           int max_attempts,
           int notify_up,
           int notify_down,
           int notify_unreachable,
           int notify_flapping,
           int notify_downtime,
           uint32_t notification_interval,
           uint32_t first_notification_delay,
           uint32_t recovery_notification_delay,
           const std::string& notification_period,
           bool notifications_enabled,
           const std::string& check_command,
           bool checks_enabled,
           bool accept_passive_checks,
           const std::string& event_handler,
           bool event_handler_enabled,
           bool flap_detection_enabled,
           double low_flap_threshold,
           double high_flap_threshold,
           int flap_detection_on_up,
           int flap_detection_on_down,
           int flap_detection_on_unreachable,
           int stalk_on_up,
           int stalk_on_down,
           int stalk_on_unreachable,
           bool process_perfdata,
           bool check_freshness,
           int freshness_threshold,
           const std::string& notes,
           const std::string& notes_url,
           const std::string& action_url,
           const std::string& icon_image,
           const std::string& icon_image_alt,
           const std::string& vrml_image,
           const std::string& statusmap_image,
           double x_2d,
           double y_2d,
           bool have_2d_coords,
           double x_3d,
           double y_3d,
           double z_3d,
           bool have_3d_coords,
           bool should_be_drawn,
           bool retain_status_information,
           bool retain_nonstatus_information,
           bool obsess_over_host,
           const std::string& timezone,
           uint64_t icon_id)
    : notifier{host_notification,
               name,
               display_name,
               check_command,
               checks_enabled,
               accept_passive_checks,
               check_interval,
               retry_interval,
               notification_interval,
               max_attempts,
               (notify_up > 0 ? up : 0) | (notify_down > 0 ? down : 0) |
                   (notify_downtime > 0 ? downtime : 0) |
                   (notify_flapping > 0
                        ? (flappingstart | flappingstop | flappingdisabled)
                        : 0) |
                   (notify_unreachable > 0 ? unreachable : 0),
               (stalk_on_down > 0 ? down : 0) |
                   (stalk_on_unreachable > 0 ? unreachable : 0) |
                   (stalk_on_up > 0 ? up : 0),
               first_notification_delay,
               recovery_notification_delay,
               notification_period,
               notifications_enabled,
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
               obsess_over_host,
               timezone,
               retain_status_information > 0,
               retain_nonstatus_information > 0,
               false,
               icon_id},
      _id{host_id},
      _address{address},
      _process_performance_data{process_perfdata},
      _vrml_image{vrml_image},
      _statusmap_image{statusmap_image},
      _have_2d_coords{have_2d_coords > 0},
      _have_3d_coords{have_3d_coords > 0},
      _x_2d{x_2d},
      _y_2d{y_2d},
      _x_3d{x_3d},
      _y_3d{y_3d},
      _z_3d{z_3d},
      _should_be_drawn{should_be_drawn > 0},
      _should_reschedule_current_check{false},
      _last_time_down{0},
      _last_time_unreachable{0},
      _last_time_up{0},
      _last_state_history_update{0},
      _total_services{0},
      _total_service_check_interval{0},
      _circular_path_checked{false},
      _contains_circular_path{false},
      _initial_state{state_up},
      _last_state{_initial_state},
      _last_hard_state{_initial_state},
      _current_state{_initial_state} {
  // Make sure we have the data we need.
  if (name.empty() || address.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Host name or address is nullptr";
    config_logger->error("Error: Host name or address is nullptr");
    throw engine_error() << "Could not register host '" << name << "'";
  }
  if (host_id == 0) {
    engine_logger(log_config_error, basic)
        << "Error: Host must contain a host id "
           "because it comes from a database";
    config_logger->error(
        "Error: Host must contain a host id "
        "because it comes from a database");
    throw engine_error() << "Could not register host '" << name << "'";
  }

  // Check if the host already exists.
  uint64_t id{host_id};
  if (host_exists(id)) {
    engine_logger(log_config_error, basic)
        << "Error: Host '" << name << "' has already been defined";
    config_logger->error("Error: Host '{}' has already been defined", name);
    throw engine_error() << "Could not register host '" << name << "'";
  }

  // Duplicate string vars.
  _alias = !alias.empty() ? alias : name;

  set_current_attempt(1);
  set_modified_attributes(MODATTR_NONE);
  set_state_type(hard);

  set_flap_type((flap_detection_on_down > 0 ? down : 0) |
                (flap_detection_on_unreachable > 0 ? unreachable : 0) |
                (flap_detection_on_up > 0 ? up : 0));
}

host::~host() {
  std::shared_ptr<commands::command> cmd = get_check_command_ptr();
  if (cmd) {
    cmd->unregister_host_serv(name(), "");
  }
}

uint64_t host::host_id() const {
  return _id;
}

void host::set_host_id(uint64_t id) {
  _id = id;
}

/**
 * @brief change name of the host
 *
 * @param new_name
 */
void host::set_name(const std::string& new_name) {
  if (new_name == name()) {
    return;
  }
  std::shared_ptr<commands::command> cmd = get_check_command_ptr();
  if (cmd) {
    cmd->unregister_host_serv(name(), "");
  }
  notifier::set_name(new_name);
  if (cmd) {
    cmd->register_host_serv(name(), "");
  }
}

void host::add_child_host(host* child) {
  // Make sure we have the data we need.
  if (!child)
    throw engine_error() << "add child link called with nullptr ptr";

  child_hosts.insert({child->name(), child});

  // Notify event broker.
  broker_relation_data(NEBTYPE_PARENT_ADD, this, nullptr, child, nullptr);
}

void host::add_parent_host(const std::string& host_name) {
  // Make sure we have the data we need.
  if (host_name.empty()) {
    engine_logger(log_config_error, basic)
        << "add child link called with bad host_name";
    config_logger->error("add child link called with bad host_name");
    throw engine_error() << "add child link called with bad host_name";
  }

  // A host cannot be a parent/child of itself.
  if (name() == host_name) {
    engine_logger(log_config_error, basic)
        << "Error: Host '" << name() << "' cannot be a child/parent of itself";
    config_logger->error("Error: Host '{}' cannot be a child/parent of itself",
                         name());
    throw engine_error() << "host is child/parent itself";
  }

  parent_hosts.insert({host_name, nullptr});
}

const std::string& host::get_alias() const {
  return _alias;
}

void host::set_alias(const std::string& alias) {
  _alias = alias;
}

const std::string& host::get_address() const {
  return _address;
}

void host::set_address(const std::string& address) {
  _address = address;
}

bool host::get_process_performance_data() const {
  return _process_performance_data;
}

void host::set_process_performance_data(bool process_performance_data) {
  _process_performance_data = process_performance_data;
}

const std::string& host::get_vrml_image() const {
  return _vrml_image;
}

void host::set_vrml_image(const std::string& image) {
  _vrml_image = image;
}

const std::string& host::get_statusmap_image() const {
  return _statusmap_image;
}

void host::set_statusmap_image(const std::string& image) {
  _statusmap_image = image;
}

bool host::get_have_2d_coords() const {
  return _have_2d_coords;
}

void host::set_have_2d_coords(bool has_coords) {
  _have_2d_coords = has_coords;
}

bool host::get_have_3d_coords() const {
  return _have_3d_coords;
}

void host::set_have_3d_coords(bool has_coords) {
  _have_3d_coords = has_coords;
}

double host::get_x_2d() const {
  return _x_2d;
}

void host::set_x_2d(double x) {
  _x_2d = x;
}

double host::get_y_2d() const {
  return _y_2d;
}

void host::set_y_2d(double y) {
  _y_2d = y;
}

double host::get_x_3d() const {
  return _x_3d;
}

void host::set_x_3d(double x) {
  _x_3d = x;
}

double host::get_y_3d() const {
  return _y_3d;
}

void host::set_y_3d(double y) {
  _y_3d = y;
}

double host::get_z_3d() const {
  return _z_3d;
}

void host::set_z_3d(double z) {
  _z_3d = z;
}

int host::get_should_be_drawn() const {
  return _should_be_drawn;
}

void host::set_should_be_drawn(int should_be_drawn) {
  _should_be_drawn = should_be_drawn;
}

time_t host::get_last_time_down() const {
  return _last_time_down;
}

void host::set_last_time_down(time_t last_time) {
  _last_time_down = last_time;
}

time_t host::get_last_time_unreachable() const {
  return _last_time_unreachable;
}

void host::set_last_time_unreachable(time_t last_time) {
  _last_time_unreachable = last_time;
}

time_t host::get_last_time_up() const {
  return _last_time_up;
}

void host::set_last_time_up(time_t last_time) {
  _last_time_up = last_time;
}

bool host::get_should_reschedule_current_check() const {
  return _should_reschedule_current_check;
}

void host::set_should_reschedule_current_check(bool should_reschedule) {
  _should_reschedule_current_check = should_reschedule;
}

time_t host::get_last_state_history_update() const {
  return _last_state_history_update;
}

void host::set_last_state_history_update(time_t last_state_history_update) {
  _last_state_history_update = last_state_history_update;
}

int host::get_total_services() const {
  return _total_services;
}

void host::set_total_services(int total_services) {
  _total_services = total_services;
}

unsigned long host::get_total_service_check_interval() const {
  return _total_service_check_interval;
}

void host::set_total_service_check_interval(
    unsigned long total_service_check_interval) {
  _total_service_check_interval = total_service_check_interval;
}

int host::get_circular_path_checked() const {
  return _circular_path_checked;
}

void host::set_circular_path_checked(int check_level) {
  _circular_path_checked = check_level;
}

bool host::get_contains_circular_path() const {
  return _contains_circular_path;
}

void host::set_contains_circular_path(bool contains_circular_path) {
  _contains_circular_path = contains_circular_path;
}

enum host::host_state host::get_current_state() const {
  return _current_state;
}

void host::set_current_state(enum host::host_state current_state) {
  _current_state = current_state;
}

enum host::host_state host::get_last_state() const {
  return _last_state;
}

void host::set_last_state(enum host::host_state last_state) {
  _last_state = last_state;
}

enum host::host_state host::get_last_hard_state() const {
  return _last_hard_state;
}

void host::set_last_hard_state(enum host::host_state last_hard_state) {
  _last_hard_state = last_hard_state;
}

enum host::host_state host::get_initial_state() const {
  return _initial_state;
}

void host::set_initial_state(enum host::host_state current_state) {
  _initial_state = current_state;
}

bool host::recovered() const {
  return _current_state == host::state_up;
}

int host::get_current_state_int() const {
  return static_cast<int>(_current_state);
}

std::ostream& operator<<(std::ostream& os, host_map_unsafe const& obj) {
  bool first = true;
  for (const auto& [key, _] : obj) {
    if (!first) {
      os << ", ";
    }
    os << key;
    first = false;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, host_map const& obj) {
  bool first = true;
  for (const auto& [key, _] : obj) {
    if (!first) {
      os << ", ";
    }
    os << key;
    first = false;
  }
  return os;
}

/**
 *  Dump host content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The host to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const host& obj) {
  hostgroup* hg{obj.get_parent_groups().front()};

  std::string evt_str;
  if (obj.get_event_handler_ptr())
    evt_str = obj.get_event_handler_ptr()->get_name();
  std::string cmd_str;
  if (obj.get_check_command_ptr())
    cmd_str = obj.get_check_command_ptr()->get_name();
  std::string chk_period_str;
  if (obj.check_period_ptr)
    chk_period_str = obj.check_period_ptr->get_name();

  std::string notif_period_str;
  if (obj.get_notification_period_ptr())
    notif_period_str = obj.get_notification_period_ptr()->get_name();

  std::string cg_oss;
  std::string c_oss;
  std::string p_oss;
  std::string child_oss;

  if (obj.get_contactgroups().empty())
    cg_oss = "\"nullptr\"";
  else {
    std::ostringstream oss;
    oss << obj.get_contactgroups();
    cg_oss = oss.str();
  }
  if (obj.contacts().empty())
    c_oss = "\"nullptr\"";
  else {
    std::ostringstream oss;
    oss << obj.contacts();
    c_oss = oss.str();
  }
  if (obj.parent_hosts.empty())
    p_oss = "\"nullptr\"";
  else {
    std::ostringstream oss;
    oss << obj.parent_hosts;
    p_oss = oss.str();
  }

  if (obj.child_hosts.empty())
    child_oss = "\"nullptr\"";
  else {
    std::ostringstream oss;
    oss << obj.child_hosts;
    child_oss = oss.str();
  }

  std::string notifications;
  {
    std::ostringstream oss;
    for (int i = 0; i < 6; i++) {
      notification* s{obj.get_current_notifications()[i].get()};
      if (s)
        oss << "  notification_" << i << ": " << *s;
    }
    notifications = oss.str();
  }

  os << "host {\n"
        "  name:                                 "
     << obj.name()
     << "\n"
        "  display_name:                         "
     << obj.get_display_name()
     << "\n"
        "  alias:                                "
     << obj.get_alias()
     << "\n"
        "  address:                              "
     << obj.get_address()
     << "\n"
        "  parent_hosts:                         "
     << p_oss
     << "\n"
        "  child_hosts:                          "
     << child_oss
     << "\n"
        "  services:                             "
     << obj.services
     << "\n"
        "  host_check_command:                   "
     << obj.check_command()
     << "\n"
        "  initial_state:                        "
     << obj.get_initial_state()
     << "\n"
        "  check_interval:                       "
     << obj.check_interval()
     << "\n"
        "  retry_interval:                       "
     << obj.retry_interval()
     << "\n"
        "  max_attempts:                         "
     << obj.max_check_attempts()
     << "\n"
        "  event_handler:                        "
     << obj.event_handler()
     << "\n"
        "  contact_groups:                       "
     << cg_oss
     << "\n"
        "  contacts:                             "
     << c_oss
     << "\n"
        "  notification_interval:                "
     << obj.get_notification_interval()
     << "\n"
        "  first_notification_delay:             "
     << obj.get_first_notification_delay()
     << "\n"
        "  recovery_notification_delay:          "
     << obj.get_recovery_notification_delay()
     << "\n"
        "  notify_on_down:                       "
     << obj.get_notify_on(notifier::down)
     << "\n"
        "  notify_on_unreachable:                "
     << obj.get_notify_on(notifier::unreachable)
     << "\n"
        "  notify_on_recovery:                   "
     << obj.get_notify_on(notifier::up)
     << "\n"
        "  notify_on_flappingstart:              "
     << obj.get_notify_on(notifier::flappingstart)
     << "\n"
        "  notify_on_flappingstop:               "
     << obj.get_notify_on(notifier::flappingstop)
     << "\n"
        "  notify_on_flappingdisabled:           "
     << obj.get_notify_on(notifier::flappingdisabled)
     << "\n"
        "  notify_on_downtime:                   "
     << obj.get_notify_on(notifier::downtime)
     << "\n"
        "  notification_period:                  "
     << obj.notification_period() << "\n"
     << notifications
     << "  check_period:                         " << obj.check_period()
     << "\n"
        "  flap_detection_enabled:               "
     << obj.flap_detection_enabled()
     << "\n"
        "  low_flap_threshold:                   "
     << obj.get_low_flap_threshold()
     << "\n"
        "  high_flap_threshold:                  "
     << obj.get_high_flap_threshold()
     << "\n"
        "  flap_detection_on_up:                 "
     << obj.get_flap_detection_on(notifier::up)
     << "\n"
        "  flap_detection_on_down:               "
     << obj.get_flap_detection_on(notifier::down)
     << "\n"
        "  flap_detection_on_unreachable:        "
     << obj.get_flap_detection_on(notifier::unreachable)
     << "\n"
        "  stalk_on_up:                          "
     << obj.get_stalk_on(notifier::up)
     << "\n"
        "  stalk_on_down:                        "
     << obj.get_stalk_on(notifier::down)
     << "\n"
        "  stalk_on_unreachable:                 "
     << obj.get_stalk_on(notifier::unreachable)
     << "\n"
        "  check_freshness:                      "
     << obj.check_freshness_enabled()
     << "\n"
        "  freshness_threshold:                  "
     << obj.get_freshness_threshold()
     << "\n"
        "  process_performance_data:             "
     << obj.get_process_performance_data()
     << "\n"
        "  checks_enabled:                       "
     << obj.active_checks_enabled()
     << "\n"
        "  accept_passive_checks:                "
     << obj.passive_checks_enabled()
     << "\n"
        "  event_handler_enabled:                "
     << obj.event_handler_enabled()
     << "\n"
        "  retain_status_information:            "
     << obj.get_retain_status_information()
     << "\n"
        "  retain_nonstatus_information:         "
     << obj.get_retain_nonstatus_information()
     << "\n"
        "  obsess_over_host:                     "
     << obj.obsess_over()
     << "\n"
        "  notes:                                "
     << obj.get_notes()
     << "\n"
        "  notes_url:                            "
     << obj.get_notes_url()
     << "\n"
        "  action_url:                           "
     << obj.get_action_url()
     << "\n"
        "  icon_image:                           "
     << obj.get_icon_image()
     << "\n"
        "  icon_image_alt:                       "
     << obj.get_icon_image_alt()
     << "\n"
        "  vrml_image:                           "
     << obj.get_vrml_image()
     << "\n"
        "  statusmap_image:                      "
     << obj.get_statusmap_image()
     << "\n"
        "  have_2d_coords:                       "
     << obj.get_have_2d_coords()
     << "\n"
        "  x_2d:                                 "
     << obj.get_x_2d()
     << "\n"
        "  y_2d:                                 "
     << obj.get_y_2d()
     << "\n"
        "  have_3d_coords:                       "
     << obj.get_have_3d_coords()
     << "\n"
        "  x_3d:                                 "
     << obj.get_x_3d()
     << "\n"
        "  y_3d:                                 "
     << obj.get_y_3d()
     << "\n"
        "  z_3d:                                 "
     << obj.get_z_3d()
     << "\n"
        "  should_be_drawn:                      "
     << obj.get_should_be_drawn()
     << "\n"
        "  problem_has_been_acknowledged:        "
     << obj.problem_has_been_acknowledged()
     << "\n"
        "  acknowledgement_type:                 "
     << obj.get_acknowledgement()
     << "\n"
        "  check_type:                           "
     << obj.get_check_type()
     << "\n"
        "  current_state:                        "
     << obj.get_current_state()
     << "\n"
        "  last_state:                           "
     << obj.get_last_state()
     << "\n"
        "  last_hard_state:                      "
     << obj.get_last_hard_state()
     << "\n"
        "  plugin_output:                        "
     << obj.get_plugin_output()
     << "\n"
        "  long_plugin_output:                   "
     << obj.get_long_plugin_output()
     << "\n"
        "  perf_data:                            "
     << obj.get_perf_data()
     << "\n"
        "  state_type:                           "
     << obj.get_state_type()
     << "\n"
        "  current_attempt:                      "
     << obj.get_current_attempt()
     << "\n"
        "  current_event_id:                     "
     << obj.get_current_event_id()
     << "\n"
        "  last_event_id:                        "
     << obj.get_last_event_id()
     << "\n"
        "  current_problem_id:                   "
     << obj.get_current_problem_id()
     << "\n"
        "  last_problem_id:                      "
     << obj.get_last_problem_id()
     << "\n"
        "  latency:                              "
     << obj.get_latency()
     << "\n"
        "  execution_time:                       "
     << obj.get_execution_time()
     << "\n"
        "  is_executing:                         "
     << obj.get_is_executing()
     << "\n"
        "  check_options:                        "
     << obj.get_check_options()
     << "\n"
        "  notifications_enabled:                "
     << obj.get_notifications_enabled()
     << "\n"
        "  last_host_notification:               "
     << string::ctime(obj.get_last_notification())
     << "\n"
        "  next_host_notification:               "
     << string::ctime(obj.get_next_notification())
     << "\n"
        "  next_check:                           "
     << string::ctime(obj.get_next_check())
     << "\n"
        "  should_be_scheduled:                  "
     << obj.get_should_be_scheduled()
     << "\n"
        "  last_check:                           "
     << string::ctime(obj.get_last_check())
     << "\n"
        "  last_state_change:                    "
     << string::ctime(obj.get_last_state_change())
     << "\n"
        "  last_hard_state_change:               "
     << string::ctime(obj.get_last_hard_state_change())
     << "\n"
        "  last_time_up:                         "
     << string::ctime(obj.get_last_time_up())
     << "\n"
        "  last_time_down:                       "
     << string::ctime(obj.get_last_time_down())
     << "\n"
        "  last_time_unreachable:                "
     << string::ctime(obj.get_last_time_unreachable())
     << "\n"
        "  has_been_checked:                     "
     << obj.has_been_checked()
     << "\n"
        "  is_being_freshened:                   "
     << obj.get_is_being_freshened()
     << "\n"
        "  notified_on_down:                     "
     << obj.get_notified_on(notifier::down)
     << "\n"
        "  notified_on_unreachable:              "
     << obj.get_notified_on(notifier::unreachable)
     << "\n"
        "  no_more_notifications:                "
     << obj.get_no_more_notifications()
     << "\n"
        "  current_notification_id:              "
     << obj.get_current_notification_id()
     << "\n"
        "  scheduled_downtime_depth:             "
     << obj.get_scheduled_downtime_depth()
     << "\n"
        "  pending_flex_downtime:                "
     << obj.get_pending_flex_downtime() << "\n";

  os << "  state_history:                        ";
  for (size_t i{0}, end{obj.get_state_history().size()}; i < end; ++i)
    os << obj.get_state_history()[i] << (i + 1 < end ? ", " : "\n");

  os << "  state_history_index:                  "
     << obj.get_state_history_index()
     << "\n"
        "  last_state_history_update:            "
     << string::ctime(obj.get_last_state_history_update())
     << "\n"
        "  is_flapping:                          "
     << obj.get_is_flapping()
     << "\n"
        "  flapping_comment_id:                  "
     << obj.get_flapping_comment_id()
     << "\n"
        "  percent_state_change:                 "
     << obj.get_percent_state_change()
     << "\n"
        "  total_services:                       "
     << obj.get_total_services()
     << "\n"
        "  total_service_check_interval:         "
     << obj.get_total_service_check_interval()
     << "\n"
        "  modified_attributes:                  "
     << obj.get_modified_attributes()
     << "\n"
        "  circular_path_checked:                "
     << obj.get_circular_path_checked()
     << "\n"
        "  contains_circular_path:               "
     << obj.get_contains_circular_path()
     << "\n"
        "  event_handler_ptr:                    "
     << evt_str
     << "\n"
        "  check_command_ptr:                    "
     << cmd_str
     << "\n"
        "  check_period_ptr:                     "
     << chk_period_str
     << "\n"
        "  notification_period_ptr:              "
     << notif_period_str
     << "\n"
        "  hostgroups_ptr:                       "
     << (hg ? hg->get_group_name() : "") << "\n";

  for (auto const& cv : obj.custom_variables)
    os << cv.first << " ; ";

  os << "\n}\n";
  return os;
}

/**
 *  Determines whether or not a specific host is an immediate child of
 *  another host.
 *
 *  @param[in] parent_host Parent host.
 *  @param[in] child_host  Child host.
 *
 *  @return true or false.
 */
int is_host_immediate_child_of_host(com::centreon::engine::host* parent_host,
                                    com::centreon::engine::host* child_host) {
  // Not enough data.
  if (!child_host)
    return false;

  // Root/top-level hosts.
  if (!parent_host) {
    if (child_host->parent_hosts.empty())
      return true;
  }
  // Mid-level/bottom hosts.
  else {
    auto it{child_host->parent_hosts.find(parent_host->name())};
    return it != child_host->parent_hosts.end();
  }

  return false;
}

/**
 *  Determines whether or not a specific host is an immediate parent of
 *  another host.
 *
 *  @param[in] child_host  Child host.
 *  @param[in] parent_host Parent host.
 *
 *  @return true or false.
 */
int is_host_immediate_parent_of_host(com::centreon::engine::host* child_host,
                                     com::centreon::engine::host* parent_host) {
  if (is_host_immediate_child_of_host(parent_host, child_host))
    return true;
  return false;
}

/**
 *  Returns a count of the immediate children for a given host.
 *
 *  @deprecated This function is only used by the CGIS.
 *
 *  @param[in] hst Target host.
 *
 *  @return Number of immediate child hosts.
 */
int number_of_immediate_child_hosts(com::centreon::engine::host* hst) {
  int children(0);
  for (const auto& [_, sptr_host] : host::hosts)
    if (is_host_immediate_child_of_host(hst, sptr_host.get()))
      ++children;
  return children;
}

/**
 *  Get the number of immediate parent hosts for a given host.
 *
 *  @deprecated This function is only used by the CGIS.
 *
 *  @param[in] hst Target host.
 *
 *  @return Number of immediate parent hosts.
 */
int number_of_immediate_parent_hosts(com::centreon::engine::host* hst) {
  int parents(0);
  for (const auto& [_, sptr_host] : host::hosts)
    if (is_host_immediate_parent_of_host(hst, sptr_host.get()))
      ++parents;
  return parents;
}

/**
 *  Returns a count of the total children for a given host.
 *
 *  @deprecated This function is only used by the CGIS.
 *
 *  @param[in] hst Target host.
 *
 *  @return Number of total child hosts.
 */
int number_of_total_child_hosts(com::centreon::engine::host* hst) {
  int children(0);
  for (const auto& [_, sptr_host] : host::hosts)
    if (is_host_immediate_child_of_host(hst, sptr_host.get()))
      children += number_of_total_child_hosts(sptr_host.get()) + 1;
  return children;
}

/**
 *  Get the total number of parent hosts for a given host.
 *
 *  @deprecated This function is only used by the CGIS.
 *
 *  @param[in] hst Target host.
 *
 *  @return Number of total parent hosts.
 */
int number_of_total_parent_hosts(com::centreon::engine::host* hst) {
  int parents(0);
  for (host_map::iterator it{host::hosts.begin()}, end{host::hosts.end()};
       it != end; ++it)
    if (is_host_immediate_parent_of_host(hst, it->second.get()))
      parents += number_of_total_parent_hosts(it->second.get()) + 1;
  return parents;
}

/**
 *  Get host by id.
 *
 *  @param[in] host_id The host id.
 *
 *  @return The struct host or throw exception if the
 *          host is not found.
 */
host& engine::find_host(uint64_t host_id) {
  host_id_map::const_iterator it{host::hosts_by_id.find(host_id)};
  if (it == host::hosts_by_id.end())
    throw engine_error() << "Host '" << host_id << "' was not found";
  return *it->second;
}

/**
 *  Get if host exist.
 *
 *  @param[in] name The host name.
 *
 *  @return True if the host is found, otherwise false.
 */
bool engine::host_exists(uint64_t host_id) noexcept {
  host_id_map::const_iterator it(host::hosts_by_id.find(host_id));
  return it != host::hosts_by_id.end();
}

/**
 *  Get the id associated with a host.
 *
 *  @param[in] name  The name of the host.
 *
 *  @return  The host id or 0.
 */
uint64_t engine::get_host_id(const std::string& name) {
  host_map::const_iterator found{host::hosts.find(name)};
  return found != host::hosts.end() ? found->second->host_id() : 0u;
}

/**
 *  Get the name associated with a host (from its id).
 *
 *  @param[in] host_id  The id of the host.
 *
 *  @return  The host name or "" if it does not exist.
 */
std::string engine::get_host_name(const uint64_t host_id) {
  auto it = host::hosts_by_id.find(host_id);
  std::string retval;
  if (it != host::hosts_by_id.end())
    retval = it->second->name();

  return retval;
}

/**
 *  Schedule acknowledgement expiration.
 *
 */
void host::schedule_acknowledgement_expiration() {
  if (acknowledgement_timeout() > 0 && last_acknowledgement() != (time_t)0) {
    events::loop::instance().schedule(
        std::make_unique<timed_event>(
            timed_event::EVENT_EXPIRE_HOST_ACK,
            last_acknowledgement() + acknowledgement_timeout(), false, 0,
            nullptr, true, this, nullptr, 0),
        false);
  }
}

/**
 *  Log host event information.
 *  This function has been DEPRECATED.
 *
 *  @param[in] hst The host to log.
 *
 *  @return Return true on success.
 */
int host::log_event() {
  unsigned long log_options{NSLOG_HOST_UP};
  char const* state("UP");
  if (get_current_state() > 0 &&
      (unsigned int)get_current_state() < tab_host_states.size()) {
    log_options = tab_host_states[get_current_state()].first;
    state = tab_host_states[get_current_state()].second.c_str();
  }
  const std::string& state_type(tab_state_type[get_state_type()]);

  engine_logger(log_options, basic)
      << "HOST ALERT: " << name() << ";" << state << ";" << state_type << ";"
      << get_current_attempt() << ";" << get_plugin_output();
  SPDLOG_LOGGER_INFO(events_logger, "HOST ALERT: {};{};{};{};{}", name(), state,
                     state_type, get_current_attempt(), get_plugin_output());

  return OK;
}

/* process results of an asynchronous host check */
int host::handle_async_check_result_3x(
    const check_result& queued_check_result) {
  enum service::service_state svc_res{service::state_ok};
  enum host::host_state hst_res{host::state_up};
  int reschedule_check{false};
  std::string old_plugin_output;
  struct timeval start_time_hires;
  struct timeval end_time_hires;

  engine_logger(dbg_functions, basic) << "handle_async_host_check_result_3x()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "handle_async_host_check_result_3x() host {} res:{}",
                      name(), queued_check_result);

  /* get the current time */
  time_t current_time = std::time(nullptr);
  bool accept_passive_host_checks;
  uint32_t cached_host_check_horizon;
  accept_passive_host_checks = pb_config.accept_passive_host_checks();
  cached_host_check_horizon = pb_config.cached_host_check_horizon();

  double execution_time =
      static_cast<double>(queued_check_result.get_finish_time().tv_sec -
                          queued_check_result.get_start_time().tv_sec) +
      static_cast<double>(queued_check_result.get_finish_time().tv_usec -
                          queued_check_result.get_start_time().tv_usec) /
          1000000.0;
  if (execution_time < 0.0)
    execution_time = 0.0;

  engine_logger(dbg_checks, more)
      << "** Handling async check result for host '" << name() << "'...";
  SPDLOG_LOGGER_DEBUG(
      checks_logger, "** Handling async check result for host '{}'...", name());

  engine_logger(dbg_checks, most)
      << "\tCheck Type:         "
      << (queued_check_result.get_check_type() == check_active ? "Active"
                                                               : "Passive")
      << "\n"
      << "\tCheck Options:      " << queued_check_result.get_check_options()
      << "\n"
      << "\tReschedule Check?:  "
      << (queued_check_result.get_reschedule_check() ? "Yes" : "No") << "\n"
      << "\tShould Reschedule Current Host Check?:"
      << get_should_reschedule_current_check() << "\tExited OK?:         "
      << (queued_check_result.get_exited_ok() ? "Yes" : "No") << "\n"
      << com::centreon::logging::setprecision(3)
      << "\tExec Time:          " << execution_time << "\n"
      << "\tLatency:            " << queued_check_result.get_latency() << "\n"
      << "\treturn Status:      " << queued_check_result.get_return_code()
      << "\n"
      << "\tOutput:             " << queued_check_result.get_output();

  SPDLOG_LOGGER_DEBUG(checks_logger, "Check Type: {}",
                      queued_check_result.get_check_type() == check_active
                          ? "Active"
                          : "Passive");
  SPDLOG_LOGGER_DEBUG(checks_logger, "Check Options: {}",
                      queued_check_result.get_check_options());
  SPDLOG_LOGGER_DEBUG(
      checks_logger, "Reschedule Check?:  {}",
      queued_check_result.get_reschedule_check() ? "Yes" : "No");
  SPDLOG_LOGGER_DEBUG(
      checks_logger, "Should Reschedule Current Host Check?: {}",
      queued_check_result.get_reschedule_check() ? "Yes" : "No");
  SPDLOG_LOGGER_DEBUG(checks_logger, "Exited OK?:         {}",
                      queued_check_result.get_exited_ok() ? "Yes" : "No");
  SPDLOG_LOGGER_DEBUG(checks_logger, "Exec Time:          {:.3f}",
                      execution_time);
  SPDLOG_LOGGER_DEBUG(checks_logger, "Latency:            {}",
                      queued_check_result.get_latency());
  SPDLOG_LOGGER_DEBUG(checks_logger, "return Status:      {}",
                      queued_check_result.get_return_code());
  SPDLOG_LOGGER_DEBUG(checks_logger, "Output:             {}",
                      queued_check_result.get_output());
  /* decrement the number of host checks still out there... */
  if (queued_check_result.get_check_type() == check_active &&
      currently_running_host_checks > 0)
    currently_running_host_checks--;

  /*
   * skip this host check results if its passive and we aren't accepting passive
   * check results */
  if (queued_check_result.get_check_type() == check_passive) {
    if (!accept_passive_host_checks) {
      engine_logger(dbg_checks, basic)
          << "Discarding passive host check result because passive host "
             "checks are disabled globally.";
      SPDLOG_LOGGER_TRACE(
          checks_logger,
          "Discarding passive host check result because passive host "
          "checks are disabled globally.");

      return ERROR;
    }
    if (!passive_checks_enabled()) {
      engine_logger(dbg_checks, basic)
          << "Discarding passive host check result because passive checks "
             "are disabled for this host.";
      SPDLOG_LOGGER_TRACE(
          checks_logger,
          "Discarding passive host check result because passive checks "
          "are disabled for this host.");
      return ERROR;
    }
  }

  /*
   * clear the freshening flag (it would have been set if this host was
   * determined to be stale) */
  if (queued_check_result.get_check_options() & CHECK_OPTION_FRESHNESS_CHECK)
    set_is_being_freshened(false);

  /* DISCARD INVALID FRESHNESS CHECK RESULTS */
  /* If a host goes stale, Engine will initiate a forced check in order
  ** to freshen it. There is a race condition whereby a passive check
  ** could arrive between the 1) initiation of the forced check and 2)
  ** the time when the forced check result is processed here. This would
  ** make the host fresh again, so we do a quick check to make sure the
  ** host is still stale before we accept the check result.
  */
  if ((queued_check_result.get_check_options() &
       CHECK_OPTION_FRESHNESS_CHECK) &&
      is_result_fresh(current_time, false)) {
    engine_logger(dbg_checks, basic)
        << "Discarding host freshness check result because the host is "
           "currently fresh (race condition avoided).";
    SPDLOG_LOGGER_TRACE(
        checks_logger,
        "Discarding host freshness check result because the host is "
        "currently fresh (race condition avoided).");
    return OK;
  }

  /* initialize the last host state change times if necessary */
  if (get_last_state_change() == (time_t)0)
    set_last_state_change(get_last_check());
  if (get_last_hard_state_change() == (time_t)0)
    set_last_hard_state_change(get_last_check());

  /* was this check passive or active? */
  set_check_type((queued_check_result.get_check_type() == check_active)
                     ? check_active
                     : check_passive);

  /* update check statistics for passive results */
  if (queued_check_result.get_check_type() == check_passive)
    update_check_stats(PASSIVE_HOST_CHECK_STATS,
                       queued_check_result.get_start_time().tv_sec);

  /* should we reschedule the next check of the host? NOTE: this might be
   * overridden later... */
  reschedule_check = queued_check_result.get_reschedule_check();

  // Inherit the should reschedule flag from the host. It is used when
  // rescheduled checks were discarded because only one check can be executed
  // on the same host at the same time. The flag is then set in the host
  // and this check should be rescheduled regardless of what it was meant
  // to initially.
  if (get_should_reschedule_current_check() &&
      !queued_check_result.get_reschedule_check())
    reschedule_check = true;

  // Clear the should reschedule flag.
  set_should_reschedule_current_check(false);

  /* check latency is passed to us for both active and passive checks */
  set_latency(queued_check_result.get_latency());

  /* update the execution time for this check (millisecond resolution) */
  set_execution_time(execution_time);

  /* set the checked flag */
  set_has_been_checked(true);

  /* clear the execution flag if this was an active check */
  if (queued_check_result.get_check_type() == check_active)
    set_is_executing(false);

  /* get the last check time */
  set_last_check(queued_check_result.get_start_time().tv_sec);

  /* was this check passive or active? */
  set_check_type((queued_check_result.get_check_type() == check_active)
                     ? check_active
                     : check_passive);

  /* save the old host state */
  set_last_state(get_current_state());
  if (get_state_type() == hard)
    set_last_hard_state(get_current_state());

  /* save old plugin output */
  if (!get_plugin_output().empty())
    old_plugin_output = get_plugin_output();

  /* parse check output to get: (1) short output, (2) long output, (3) perf data
   */

  std::string output{queued_check_result.get_output()};
  std::string plugin_output;
  std::string long_plugin_output;
  std::string perf_data;
  parse_check_output(output, plugin_output, long_plugin_output, perf_data, true,
                     false);
  set_plugin_output(plugin_output);
  set_long_plugin_output(long_plugin_output);
  set_perf_data(perf_data);

  /* make sure we have some data */
  if (get_plugin_output().empty()) {
    set_plugin_output("(No output returned from host check)");
  }

  /* replace semicolons in plugin output (but not performance data) with colons
   */
  std::string temp_str(get_plugin_output());
  std::replace(temp_str.begin(), temp_str.end(), ';', ':');
  set_plugin_output(temp_str);

  engine_logger(dbg_checks, most)
      << "Parsing check output...\n"
      << "Short Output:\n"
      << (get_plugin_output().empty() ? "nullptr" : get_plugin_output()) << "\n"
      << "Long Output:\n"
      << (get_long_plugin_output().empty() ? "nullptr"
                                           : get_long_plugin_output())
      << "\n"
      << "Perf Data:\n"
      << (get_perf_data().empty() ? "nullptr" : get_perf_data());
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "Parsing check output... Short Output: {}  Long Output: {} "
      "Perf Data: {}",
      get_plugin_output().empty() ? "nullptr" : get_plugin_output(),
      get_long_plugin_output().empty() ? "nullptr" : get_long_plugin_output(),
      get_perf_data().empty() ? "nullptr" : get_perf_data());
  /* get the unprocessed return code */
  /* NOTE: for passive checks, this is the final/processed state */
  svc_res = static_cast<enum service::service_state>(
      queued_check_result.get_return_code());
  hst_res =
      static_cast<enum host::host_state>(queued_check_result.get_return_code());

  /* adjust return code (active checks only) */
  if (queued_check_result.get_check_type() == check_active) {
    /* if there was some error running the command, just skip it (this shouldn't
     * be happening) */
    if (!queued_check_result.get_exited_ok()) {
      engine_logger(log_runtime_warning, basic)
          << "Warning:  Check of host '" << name()
          << "' did not exit properly!";
      SPDLOG_LOGGER_WARN(runtime_logger,
                         "Warning:  Check of host '{}' did not exit properly!",
                         name());

      set_plugin_output("(Host check did not exit properly)");
      set_long_plugin_output("");
      set_perf_data("");

      svc_res = service::state_unknown;
    }

    /* make sure the return code is within bounds */
    else if (queued_check_result.get_return_code() < 0 ||
             queued_check_result.get_return_code() > 3) {
      engine_logger(log_runtime_warning, basic)
          << "Warning: return (code of "
          << queued_check_result.get_return_code() << " for check of host '"
          << name() << "' was out of bounds."
          << ((queued_check_result.get_return_code() == 126 ||
               queued_check_result.get_return_code() == 127)
                  ? " Make sure the plugin you're trying to run actually "
                    "exists."
                  : "");
      SPDLOG_LOGGER_WARN(
          runtime_logger,
          "Warning: return (code of {} for check of host '{}' was out of "
          "bounds.",
          queued_check_result.get_return_code(), name(),
          (queued_check_result.get_return_code() == 126 ||
           queued_check_result.get_return_code() == 127)
              ? " Make sure the plugin you're trying to run actually exists."
              : "");

      std::ostringstream oss;
      oss << "(Return code of " << queued_check_result.get_return_code()
          << " is out of bounds"
          << ((queued_check_result.get_return_code() == 126 ||
               queued_check_result.get_return_code() == 127)
                  ? " - plugin may be missing"
                  : "")
          << ")";

      set_plugin_output(oss.str());
      set_long_plugin_output("");
      set_perf_data("");

      svc_res = service::state_unknown;
    }

    /* a nullptr host check command means we should assume the host is UP */
    if (check_command().empty()) {
      set_plugin_output("(Host assumed to be UP)");
      svc_res = service::state_ok;
    }
  }

  /* translate return code to basic UP/DOWN state - the DOWN/UNREACHABLE state
   * determination is made later */
  /* NOTE: only do this for active checks - passive check results already have
   * the final state */
  if (queued_check_result.get_check_type() == check_active) {
    /* Let WARNING states indicate the host is up
     * (fake the result to be state_ok) */
    if (svc_res == service::state_warning)
      svc_res = service::state_ok;

    /* OK states means the host is UP */
    if (svc_res == service::state_ok)
      hst_res = host::state_up;

    /* any problem state indicates the host is not UP */
    else
      hst_res = host::state_down;
  }

  /******************* PROCESS THE CHECK RESULTS ******************/

  /* process the host check result */
  process_check_result_3x(hst_res, old_plugin_output, CHECK_OPTION_NONE,
                          reschedule_check, true, cached_host_check_horizon);

  engine_logger(dbg_checks, more)
      << "** Async check result for host '" << name()
      << "' handled: new state=" << get_current_state();
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "** Async check result for host '{}' handled: new state={}", name(),
      static_cast<uint32_t>(get_current_state()));

  /* high resolution start time for event broker */
  start_time_hires = queued_check_result.get_start_time();

  /* high resolution end time for event broker */
  gettimeofday(&end_time_hires, nullptr);

  /* send data to event broker */
  broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED, this, get_check_type(),
                    nullptr);
  return OK;
}

/* run a scheduled host check asynchronously */
int host::run_scheduled_check(int check_options, double latency) {
  int result = OK;
  time_t current_time = 0L;
  time_t preferred_time = 0L;
  time_t next_valid_time = 0L;
  bool time_is_valid = true;

  uint32_t interval_length;
  interval_length = pb_config.interval_length();

  engine_logger(dbg_functions, basic) << "run_scheduled_host_check_3x()";
  SPDLOG_LOGGER_TRACE(functions_logger, "run_scheduled_host_check_3x()");

  engine_logger(dbg_checks, basic)
      << "Attempting to run scheduled check of host '" << name()
      << "': check options=" << check_options << ", latency=" << latency;
  SPDLOG_LOGGER_TRACE(
      checks_logger,
      "Attempting to run scheduled check of host '{}': check options={}, "
      "latency={}",
      name(), check_options, latency);

  /* attempt to run the check */
  result = run_async_check(check_options, latency, true, true, &time_is_valid,
                           &preferred_time);

  /* an error occurred, so reschedule the check */
  if (result == ERROR) {
    engine_logger(dbg_checks, more)
        << "Unable to run scheduled host check at this time";
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Unable to run scheduled host check at this time");

    /* only attempt to (re)schedule checks that should get checked... */
    if (get_should_be_scheduled()) {
      /* get current time */
      time(&current_time);

      /* determine next time we should check the host if needed */
      /* if host has no check interval, schedule it again for 5 minutes from now
       */
      if (current_time >= preferred_time)
        preferred_time =
            current_time +
            static_cast<time_t>(check_interval() <= 0
                                    ? 300
                                    : check_interval() * interval_length);

      // Make sure we rescheduled the next host check at a valid time.
      {
        timezone_locker lock(get_timezone());
        get_next_valid_time(preferred_time, &next_valid_time,
                            this->check_period_ptr);
      }

      /* the host could not be rescheduled properly - set the next check time
       * for next week */
      if (!time_is_valid && next_valid_time == preferred_time) {
        set_next_check((time_t)(next_valid_time + 60 * 60 * 24 * 7));

        engine_logger(log_runtime_warning, basic)
            << "Warning: Check of host '" << name()
            << "' could not be "
               "rescheduled properly.  Scheduling check for next week... "
            << " next_check  " << get_next_check();
        SPDLOG_LOGGER_WARN(
            runtime_logger,
            "Warning: Check of host '{}' could not be rescheduled properly.  "
            "Scheduling check for next week... next_check  {}",
            name(), get_next_check());

        engine_logger(dbg_checks, more)
            << "Unable to find any valid times to reschedule the next"
               " host check!";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "Unable to find any valid times to reschedule the next host "
            "check!");
      }
      /* this service could be rescheduled... */
      else {
        set_next_check(next_valid_time);
        set_should_be_scheduled(true);

        engine_logger(dbg_checks, more)
            << "Rescheduled next host check for " << my_ctime(&next_valid_time);
        SPDLOG_LOGGER_DEBUG(checks_logger, "Rescheduled next host check for {}",
                            my_ctime(&next_valid_time));
      }
    }

    /* update the status log */
    update_status();

    /* reschedule the next host check - unless we couldn't find a valid next
     * check time */
    /* 10/19/07 EG - keep original check options */
    if (get_should_be_scheduled())
      schedule_check(get_next_check(), check_options);

    return ERROR;
  }
  return OK;
}

/* perform an asynchronous check of a host */
/* scheduled host checks will use this, as will some checks that result from
 * on-demand checks... */
int host::run_async_check(int check_options,
                          double latency,
                          bool scheduled_check,
                          bool reschedule_check,
                          bool* time_is_valid,
                          time_t* preferred_time) noexcept {
  engine_logger(dbg_functions, basic)
      << "host::run_async_check, check_options=" << check_options
      << ", latency=" << latency << ", scheduled_check=" << scheduled_check
      << ", reschedule_check=" << reschedule_check;
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "host::run_async_check, check_options={}, latency={}, "
                      "scheduled_check={}, reschedule_check={}",
                      check_options, latency, scheduled_check,
                      reschedule_check);

  // Preamble.
  if (!get_check_command_ptr()) {
    engine_logger(log_runtime_error, basic)
        << "Error: Attempt to run active check on host '" << name()
        << "' with no check command";
    runtime_logger->error(
        "Error: Attempt to run active check on host '{}' with no check command",
        name());
    return ERROR;
  }

  engine_logger(dbg_checks, basic)
      << "** Running async check of host '" << name() << "'...";
  SPDLOG_LOGGER_TRACE(checks_logger, "** Running async check of host '{}'...",
                      name());

  // Check if the host is viable now.
  if (!verify_check_viability(check_options, time_is_valid, preferred_time))
    return ERROR;

  int32_t host_check_timeout;
  host_check_timeout = pb_config.host_check_timeout();

  // If this check is a rescheduled check, propagate the rescheduled check
  // flag to the host. This solves the problem when a new host check is bound
  // to be rescheduled but would be discarded because a host check is already
  // running.
  if (reschedule_check)
    set_should_reschedule_current_check(true);

  // Don't execute a new host check if one is already running.
  if (get_is_executing() && !(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
    engine_logger(dbg_checks, basic)
        << "A check of this host (" << name()
        << ") is already being executed, so we'll pass for the moment...";
    SPDLOG_LOGGER_TRACE(
        checks_logger,
        "A check of this host ({}) is already being executed, so we'll pass "
        "for the moment...",
        name());
    return OK;
  }

  // Send broker event.
  timeval start_time{0, 0};
  int res = broker_host_check(NEBTYPE_HOSTCHECK_ASYNC_PRECHECK, this,
                              checkable::check_active, nullptr);

  // Host check was cancel by NEB module. Reschedule check later.
  if (NEBERROR_CALLBACKCANCEL == res) {
    engine_logger(log_runtime_error, basic)
        << "Error: Some broker module cancelled check of host '" << name()
        << "'";
    runtime_logger->error(
        "Error: Some broker module cancelled check of host '{}'", name());
    return ERROR;
  }
  // Host check was overriden by NEB module.
  else if (NEBERROR_CALLBACKOVERRIDE == res) {
    engine_logger(dbg_functions, basic)
        << "Some broker module overrode check of host '" << name()
        << "' so we'll bail out";
    SPDLOG_LOGGER_TRACE(
        functions_logger,
        "Some broker module overrode check of host '{}' so we'll bail out",
        name());
    return OK;
  }

  // Checking starts.
  engine_logger(dbg_functions, basic) << "Checking host '" << name() << "'...";
  SPDLOG_LOGGER_TRACE(functions_logger, "Checking host '{}'...", name());

  // Clear check options.
  if (scheduled_check)
    set_check_options(CHECK_OPTION_NONE);

  // Adjust check attempts.
  adjust_check_attempt(true);

  // Update latency for event broker and macros.
  double old_latency(get_latency());
  set_latency(latency);

  // Get current host macros.
  nagios_macros* macros(get_global_macros());

  std::string processed_cmd = get_check_command_line(macros);

  // Time to start command.
  gettimeofday(&start_time, nullptr);

  // Set check time for on-demand checks, so they're
  // not incorrectly detected as being orphaned.
  if (!scheduled_check)
    set_next_check(start_time.tv_sec);

  // Update the number of running host checks.
  ++currently_running_host_checks;

  // Set the execution flag.
  set_is_executing(true);

  // Send event broker.
  broker_host_check(NEBTYPE_HOSTCHECK_INITIATE, this, checkable::check_active,
                    processed_cmd.c_str());

  // Restore latency.
  set_latency(old_latency);

  // Update statistics.
  update_check_stats(scheduled_check ? ACTIVE_SCHEDULED_HOST_CHECK_STATS
                                     : ACTIVE_ONDEMAND_HOST_CHECK_STATS,
                     start_time.tv_sec);
  update_check_stats(PARALLEL_HOST_CHECK_STATS, start_time.tv_sec);

  // Init check result info.
  check_result::pointer check_result_info = std::make_shared<check_result>(
      host_check, this, checkable::check_active, check_options,
      reschedule_check, latency, start_time, start_time, false, true,
      service::state_ok, "");

  auto run_failure = [&](const std::string& reason) {
    // Update check result.
    timeval tv;
    gettimeofday(&tv, nullptr);
    check_result_info->set_finish_time(tv);
    check_result_info->set_early_timeout(false);
    check_result_info->set_return_code(service::state_unknown);
    check_result_info->set_exited_ok(true);
    check_result_info->set_output(reason);

    // Queue check result.
    checks::checker::instance().add_check_result_to_reap(check_result_info);
  };

  // allowed by whitelist?
  if (!command_is_allowed_by_whitelist(processed_cmd, CHECK_TYPE)) {
    SPDLOG_LOGGER_ERROR(commands_logger,
                        "host {}: this command cannot be executed because of "
                        "security restrictions on the poller. A whitelist has "
                        "been defined, and it does not include this command.",
                        name());
    SPDLOG_LOGGER_DEBUG(commands_logger,
                        "host {}: command not allowed by whitelist {}", name(),
                        processed_cmd);
    run_failure(configuration::command_blacklist_output);
  } else {
    // Run command.
    bool retry;
    do {
      retry = false;
      try {
        // Run command.
        get_check_command_ptr()->run(processed_cmd, *macros, host_check_timeout,
                                     check_result_info);
      } catch (std::exception const& e) {
        // Update check result.
        run_failure("(Execute command failed)");

        engine_logger(log_runtime_warning, basic)
            << "Error: Host check command execution failed: " << e.what();
        SPDLOG_LOGGER_WARN(runtime_logger,
                           "Error: Host check command execution failed: {}",
                           e.what());
      }
    } while (retry);
  }
  // Cleanup.
  clear_volatile_macros_r(macros);
  return OK;
}

/**
 * @brief Schedules an immediate or delayed host check
 *
 * @param check_time The check time.
 * @param options A bit field with several options.
 *
 * @return a boolean telling if yes or not the host status is sent to broker.
 */
bool host::schedule_check(time_t check_time,
                          uint32_t options,
                          bool no_update_status_now) {
  engine_logger(dbg_functions, basic) << "schedule_host_check()";
  SPDLOG_LOGGER_TRACE(functions_logger, "schedule_host_check()");

  engine_logger(dbg_checks, basic)
      << "Scheduling a "
      << (options & CHECK_OPTION_FORCE_EXECUTION ? "forced" : "non-forced")
      << ", active check of host '" << name() << "' @ "
      << my_ctime(&check_time);
  SPDLOG_LOGGER_TRACE(
      checks_logger, "Scheduling a {}, active check of host '{}' @ {}",
      options & CHECK_OPTION_FORCE_EXECUTION ? "forced" : "non-forced", name(),
      my_ctime(&check_time));

  /* don't schedule a check if active checks of this host are disabled */
  if (!active_checks_enabled() && !(options & CHECK_OPTION_FORCE_EXECUTION)) {
    engine_logger(dbg_checks, basic)
        << "Active checks are disabled for this host.";
    SPDLOG_LOGGER_TRACE(checks_logger,
                        "Active checks are disabled for this host.");
    return false;
  }

  /* default is to use the new event */
  int use_original_event = false;

#ifdef PERFORMANCE_INCREASE_BUT_VERY_BAD_IDEA_INDEED
/* WARNING! 1/19/07 on-demand async host checks will end up causing mutliple
 * scheduled checks of a host to appear in the queue if the code below is
 * skipped */
/* if(use_large_installation_tweaks==false)... skip code below */
#endif

  /* see if there are any other scheduled checks of this host in the queue */
  timed_event_list::iterator found = events::loop::instance().find_event(
      events::loop::low, timed_event::EVENT_HOST_CHECK, this);

  /* we found another host check event for this host in the queue - what should
   * we do? */
  if (found != events::loop::instance().list_end(events::loop::low)) {
    auto& temp_event = *found;
    engine_logger(dbg_checks, most)
        << "Found another host check event for this host @ "
        << my_ctime(&temp_event->run_time);
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Found another host check event for this host @ {}",
                        my_ctime(&temp_event->run_time));
    /* use the originally scheduled check unless we decide otherwise */
    use_original_event = true;

    /* the original event is a forced check... */
    if ((temp_event->event_options & CHECK_OPTION_FORCE_EXECUTION)) {
      /* the new event is also forced and its execution time is earlier than the
       * original, so use it instead */
      if ((options & CHECK_OPTION_FORCE_EXECUTION) &&
          (check_time < temp_event->run_time)) {
        engine_logger(dbg_checks, most)
            << "New host check event is forced and occurs before the "
               "existing event, so the new event be used instead.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New host check event is forced and occurs before the "
            "existing event, so the new event be used instead.");
        use_original_event = false;
      }
    }

    /* the original event is not a forced check... */
    else {
      /* the new event is a forced check, so use it instead */
      if ((options & CHECK_OPTION_FORCE_EXECUTION)) {
        use_original_event = false;
        engine_logger(dbg_checks, most)
            << "New host check event is forced, so it will be used "
               "instead of the existing event.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New host check event is forced, so it will be used "
            "instead of the existing event.");
      }

      /* the new event is not forced either and its execution time is earlier
         than the original, so use it instead */
      else if (check_time < temp_event->run_time) {
        use_original_event = false;
        engine_logger(dbg_checks, most)
            << "New host check event occurs before the existing (older) "
               "event, so it will be used instead.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New host check event occurs before the existing (older) "
            "event, so it will be used instead.");
      }

      /* the new event is older, so override the existing one */
      else {
        engine_logger(dbg_checks, most)
            << "New host check event occurs after the existing event, "
               "so we'll ignore it.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New host check event occurs after the existing event, "
            "so we'll ignore it.");
      }
    }

    if (!use_original_event)
      events::loop::instance().remove_event(found, events::loop::low);
    else {
      /* reset the next check time (it may be out of sync) */
      set_next_check(temp_event->run_time);

      engine_logger(dbg_checks, most)
          << "Keeping original host check event (ignoring the new one).";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Keeping original host check event at {:%Y-%m-%dT%H:%M:%S} (ignoring "
          "the new one at {:%Y-%m-%dT%H:%M:%S}).",
          fmt::localtime(get_next_check()), fmt::localtime(check_time));
    }
  }

  /* save check options for retention purposes */
  set_check_options(options);

  /* use the new event */
  if (!use_original_event) {
    engine_logger(dbg_checks, most) << "Scheduling new host check event.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Scheduling new host check event.");

    /* set the next host check time */
    set_next_check(check_time);

    /* place the new event in the event queue */
    auto new_event{std::make_unique<timed_event>(
        timed_event::EVENT_HOST_CHECK, get_next_check(), false, 0L, nullptr,
        true, (void*)this, nullptr, options)};

    events::loop::instance().reschedule_event(std::move(new_event),
                                              events::loop::low);
  }

  /* update the status log */
  if (!no_update_status_now) {
    update_status();
    return true;
  } else
    return false;
}

/* detects host flapping */
void host::check_for_flapping(bool update,
                              bool actual_check,
                              bool allow_flapstart_notification) {
  bool update_history;
  bool is_flapping = false;
  unsigned int x = 0;
  unsigned int y = 0;
  int last_state_history_value = host::state_up;
  unsigned long wait_threshold = 0L;
  double curved_changes = 0.0;
  double curved_percent_change = 0.0;
  time_t current_time = 0L;
  double low_threshold = 0.0;
  double high_threshold = 0.0;
  double low_curve_value = 0.75;
  double high_curve_value = 1.25;

  uint32_t interval_length;
  float low_host_flap_threshold;
  float high_host_flap_threshold;
  bool enable_flap_detection;

  interval_length = pb_config.interval_length();
  low_host_flap_threshold = pb_config.low_host_flap_threshold();
  high_host_flap_threshold = pb_config.high_host_flap_threshold();
  enable_flap_detection = pb_config.enable_flap_detection();

  engine_logger(dbg_functions, basic) << "host::check_for_flapping()";
  SPDLOG_LOGGER_TRACE(functions_logger, "host::check_for_flapping()");

  engine_logger(dbg_flapping, more)
      << "Checking host '" << name() << "' for flapping...";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Checking host '{}' for flapping...",
                      name());

  time(&current_time);

  /* period to wait for updating archived state info if we have no state change
   */
  if (get_total_services() == 0)
    wait_threshold = static_cast<unsigned long>(get_notification_interval() *
                                                interval_length);
  else
    wait_threshold =
        static_cast<unsigned long>(get_total_service_check_interval() *
                                   interval_length / get_total_services());

  update_history = update;

  /* should we update state history for this state? */
  if (update_history) {
    if ((get_current_state() == host::state_up && !get_flap_detection_on(up)) ||
        (get_current_state() == host::state_down &&
         !get_flap_detection_on(down)) ||
        (get_current_state() == host::state_unreachable))
      update_history = false;
  }

  /* if we didn't have an actual check, only update if we've waited long enough
   */
  if (update_history && !actual_check &&
      static_cast<unsigned long>(
          current_time - get_last_state_history_update()) < wait_threshold) {
    update_history = false;
  }

  /* what thresholds should we use (global or host-specific)? */
  low_threshold = (get_low_flap_threshold() <= 0.0) ? low_host_flap_threshold
                                                    : get_low_flap_threshold();
  high_threshold = (get_high_flap_threshold() <= 0.0)
                       ? high_host_flap_threshold
                       : get_high_flap_threshold();

  /* record current host state */
  if (update_history) {
    /* update the last record time */
    set_last_state_history_update(current_time);

    /* record the current state in the state history */
    get_state_history()[get_state_history_index()] = get_current_state();

    /* increment state history index to next available slot */
    set_state_history_index(get_state_history_index() + 1);
    if (get_state_history_index() >= MAX_STATE_HISTORY_ENTRIES)
      set_state_history_index(0);
  }

  /* calculate overall changes in state */
  for (x = 0, y = get_state_history_index(); x < MAX_STATE_HISTORY_ENTRIES;
       x++) {
    if (x == 0) {
      last_state_history_value = get_state_history()[y];
      y++;
      if (y >= MAX_STATE_HISTORY_ENTRIES)
        y = 0;
      continue;
    }

    if (last_state_history_value != get_state_history()[y])
      curved_changes +=
          (((double)(x - 1) * (high_curve_value - low_curve_value)) /
           ((double)(MAX_STATE_HISTORY_ENTRIES - 2))) +
          low_curve_value;

    last_state_history_value = get_state_history()[y];

    y++;
    if (y >= MAX_STATE_HISTORY_ENTRIES)
      y = 0;
  }

  /* calculate overall percent change in state */
  curved_percent_change = (double)(((double)curved_changes * 100.0) /
                                   (double)(MAX_STATE_HISTORY_ENTRIES - 1));

  set_percent_state_change(curved_percent_change);

  engine_logger(dbg_flapping, most)
      << com::centreon::logging::setprecision(2) << "LFT=" << low_threshold
      << ", HFT=" << high_threshold << ", CPC=" << curved_percent_change
      << ", PSC=" << curved_percent_change << "%";
  SPDLOG_LOGGER_DEBUG(checks_logger, "LFT={:.2f}, HFT={}, CPC={}, PSC={}%",
                      low_threshold, high_threshold, curved_percent_change,
                      curved_percent_change);

  /* don't do anything if we don't have flap detection enabled on a program-wide
   * basis */
  if (!enable_flap_detection)
    return;

  /* don't do anything if we don't have flap detection enabled for this host */
  if (!flap_detection_enabled())
    return;

  /* are we flapping, undecided, or what?... */

  /* we're undecided, so don't change the current flap state */
  if (curved_percent_change > low_threshold &&
      curved_percent_change < high_threshold)
    return;
  /* we're below the lower bound, so we're not flapping */
  else if (curved_percent_change <= low_threshold)
    is_flapping = false;
  /* else we're above the upper bound, so we are flapping */
  else if (curved_percent_change >= high_threshold) {
    /* start flapping on !OK states which makes more sense */
    if ((get_current_state() != host::state_up) || get_is_flapping())
      is_flapping = true;
  }
  engine_logger(dbg_flapping, more)
      << "Host " << (is_flapping ? "is" : "is not") << " flapping ("
      << curved_percent_change << "% state change).";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Host {} flapping ({}% state change).",
                      is_flapping ? "is" : "is not", curved_percent_change);

  /* did the host just start flapping? */
  if (is_flapping && !get_is_flapping())
    set_flap(curved_percent_change, high_threshold, low_threshold,
             allow_flapstart_notification);

  /* did the host just stop flapping? */
  else if (!is_flapping && get_is_flapping())
    clear_flap(curved_percent_change, high_threshold, low_threshold);
}

void host::set_flap(double percent_change,
                    double high_threshold,
                    double low_threshold [[maybe_unused]],
                    bool allow_flapstart_notification) {
  engine_logger(dbg_functions, basic) << "set_host_flap()";
  SPDLOG_LOGGER_TRACE(functions_logger, "set_host_flap()");

  engine_logger(dbg_flapping, more)
      << "Host '" << name() << "' started flapping!";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Host '{}' started flapping!", name());

  /* log a notice - this one is parsed by the history CGI */
  engine_logger(log_runtime_warning, basic)
      << com::centreon::logging::setprecision(1)
      << "HOST FLAPPING ALERT: " << name()
      << ";STARTED; Host appears to have started flapping (" << percent_change
      << "% change > " << high_threshold << "% threshold)";
  SPDLOG_LOGGER_WARN(
      runtime_logger,
      "HOST FLAPPING ALERT: {};STARTED; Host appears to have started flapping "
      "({:.1f}% change > {:.1f}% threshold)",
      name(), percent_change, high_threshold);

  /* add a non-persistent comment to the host */
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1)
      << "Notifications for this host are being suppressed because it "
         "was detected as "
      << "having been flapping between different "
         "states ("
      << percent_change << "% change > " << high_threshold
      << "% threshold).  When the host state stabilizes and the "
      << "flapping stops, notifications will be re-enabled.";

  auto com = std::make_shared<comment>(
      comment::host, comment::flapping, host_id(), 0, time(nullptr),
      "(Centreon Engine Process)", oss.str(), false, comment::internal, false,
      (time_t)0);

  comment::comments.insert({com->get_comment_id(), com});

  uint64_t comment_id{com->get_comment_id()};
  set_flapping_comment_id(comment_id);

  /* set the flapping indicator */
  set_is_flapping(true);

  /* send a notification */
  if (allow_flapstart_notification)
    notify(reason_flappingstart, "", "", notifier::notification_option_none);
}

/* handles a host that has stopped flapping */
void host::clear_flap(double percent_change,
                      double high_threshold [[maybe_unused]],
                      double low_threshold) {
  engine_logger(dbg_functions, basic) << "host::clear_flap()";
  SPDLOG_LOGGER_TRACE(functions_logger, "host::clear_flap()");

  engine_logger(dbg_flapping, basic)
      << "Host '" << name() << "' stopped flapping.";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Host '{}' stopped flapping.", name());

  /* log a notice - this one is parsed by the history CGI */
  engine_logger(log_info_message, basic)
      << com::centreon::logging::setprecision(1)
      << "HOST FLAPPING ALERT: " << name()
      << ";STOPPED; Host appears to have stopped flapping (" << percent_change
      << "% change < " << low_threshold << "% threshold)";
  SPDLOG_LOGGER_INFO(
      events_logger,
      "HOST FLAPPING ALERT: {};STOPPED; Host appears to have stopped flapping "
      "({:.1f}% change < {:.1f}% threshold)",
      name(), percent_change, low_threshold);

  /* delete the comment we added earlier */
  if (get_flapping_comment_id() != 0)
    comment::delete_comment(get_flapping_comment_id());
  set_flapping_comment_id(0);

  /* clear the flapping indicator */
  set_is_flapping(false);

  /* send a notification */
  notify(reason_flappingstop, "", "", notifier::notification_option_none);

  /* Send a recovery notification if needed */
  notify(reason_recovery, "", "", notifier::notification_option_none);
}

/**
 * @brief Updates host status info. Data are sent to event broker.
 *
 * @param attributes A bits field based on status_attribute enum (default value:
 * STATUS_ALL).
 */
void host::update_status(uint32_t attributes) {
  broker_host_status(this, attributes);
}

/**
 *  Check if acknowledgement on host expired.
 *
 */
void host::check_for_expired_acknowledgement() {
  if (problem_has_been_acknowledged()) {
    if (acknowledgement_timeout() > 0) {
      time_t now = time(nullptr);
      if (last_acknowledgement() + acknowledgement_timeout() >= now) {
        engine_logger(log_info_message, basic)
            << "Acknowledgement of host '" << name() << "' just expired";
        SPDLOG_LOGGER_INFO(events_logger,
                           "Acknowledgement of host '{}' just expired", name());
        set_acknowledgement(AckType::NONE);
        update_status(STATUS_ACKNOWLEDGEMENT);
      }
    }
  }
}

/* top level host state handler - occurs after every host check (soft/hard and
 * active/passive) */
int host::handle_state() {
  bool state_change = false;
  time_t current_time;
  bool log_host_retries;

  log_host_retries = pb_config.log_host_retries();
  bool use_host_down_disable_service_checks =
      pb_config.host_down_disable_service_checks();

  engine_logger(dbg_functions, basic) << "handle_host_state()";
  SPDLOG_LOGGER_TRACE(functions_logger, "handle_host_state()");

  /* get current time */
  time(&current_time);

  /* obsess over this host check */
  obsessive_compulsive_host_check_processor(this);

  /* update performance data */
  update_performance_data();

  bool have_to_change_service_state = false;

  /* record latest time for current state */
  switch (get_current_state()) {
    case host::state_up:
      set_last_time_up(current_time);
      break;

    case host::state_down:
      set_last_time_down(current_time);
      have_to_change_service_state =
          use_host_down_disable_service_checks && get_state_type() == hard;
      break;

    case host::state_unreachable:
      set_last_time_unreachable(current_time);
      have_to_change_service_state =
          use_host_down_disable_service_checks && get_state_type() == hard;
      break;

    default:
      break;
  }

  /* has the host state changed? */
  if (get_last_state() != get_current_state() ||
      get_last_hard_state() != get_current_state() ||
      (get_current_state() == host::state_up && get_state_type() == soft))
    state_change = true;

  /* if the host state has changed... */
  if (state_change) {
    /* update last state change times */
    if (get_state_type() == soft || get_last_state() != get_current_state())
      set_last_state_change(current_time);
    if (get_state_type() == hard)
      set_last_hard_state_change(current_time);

    /* update the event id */
    set_last_event_id(get_current_event_id());
    set_current_event_id(next_event_id);
    next_event_id++;

    /* update the problem id when transitioning to a problem state */
    if (get_last_state() == host::state_up) {
      /* don't reset last problem id, or it will be zero the next time a problem
       * is encountered */
      /*this->get_last_problem_id=this->get_current_problem_id; */
      set_current_problem_id(next_problem_id);
      next_problem_id++;
    }

    /* clear the problem id when transitioning from a problem state to an UP
     * state */
    if (get_current_state() == host::state_up) {
      set_last_problem_id(get_current_problem_id());
      set_current_problem_id(0L);
    }

    /* reset the acknowledgement flag if necessary */
    if (get_acknowledgement() == AckType::NORMAL) {
      set_acknowledgement(AckType::NONE);

      /* remove any non-persistant comments associated with the ack */
      comment::delete_host_acknowledgement_comments(this);
    } else if (get_acknowledgement() == AckType::STICKY &&
               get_current_state() == host::state_up) {
      set_acknowledgement(AckType::NONE);

      /* remove any non-persistant comments associated with the ack */
      comment::delete_host_acknowledgement_comments(this);
    }

    /* reset the next and last notification times */
    set_last_notification((time_t)0);
    set_next_notification((time_t)0);

    /* reset notification suppression option */
    set_no_more_notifications(false);

    /* write the host state change to the main log file */
    if (get_state_type() == hard ||
        (get_state_type() == soft && log_host_retries))
      log_event();

    /* check for start of flexible (non-fixed) scheduled downtime */
    /* CHANGED 08-05-2010 EG flex downtime can now start on soft states */
    /*if(this->state_type==hard) */
    downtime_manager::instance().check_pending_flex_host_downtime(this);

    /* notify contacts about the recovery or problem if its a "hard" state */
    if (get_current_state_int() == 0)
      notify(reason_recovery, "", "", notifier::notification_option_none);
    else
      notify(reason_normal, "", "", notifier::notification_option_none);

    /* handle the host state change */
    handle_host_event(this);

    /* the host just recovered, so reset the current host attempt */
    if (get_current_state() == host::state_up)
      set_current_attempt(1);

    // have to change service state to UNKNOWN?
    if (have_to_change_service_state) {
      _switch_all_services_to_unknown();
    }
  }
  /* else the host state has not changed */
  else {
    /* notify contacts if needed */
    if (get_current_state() != host::state_up)
      notify(reason_normal, "", "", notifier::notification_option_none);
    else
      notify(reason_recovery, "", "", notifier::notification_option_none);

    /* if we're in a soft state and we should log host retries, do so now... */
    if (get_state_type() == soft && log_host_retries)
      log_event();
  }

  return OK;
}

/* updates host performance data */
void host::update_performance_data() {
  /* should we be processing performance data for anything? */

  bool process_performance_data = pb_config.process_performance_data();
  if (!process_performance_data)
    return;

  /* should we process performance data for this host? */
  if (!get_process_performance_data())
    return;
}

/**
 * @brief Several configurations could be initially handled by host status
 * events. But they are configurations and do not have to be handled like this.
 * So, to have host_status lighter, we removed these items from it but we
 * add a new adaptive host event containing these ones. it is sent when this
 * method is called.
 */
void host::update_adaptive_data() {
  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, this,
                            get_modified_attributes());
}

/* checks viability of performing a host check */
bool host::verify_check_viability(int check_options,
                                  bool* time_is_valid,
                                  time_t* new_time) {
  bool result = true;
  int perform_check = true;
  time_t current_time = 0L;
  time_t preferred_time = 0L;
  int check_interval = 0;

  engine_logger(dbg_functions, basic) << "check_host_check_viability_3x()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_host_check_viability_3x()");

  uint32_t interval_length;
  interval_length = pb_config.interval_length();
  /* get the check interval to use if we need to reschedule the check */
  if (this->get_state_type() == soft &&
      this->get_current_state() != host::state_up)
    check_interval = static_cast<int>(this->retry_interval() * interval_length);
  else
    check_interval = static_cast<int>(this->check_interval() * interval_length);

  /* make sure check interval is positive - otherwise use 5 minutes out for next
   * check */
  if (check_interval <= 0)
    check_interval = 300;

  /* get the current time */
  time(&current_time);

  /* initialize the next preferred check time */
  preferred_time = current_time;

  /* can we check the host right now? */
  if (!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
    /* if checks of the host are currently disabled... */
    if (!this->active_checks_enabled()) {
      preferred_time = current_time + check_interval;
      perform_check = false;
    }

    // Make sure this is a valid time to check the host.
    {
      timezone_locker lock(get_timezone());
      if (!check_time_against_period(static_cast<unsigned long>(current_time),
                                     this->check_period_ptr)) {
        preferred_time = current_time;
        if (time_is_valid)
          *time_is_valid = false;
        perform_check = false;
      }
    }

    /* check host dependencies for execution */
    if (!authorized_by_dependencies(hostdependency::execution)) {
      preferred_time = current_time + check_interval;
      perform_check = false;
    }
  }

  /* pass back the next viable check time */
  if (new_time)
    *new_time = preferred_time;

  result = (perform_check) ? true : false;
  return result;
}

void host::grab_macros_r(nagios_macros* mac) {
  grab_host_macros_r(mac, this);
}

/* notify a specific contact that an entire host is down or up */
int host::notify_contact(nagios_macros* mac,
                         contact* cntct,
                         notifier::reason_type type,
                         const std::string& not_author,
                         const std::string& not_data,
                         int options __attribute((unused)),
                         int escalated) {
  std::string raw_command;
  std::string processed_command;
  bool early_timeout = false;
  double exectime;
  struct timeval start_time, end_time;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  engine_logger(dbg_functions, basic) << "notify_contact_of_host()";
  SPDLOG_LOGGER_TRACE(functions_logger, "notify_contact_of_host()");
  engine_logger(dbg_notifications, most)
      << "** Notifying contact '" << cntct->get_name() << "'";
  notifications_logger->debug("** Notifying contact '{}'", cntct->get_name());

  bool log_notifications;
  uint32_t notification_timeout;
  log_notifications = pb_config.log_notifications();
  notification_timeout = pb_config.notification_timeout();

  /* get start time */
  gettimeofday(&start_time, nullptr);

  /* process all the notification commands this user has */
  for (std::shared_ptr<commands::command> const& cmd :
       cntct->get_host_notification_commands()) {
    /* get the raw command line */
    get_raw_command_line_r(mac, cmd, cmd->get_command_line().c_str(),
                           raw_command, macro_options);
    if (raw_command.empty())
      continue;

    engine_logger(dbg_notifications, most)
        << "Raw notification command: " << raw_command;
    notifications_logger->debug("Raw notification command: {}", raw_command);

    /* process any macros contained in the argument */
    process_macros_r(mac, raw_command, processed_command, macro_options);
    if (processed_command.empty())
      continue;

    /* run the notification command... */

    engine_logger(dbg_notifications, most)
        << "Processed notification command: " << processed_command;
    notifications_logger->trace("Processed notification command: {}",
                                processed_command);

    /* log the notification to program log file */
    if (log_notifications) {
      char const* host_state_str("UP");
      if ((unsigned int)_current_state < tab_host_states.size())
        // sizeof(tab_host_state_str) / sizeof(*tab_host_state_str))
        host_state_str = tab_host_states[_current_state].second.c_str();

      char const* notification_str("");
      if ((unsigned int)type < tab_notification_str.size())
        notification_str = tab_notification_str[type].c_str();

      std::string info;
      if (type == reason_custom)
        notification_str = "CUSTOM";
      else if (type == reason_acknowledgement)
        info.append(";").append(not_author).append(";").append(not_data);

      std::string host_notification_state;
      if (strcmp(notification_str, "NORMAL") == 0)
        host_notification_state.append(host_state_str);
      else
        host_notification_state.append(notification_str)
            .append(" (")
            .append(host_state_str)
            .append(")");

      engine_logger(log_host_notification, basic)
          << "HOST NOTIFICATION: " << cntct->get_name() << ';' << this->name()
          << ';' << host_notification_state << ";" << cmd->get_name() << ';'
          << this->get_plugin_output() << info;
      notifications_logger->info("HOST NOTIFICATION: {};{};{};{};{}{}",
                                 cntct->get_name(), this->name(),
                                 host_notification_state, cmd->get_name(),
                                 this->get_plugin_output(), info);
    }

    /* run the notification command */
    if (command_is_allowed_by_whitelist(processed_command, NOTIF_TYPE)) {
      try {
        std::string out;
        my_system_r(mac, processed_command, notification_timeout,
                    &early_timeout, &exectime, out, 0);
      } catch (std::exception const& e) {
        engine_logger(log_runtime_error, basic)
            << "Error: can't execute host notification for contact '"
            << cntct->get_name() << "' : " << e.what();
        runtime_logger->error(
            "Error: can't execute host notification for contact '{}' : {}",
            cntct->get_name(), e.what());
      }
    } else {
      runtime_logger->error(
          "Error: can't execute host notification for contact '{}' : it is not "
          "allowed by the whitelist",
          cntct->get_name());
    }

    /* check to see if the notification timed out */
    if (early_timeout) {
      engine_logger(log_host_notification | log_runtime_warning, basic)
          << "Warning: Contact '" << cntct->get_name()
          << "' host notification command '" << processed_command
          << "' timed out after " << notification_timeout << " seconds";
      notifications_logger->info(
          "Warning: Contact '{}' host notification command '{}' timed out "
          "after {} seconds",
          cntct->get_name(), processed_command, notification_timeout);
    }
  }

  /* get end time */
  gettimeofday(&end_time, nullptr);

  /* update the contact's last host notification time */
  cntct->set_last_host_notification(start_time.tv_sec);

  return OK;
}

void host::update_notification_flags() {
  /* update notifications flags */
  if (get_current_state() == host::state_down)
    add_notified_on(down);
  else if (get_current_state() == host::state_unreachable)
    add_notified_on(unreachable);
}

/* disables flap detection for a specific host */
void host::disable_flap_detection() {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  engine_logger(dbg_functions, basic) << "disable_host_flap_detection()";
  SPDLOG_LOGGER_TRACE(functions_logger, "disable_host_flap_detection()");

  engine_logger(dbg_functions, more)
      << "Disabling flap detection for host '" << name() << "'.";
  functions_logger->debug("Disabling flap detection for host '{}'.", name());

  /* nothing to do... */
  if (!flap_detection_enabled())
    return;

  /* set the attribute modified flag */
  add_modified_attributes(attr);

  /* set the flap detection enabled flag */
  set_flap_detection_enabled(false);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, this,
                            attr);

  /* handle the details... */
  handle_flap_detection_disabled();
}

/* enables flap detection for a specific host */
void host::enable_flap_detection() {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  engine_logger(dbg_functions, basic) << "host::enable_flap_detection()";
  SPDLOG_LOGGER_TRACE(functions_logger, "host::enable_flap_detection()");

  engine_logger(dbg_flapping, more)
      << "Enabling flap detection for host '" << name() << "'.";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Enabling flap detection for host '{}'.",
                      name());

  /* nothing to do... */
  if (flap_detection_enabled())
    return;

  /* set the attribute modified flag */
  add_modified_attributes(attr);

  /* set the flap detection enabled flag */
  set_flap_detection_enabled(true);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, this,
                            attr);

  /* check for flapping */
  check_for_flapping(false, false, true);

  /* update host status */
  /* FIXME DBO: seems not necessary */
  // update_status();
}

/*
 * checks to see if a host escalation entry is a match for the current host
 * notification
 */
bool host::is_valid_escalation_for_notification(escalation const* e,
                                                int options) const {
  uint32_t notification_number;
  time_t current_time;

  engine_logger(dbg_functions, basic)
      << "host::is_valid_escalation_for_notification()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "host::is_valid_escalation_for_notification()");

  /* get the current time */
  time(&current_time);

  /*
   * if this is a recovery, really we check for who got notified about a
   * previous problem
   */
  if (get_current_state() == host::state_up)
    notification_number = get_notification_number() - 1;
  else
    notification_number = get_notification_number();

  /* find the host this escalation entry is associated with */
  if (e->notifier_ptr != this)
    return false;

  /*** EXCEPTION ***/
  /* broadcast options go to everyone, so this escalation is valid */
  if (options & notifier::notification_option_broadcast)
    return true;

  /* skip this escalation if it happens later */
  if (e->get_first_notification() > notification_number)
    return false;

  /* skip this escalation if it has already passed */
  if (e->get_last_notification() != 0 &&
      e->get_last_notification() < notification_number)
    return false;

  /*
   * skip this escalation if it has a timeperiod and the current time
   * isn't valid
   */
  if (!e->get_escalation_period().empty() &&
      !check_time_against_period(current_time, e->escalation_period_ptr))
    return false;

  /* skip this escalation if the state options don't match */
  if (get_current_state() == host::state_up && !e->get_escalate_on(up))
    return false;
  else if (get_current_state() == host::state_down && !e->get_escalate_on(down))
    return false;
  else if (get_current_state() == host::state_unreachable &&
           !e->get_escalate_on(unreachable))
    return false;

  return true;
}

/* checks to see if a hosts's check results are fresh */
bool host::is_result_fresh(time_t current_time, int log_this) {
  time_t expiration_time = 0L;
  int freshness_threshold = 0;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int tdays = 0;
  int thours = 0;
  int tminutes = 0;
  int tseconds = 0;

  uint32_t interval_length;
  int32_t additional_freshness_latency;
  uint32_t max_host_check_spread;
  interval_length = pb_config.interval_length();
  additional_freshness_latency = pb_config.additional_freshness_latency();
  max_host_check_spread = pb_config.max_host_check_spread();

  engine_logger(dbg_checks, most)
      << "Checking freshness of host '" << name() << "'...";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Checking freshness of host '{}'...",
                      name());

  /* use user-supplied freshness threshold or auto-calculate a freshness
   * threshold to use? */
  if (get_freshness_threshold() == 0) {
    double interval;
    if ((hard == get_state_type()) || (host::state_up == get_current_state()))
      interval = check_interval();
    else
      interval = retry_interval();
    freshness_threshold =
        static_cast<int>(interval * interval_length + get_latency() +
                         additional_freshness_latency);
  } else
    freshness_threshold = get_freshness_threshold();

  engine_logger(dbg_checks, most)
      << "Freshness thresholds: host=" << get_freshness_threshold()
      << ", use=" << freshness_threshold;
  SPDLOG_LOGGER_DEBUG(checks_logger, "Freshness thresholds: host={}, use={}",
                      get_freshness_threshold(), freshness_threshold);

  /* calculate expiration time */
  /* CHANGED 11/10/05 EG - program start is only used in expiration time
   * calculation if > last check AND active checks are enabled, so active checks
   * can become stale immediately upon program startup */
  if (!has_been_checked())
    expiration_time = (time_t)(event_start + freshness_threshold);
  /* CHANGED 06/19/07 EG - Per Ton's suggestion (and user requests), only use
   * program start time over last check if no specific threshold has been set by
   * user.  Otheriwse use it.  Problems can occur if Engine is restarted more
   * frequently that freshness threshold intervals (hosts never go stale). */
  /* CHANGED 10/07/07 EG - Added max_host_check_spread to expiration time as
   * suggested by Altinity */
  else if (active_checks_enabled() && event_start > get_last_check() &&
           get_freshness_threshold() == 0)
    expiration_time = (time_t)(event_start + freshness_threshold +
                               max_host_check_spread * interval_length);
  else
    expiration_time = (time_t)(get_last_check() + freshness_threshold);

  engine_logger(dbg_checks, most)
      << "HBC: " << has_been_checked() << ", PS: " << program_start
      << ", ES: " << event_start << ", LC: " << get_last_check()
      << ", CT: " << current_time << ", ET: " << expiration_time;
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "HBC: {}, PS: {}, ES: {}, LC: {}, CT: {}, ET: {}",
                      has_been_checked(), program_start, event_start,
                      get_last_check(), current_time, expiration_time);

  /* the results for the last check of this host are stale */
  if (expiration_time < current_time) {
    get_time_breakdown((current_time - expiration_time), &days, &hours,
                       &minutes, &seconds);
    get_time_breakdown(freshness_threshold, &tdays, &thours, &tminutes,
                       &tseconds);

    /* log a warning */
    if (log_this)
      engine_logger(log_runtime_warning, basic)
          << "Warning: The results of host '" << name() << "' are stale by "
          << days << "d " << hours << "h " << minutes << "m " << seconds
          << "s (threshold=" << tdays << "d " << thours << "h " << tminutes
          << "m " << tseconds
          << "s).  I'm forcing an immediate check of"
             " the host.";
    SPDLOG_LOGGER_WARN(
        runtime_logger,
        "Warning: The results of host '{}' are stale by {}d {}h {}m {}s "
        "(threshold={}d {}h {}m {}s).  I'm forcing an immediate check of the "
        "host.",
        name(), days, hours, minutes, seconds, tdays, thours, tminutes,
        tseconds);

    engine_logger(dbg_checks, more)
        << "Check results for host '" << name() << "' are stale by " << days
        << "d " << hours << "h " << minutes << "m " << seconds
        << "s (threshold=" << tdays << "d " << thours << "h " << tminutes
        << "m " << tseconds
        << "s).  "
           "Forcing an immediate check of the host...";
    SPDLOG_LOGGER_DEBUG(
        checks_logger,
        "Check results for host '{}' are stale by {}d {}h {}m {}s "
        "(threshold={}d {}h {}m {}s). Forcing an immediate check of the "
        "host...",
        name(), days, hours, minutes, seconds, tdays, thours, tminutes,
        tseconds);

    return false;
  } else
    engine_logger(dbg_checks, more)
        << "Check results for host '" << this->name() << "' are fresh.";
  SPDLOG_LOGGER_DEBUG(checks_logger, "Check results for host '{}' are fresh.",
                      this->name());

  return true;
}

/* handles the details for a host when flap detection is disabled (globally or
 * per-host) */
void host::handle_flap_detection_disabled() {
  engine_logger(dbg_functions, basic)
      << "handle_host_flap_detection_disabled()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "handle_host_flap_detection_disabled()");
  /* if the host was flapping, remove the flapping indicator */
  if (get_is_flapping()) {
    this->set_is_flapping(false);

    /* delete the original comment we added earlier */
    if (this->get_flapping_comment_id() != 0)
      comment::delete_comment(this->get_flapping_comment_id());
    this->set_flapping_comment_id(0);

    /* log a notice - this one is parsed by the history CGI */
    engine_logger(log_info_message, basic)
        << "HOST FLAPPING ALERT: " << this->name()
        << ";DISABLED; Flap detection has been disabled";
    SPDLOG_LOGGER_INFO(
        events_logger,
        "HOST FLAPPING ALERT: {};DISABLED; Flap detection has been disabled",
        this->name());

    /* send a notification */
    notify(reason_flappingdisabled, "", "", notifier::notification_option_none);

    /* Send a recovery notification if needed */
    notify(reason_recovery, "", "", notification_option_none);
  }

  /* update host status */
  update_status();
}

int host::perform_on_demand_check(enum host::host_state* check_return_code,
                                  int check_options,
                                  int use_cached_result,
                                  unsigned long check_timestamp_horizon) {
  engine_logger(dbg_functions, basic) << "perform_on_demand_host_check()";
  SPDLOG_LOGGER_TRACE(functions_logger, "perform_on_demand_host_check()");

  perform_on_demand_check_3x(check_return_code, check_options,
                             use_cached_result, check_timestamp_horizon);
  return OK;
}

/* check to see if we can reach the host */
int host::perform_on_demand_check_3x(host::host_state* check_result_code,
                                     int check_options,
                                     int use_cached_result,
                                     unsigned long check_timestamp_horizon) {
  int result = OK;

  engine_logger(dbg_functions, basic) << "perform_on_demand_host_check_3x()";
  SPDLOG_LOGGER_TRACE(functions_logger, "perform_on_demand_host_check_3x()");

  engine_logger(dbg_checks, basic)
      << "** On-demand check for host '" << name() << "'...";
  SPDLOG_LOGGER_TRACE(checks_logger, "** On-demand check for host '{}'...",
                      name());

  /* check the status of the host */
  result = this->run_sync_check_3x(check_result_code, check_options,
                                   use_cached_result, check_timestamp_horizon);
  return result;
}

/* perform a synchronous check of a host */ /* on-demand host checks will use
                                               this... */
int host::run_sync_check_3x(enum host::host_state* check_result_code,
                            int check_options,
                            int use_cached_result,
                            unsigned long check_timestamp_horizon) {
  engine_logger(dbg_functions, basic)
      << "run_sync_host_check_3x: hst=" << this
      << ", check_options=" << check_options
      << ", use_cached_result=" << use_cached_result
      << ", check_timestamp_horizon=" << check_timestamp_horizon;
  SPDLOG_LOGGER_TRACE(
      functions_logger,
      "run_sync_host_check_3x: hst={}, check_options={}, use_cached_result={}, "
      "check_timestamp_horizon={}",
      (void*)this, check_options, use_cached_result, check_timestamp_horizon);

  try {
    checks::checker::instance().run_sync(this, check_result_code, check_options,
                                         use_cached_result,
                                         check_timestamp_horizon);
  } catch (std::exception const& e) {
    engine_logger(log_runtime_error, basic) << "Error: " << e.what();
    runtime_logger->error("Error: {}", e.what());
    return ERROR;
  }
  return OK;
}

/* processes the result of a synchronous or asynchronous host check */
int host::process_check_result_3x(enum host::host_state new_state,
                                  const std::string& old_plugin_output,
                                  int check_options,
                                  int reschedule_check,
                                  int use_cached_result,
                                  unsigned long check_timestamp_horizon) {
  com::centreon::engine::host* master_host = nullptr;
  host* temp_host;
  std::list<host*> check_hostlist;
  host::host_state parent_state = host::state_up;
  time_t current_time = 0L;

  uint32_t interval_length;
  bool log_passive_checks;
  bool enable_predictive_host_dependency_checks;
  interval_length = pb_config.interval_length();
  log_passive_checks = pb_config.log_passive_checks();
  enable_predictive_host_dependency_checks =
      pb_config.enable_predictive_host_dependency_checks();

  time_t next_check{get_last_check() + check_interval() * interval_length};
  time_t preferred_time = 0L;
  time_t next_valid_time = 0L;
  int run_async_check = true;
  bool has_parent;

  engine_logger(dbg_functions, basic) << "process_host_check_result_3x()";
  SPDLOG_LOGGER_TRACE(functions_logger, "process_host_check_result_3x()");

  engine_logger(dbg_checks, more)
      << "HOST: " << name() << ", ATTEMPT=" << get_current_attempt() << "/"
      << max_check_attempts() << ", CHECK TYPE="
      << (get_check_type() == check_active ? "ACTIVE" : "PASSIVE")
      << ", STATE TYPE=" << (get_state_type() == hard ? "HARD" : "SOFT")
      << ", OLD STATE=" << get_current_state() << ", NEW STATE=" << new_state;
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "HOST: {}, ATTEMPT={}/{}, CHECK TYPE={}, STATE TYPE={}, OLD STATE={}, "
      "NEW STATE={}",
      name(), get_current_attempt(), max_check_attempts(),
      get_check_type() == check_active ? "ACTIVE" : "PASSIVE",
      get_state_type() == hard ? "HARD" : "SOFT",
      static_cast<uint32_t>(get_current_state()),
      static_cast<uint32_t>(new_state));

  /* we have to adjust current attempt # for passive checks, as it isn't done
   * elsewhere */
  if (get_check_type() == check_passive)
    adjust_check_attempt(false);

  /* log passive checks - we need to do this here, as some my bypass external
   * commands by getting dropped in checkresults dir */
  if (get_check_type() == check_passive) {
    if (log_passive_checks)
      engine_logger(log_passive_check, basic)
          << "PASSIVE HOST CHECK: " << name() << ";" << new_state << ";"
          << get_plugin_output();
    SPDLOG_LOGGER_DEBUG(checks_logger, "PASSIVE HOST CHECK: {};{};{}", name(),
                        static_cast<uint32_t>(new_state), get_plugin_output());
  }

  /******* HOST WAS DOWN/UNREACHABLE INITIALLY *******/
  if (_current_state != host::state_up) {
    engine_logger(dbg_checks, more) << "Host was DOWN/UNREACHABLE.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Host was DOWN/UNREACHABLE.");

    /***** HOST IS NOW UP *****/
    /* the host just recovered! */
    if (new_state == host::state_up) {
      /* set the current state */
      _current_state = host::state_up;

      /* set the state type */
      /* set state type to HARD for passive checks and active checks that were
       * previously in a HARD STATE */
      if (get_state_type() == hard)
        set_state_type(hard);
      else
        set_state_type(soft);

      engine_logger(dbg_checks, more)
          << "Host experienced a "
          << (get_state_type() == hard ? "HARD" : "SOFT")
          << " recovery (it's now UP).";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "Host experienced a {} recovery (it's now UP).",
                          get_state_type() == hard ? "HARD" : "SOFT");

      /* reschedule the next check of the host at the normal interval */
      reschedule_check = true;

      /* propagate checks to immediate parents if they are not already UP */
      /* we do this because a parent host (or grandparent) may have recovered
       * somewhere and we should catch the recovery as soon as possible */
      engine_logger(dbg_checks, more)
          << "Propagating checks to parent host(s)...";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "Propagating checks to parent host(s)...");

      for (const auto& [key, sptr_host] : parent_hosts) {
        if (!sptr_host)
          continue;
        if (sptr_host->get_current_state() != host::state_up) {
          engine_logger(dbg_checks, more)
              << "Check of parent host '" << key << "' queued.";
          SPDLOG_LOGGER_DEBUG(checks_logger,
                              "Check of parent host '{}' queued.", key);
          check_hostlist.push_back(sptr_host.get());
        }
      }

      /* propagate checks to immediate children if they are not already UP */
      /* we do this because children may currently be UNREACHABLE, but may (as a
       * result of this recovery) switch to UP or DOWN states */
      engine_logger(dbg_checks, more)
          << "Propagating checks to child host(s)...";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "Propagating checks to child host(s)...");

      for (const auto& [key, ptr_host] : child_hosts) {
        if (!ptr_host)
          continue;
        if (ptr_host->get_current_state() != host::state_up) {
          engine_logger(dbg_checks, more)
              << "Check of child host '" << key << "' queued.";
          SPDLOG_LOGGER_DEBUG(checks_logger, "Check of child host '{}' queued.",
                              key);
          check_hostlist.push_back(ptr_host);
        }
      }
    }

    /***** HOST IS STILL DOWN/UNREACHABLE *****/
    /* we're still in a problem state... */
    else {
      engine_logger(dbg_checks, more) << "Host is still DOWN/UNREACHABLE.";
      SPDLOG_LOGGER_DEBUG(checks_logger, "Host is still DOWN/UNREACHABLE.");

      /* set the state type */
      /* we've maxed out on the retries */
      if (get_current_attempt() == max_check_attempts())
        set_state_type(hard);
      /* the host was in a hard problem state before, so it still is now */
      else if (get_current_attempt() == 1)
        set_state_type(hard);
      /* the host is in a soft state and the check will be retried */
      else
        set_state_type(soft);

      /* make a determination of the host's state */
      /* translate host state between DOWN/UNREACHABLE */
      _current_state = determine_host_reachability(new_state);

      /* reschedule the next check if the host state changed */
      if (_last_state != _current_state || _last_hard_state != _current_state) {
        reschedule_check = true;
        /* schedule a re-check of the host at the retry interval because we
         * can't determine its final state yet... */
        if (get_state_type() == soft)
          next_check = get_last_check() + retry_interval() * interval_length;
      }
    }
  }

  /******* HOST WAS UP INITIALLY *******/
  else {
    engine_logger(dbg_checks, more) << "Host was UP.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Host was UP.");

    /***** HOST IS STILL UP *****/
    /* either the host never went down since last check */
    if (new_state == host::state_up) {
      engine_logger(dbg_checks, more) << "Host is still UP.";
      SPDLOG_LOGGER_DEBUG(checks_logger, "Host is still UP.");

      /* set the current state */
      _current_state = host::state_up;

      /* set the state type */
      set_state_type(hard);

    }
    /***** HOST IS NOW DOWN/UNREACHABLE *****/
    else {
      engine_logger(dbg_checks, more) << "Host is now DOWN/UNREACHABLE.";
      SPDLOG_LOGGER_DEBUG(checks_logger, "Host is now DOWN/UNREACHABLE.");

      /***** SPECIAL CASE FOR HOSTS WITH MAX_ATTEMPTS==1 *****/
      if (max_check_attempts() == 1) {
        engine_logger(dbg_checks, more) << "Max attempts = 1!.";
        SPDLOG_LOGGER_DEBUG(checks_logger, "Max attempts = 1!.");

        /* set the state type */
        set_state_type(hard);

        /* host has maxed out on retries, so reschedule the next check at the
         * normal interval */
        reschedule_check = true;

        /* we need to run SYNCHRONOUS checks of all parent hosts to accurately
         * determine the state of this host */
        /* this is extremely inefficient (reminiscent of Nagios 2.x logic), but
         * there's no other good way around it */
        /* check all parent hosts to see if we're DOWN or UNREACHABLE */
        /* only do this for ACTIVE checks, as PASSIVE checks contain a
         * pre-determined state */
        if (get_check_type() == check_active) {
          has_parent = false;

          engine_logger(dbg_checks, more)
              << "** WARNING: Max attempts = 1, so we have to run serial "
                 "checks of all parent hosts!";
          SPDLOG_LOGGER_DEBUG(
              checks_logger,
              "** WARNING: Max attempts = 1, so we have to run serial "
              "checks of all parent hosts!");

          for (const auto& [key, sptr_host] : parent_hosts) {
            if (!sptr_host)
              continue;

            has_parent = true;

            engine_logger(dbg_checks, more)
                << "Running serial check parent host '" << key << "'...";
            SPDLOG_LOGGER_DEBUG(
                checks_logger, "Running serial check parent host '{}'...", key);

            /* run an immediate check of the parent host */
            sptr_host->run_sync_check_3x(&parent_state, check_options,
                                         use_cached_result,
                                         check_timestamp_horizon);

            /* bail out as soon as we find one parent host that is UP */
            if (parent_state == host::state_up) {
              engine_logger(dbg_checks, more)
                  << "Parent host is UP, so this one is DOWN.";
              SPDLOG_LOGGER_DEBUG(checks_logger,
                                  "Parent host is UP, so this one is DOWN.");

              /* set the current state */
              _current_state = host::state_down;
              break;
            }
          }

          if (!has_parent) {
            /* host has no parents, so its up */
            if (parent_hosts.empty()) {
              engine_logger(dbg_checks, more)
                  << "Host has no parents, so it's DOWN.";
              SPDLOG_LOGGER_DEBUG(checks_logger,
                                  "Host has no parents, so it's DOWN.");
              _current_state = host::state_down;
            } else {
              /* no parents were up, so this host is UNREACHABLE */
              engine_logger(dbg_checks, more)
                  << "No parents were UP, so this host is UNREACHABLE.";
              SPDLOG_LOGGER_DEBUG(
                  checks_logger,
                  "No parents were UP, so this host is UNREACHABLE.");
              _current_state = host::state_unreachable;
            }
          }
        }
        /* set the host state for passive checks */
        else {
          /* set the state */
          /* translate host state between DOWN/UNREACHABLE */
          /* make a determination of the host's state */
          _current_state = determine_host_reachability(new_state);
        }

        /* propagate checks to immediate children if they are not UNREACHABLE */
        /* we do this because we may now be blocking the route to child hosts */
        engine_logger(dbg_checks, more)
            << "Propagating check to immediate non-UNREACHABLE child hosts...";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "Propagating check to immediate non-UNREACHABLE child hosts...");

        for (const auto& [key, ptr_host] : child_hosts) {
          if (!ptr_host)
            continue;
          if (ptr_host->get_current_state() != host::state_unreachable) {
            engine_logger(dbg_checks, more)
                << "Check of child host '" << key << "' queued.";
            SPDLOG_LOGGER_DEBUG(checks_logger,
                                "Check of child host '{}' queued.", key);
            check_hostlist.push_back(ptr_host);
          }
        }
      }
      /***** MAX ATTEMPTS > 1 *****/
      else {
        /* active and passive check results are treated as SOFT states */
        /* set the state type */
        set_state_type(soft);

        /* make a (in some cases) preliminary determination of the host's state
         */
        /* translate host state between DOWN/UNREACHABLE  */
        _current_state = determine_host_reachability(new_state);

        /* reschedule a check of the host */
        reschedule_check = true;

        /* schedule a re-check of the host at the retry interval because we
         * can't determine its final state yet... */
        next_check = get_last_check() + retry_interval() * interval_length;

        /* propagate checks to immediate parents if they are UP */
        /* we do this because a parent host (or grandparent) may have gone down
         * and blocked our route */
        /* checking the parents ASAP will allow us to better determine the final
         * state (DOWN/UNREACHABLE) of this host later */
        engine_logger(dbg_checks, more)
            << "Propagating checks to immediate parent hosts that "
               "are UP...";
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "Propagating checks to immediate parent hosts that "
                            "are UP...");

        for (const auto& [key, sptr_host] : parent_hosts) {
          if (sptr_host == nullptr)
            continue;
          if (sptr_host->get_current_state() == host::state_up) {
            check_hostlist.push_back(sptr_host.get());
            engine_logger(dbg_checks, more)
                << "Check of host '" << key << "' queued.";
            SPDLOG_LOGGER_DEBUG(checks_logger, "Check of host '{}' queued.",
                                key);
          }
        }

        /* propagate checks to immediate children if they are not UNREACHABLE */
        /* we do this because we may now be blocking the route to child hosts */
        engine_logger(dbg_checks, more)
            << "Propagating checks to immediate non-UNREACHABLE "
               "child hosts...";
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "Propagating checks to immediate non-UNREACHABLE "
                            "child hosts...");

        for (const auto& [key, ptr_host] : child_hosts) {
          if (!ptr_host)
            continue;
          if (ptr_host->get_current_state() != host::state_unreachable) {
            engine_logger(dbg_checks, more)
                << "Check of child host '" << key << "' queued.";
            SPDLOG_LOGGER_DEBUG(checks_logger,
                                "Check of child host '{}' queued.", key);
            check_hostlist.push_back(ptr_host);
          }
        }

        /* check dependencies on second to last host check */
        if (enable_predictive_host_dependency_checks &&
            get_current_attempt() == max_check_attempts() - 1) {
          /* propagate checks to hosts that THIS ONE depends on for
           * notifications AND execution */
          /* we do to help ensure that the dependency checks are accurate before
           * it comes time to notify */
          engine_logger(dbg_checks, more)
              << "Propagating predictive dependency checks to hosts this "
                 "one depends on...";
          SPDLOG_LOGGER_DEBUG(
              checks_logger,
              "Propagating predictive dependency checks to hosts this "
              "one depends on...");

          for (auto it = hostdependency::hostdependencies.find(name()),
                    end = hostdependency::hostdependencies.end();
               it != end && it->first == name(); ++it) {
            hostdependency* temp_dependency(it->second.get());
            if (temp_dependency->dependent_host_ptr == this &&
                temp_dependency->master_host_ptr != nullptr) {
              master_host = (host*)temp_dependency->master_host_ptr;
              engine_logger(dbg_checks, more)
                  << "Check of host '" << master_host->name() << "' queued.";
              SPDLOG_LOGGER_DEBUG(checks_logger, "Check of host '{}' queued.",
                                  master_host->name());
              check_hostlist.push_back(master_host);
            }
          }
        }
      }
    }
  }

  engine_logger(dbg_checks, more)
      << "Pre-handle_host_state() Host: " << name()
      << ", Attempt=" << get_current_attempt() << "/" << max_check_attempts()
      << ", Type=" << (get_state_type() == hard ? "HARD" : "SOFT")
      << ", Final State=" << _current_state;
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "Pre-handle_host_state() Host: {}, Attempt={}/{}, Type={}, Final "
      "State={}",
      name(), get_current_attempt(), max_check_attempts(),
      get_state_type() == hard ? "HARD" : "SOFT",
      static_cast<uint32_t>(_current_state));

  /* handle the host state */
  handle_state();

  engine_logger(dbg_checks, more)
      << "Post-handle_host_state() Host: " << name()
      << ", Attempt=" << get_current_attempt() << "/" << max_check_attempts()
      << ", Type=" << (get_state_type() == hard ? "HARD" : "SOFT")
      << ", Final State=" << _current_state;
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "Post-handle_host_state() Host: {}, Attempt={}/{}, Type={}, Final "
      "State={}",
      name(), get_current_attempt(), max_check_attempts(),
      get_state_type() == hard ? "HARD" : "SOFT",
      static_cast<uint32_t>(_current_state));

  /******************** POST-PROCESSING STUFF *********************/

  /* if the plugin output differs from previous check and no state change, log
   * the current state/output if state stalking is enabled */
  if (_last_state == _current_state &&
      old_plugin_output == get_plugin_output()) {
    if (_current_state == host::state_up && get_stalk_on(up))
      log_event();

    else if (_current_state == host::state_down && get_stalk_on(down))
      log_event();

    else if (_current_state == host::state_unreachable &&
             get_stalk_on(unreachable))
      log_event();
  }

  /* check to see if the associated host is flapping */
  check_for_flapping(true, true, true);

  /* reschedule the next check of the host (usually ONLY for scheduled, active
   * checks, unless overridden above) */
  bool sent = false;
  if (reschedule_check) {
    engine_logger(dbg_checks, more)
        << "Rescheduling next check of host at " << my_ctime(&next_check);
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Rescheduling next check of host: {} of last check at "
                        "{:%Y-%m-%dT%H:%M:%S} and next "
                        "check at {:%Y-%m-%dT%H:%M:%S}",
                        name(), fmt::localtime(get_last_check()),
                        fmt::localtime(next_check));

    /* default is to reschedule host check unless a test below fails... */
    set_should_be_scheduled(true);

    /* get the new current time */
    current_time = std::time(nullptr);

    /* make sure we don't get ourselves into too much trouble... */
    if (current_time > next_check)
      set_next_check(current_time);
    else
      set_next_check(next_check);

    // Make sure we rescheduled the next host check at a valid time.
    {
      timezone_locker lock{get_timezone()};
      preferred_time = get_next_check();
      get_next_valid_time(preferred_time, &next_valid_time, check_period_ptr);
      set_next_check(next_valid_time);
    }

    /* hosts with non-recurring intervals do not get rescheduled if we're in a
     * HARD or UP state */
    if (check_interval() == 0 &&
        (get_state_type() == hard || _current_state == host::state_up))
      set_should_be_scheduled(false);

    /* host with active checks disabled do not get rescheduled */
    if (!active_checks_enabled())
      set_should_be_scheduled(false);

    /* schedule a non-forced check if we can */
    if (get_should_be_scheduled())
      sent = schedule_check(get_next_check(), CHECK_OPTION_NONE);
  }

  /* update host status - for both active (scheduled) and passive
   * (non-scheduled) hosts */
  /* This condition is to avoid to send host status twice. */
  if (!sent)
    update_status();

  /* run async checks of all hosts we added above */
  /* don't run a check if one is already executing or we can get by with a
   * cached state */
  for (std::list<host*>::iterator it{check_hostlist.begin()},
       end{check_hostlist.end()};
       it != end; ++it) {
    run_async_check = true;
    temp_host = *it;

    engine_logger(dbg_checks, most)
        << "ASYNC CHECK OF HOST: " << temp_host->name()
        << ", CURRENTTIME: " << current_time
        << ", LASTHOSTCHECK: " << temp_host->get_last_check()
        << ", CACHEDTIMEHORIZON: " << check_timestamp_horizon
        << ", USECACHEDRESULT: " << use_cached_result
        << ", ISEXECUTING: " << temp_host->get_is_executing();
    SPDLOG_LOGGER_DEBUG(
        checks_logger,
        "ASYNC CHECK OF HOST: {}, CURRENTTIME: {}, LASTHOSTCHECK: {}, "
        "CACHEDTIMEHORIZON: {}, USECACHEDRESULT: {}, ISEXECUTING: {}",
        temp_host->name(), current_time, temp_host->get_last_check(),
        check_timestamp_horizon, use_cached_result,
        temp_host->get_is_executing());

    if (use_cached_result && (static_cast<unsigned long>(
                                  current_time - temp_host->get_last_check()) <=
                              check_timestamp_horizon))
      run_async_check = false;
    if (temp_host->get_is_executing())
      run_async_check = false;
    if (run_async_check)
      temp_host->run_async_check(CHECK_OPTION_NONE, 0.0, false, false, nullptr,
                                 nullptr);
  }
  return OK;
}

/* determination of the host's state based on route availability*/ /* used only
                                                                      to
                                                                      determine
                                                                      difference
                                                                      between
                                                                      DOWN and
                                                                      UNREACHABLE
                                                                      states */
enum host::host_state host::determine_host_reachability(
    enum host::host_state new_state) {
  enum host::host_state state = host::state_down;
  bool is_host_present = false;

  engine_logger(dbg_functions, basic) << "determine_host_reachability()";
  SPDLOG_LOGGER_TRACE(functions_logger, "determine_host_reachability()");

  engine_logger(dbg_checks, most) << "Determining state of host '" << name()
                                  << "': current state=" << new_state;
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Determining state of host '{}': current state= {}",
                      name(), static_cast<uint32_t>(new_state));

  /* host is UP - no translation needed */
  if (new_state == host::state_up) {
    state = host::state_up;
    engine_logger(dbg_checks, most)
        << "Host is UP, no state translation needed.";
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Host is UP, no state translation needed.");
  }

  /* host has no parents, so it is DOWN */
  else if (parent_hosts.size() == 0) {
    state = host::state_down;
    engine_logger(dbg_checks, most) << "Host has no parents, so it is DOWN.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Host has no parents, so it is DOWN.");
  }

  /* check all parent hosts to see if we're DOWN or UNREACHABLE */
  else {
    for (const auto& [key, sptr_host] : parent_hosts) {
      if (!sptr_host)
        continue;

      /* bail out as soon as we find one parent host that is UP */
      if (sptr_host->get_current_state() == host::state_up) {
        is_host_present = true;
        /* set the current state */
        state = host::state_down;
        engine_logger(dbg_checks, most)
            << "At least one parent (" << key << ") is up, so host is DOWN.";
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "At least one parent ({}) is up, so host is DOWN.",
                            key);
        break;
      }
    }
    /* no parents were up, so this host is UNREACHABLE */
    if (!is_host_present) {
      state = host::state_unreachable;
      engine_logger(dbg_checks, most)
          << "No parents were up, so host is UNREACHABLE.";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "No parents were up, so host is UNREACHABLE.");
    }
  }

  return state;
}

std::list<hostgroup*> const& host::get_parent_groups() const {
  return _hostgroups;
}

std::list<hostgroup*>& host::get_parent_groups() {
  return _hostgroups;
}

/**
 *  This function returns a boolean telling if the master hosts of this one
 *  authorize it or forbide it to make its job (execution or notification).
 *
 * @param dependency_type execution / notification
 *
 * @return true if it is authorized.
 */
bool host::authorized_by_dependencies(dependency::types dependency_type) const {
  engine_logger(dbg_functions, basic) << "host::authorized_by_dependencies()";
  SPDLOG_LOGGER_TRACE(functions_logger, "host::authorized_by_dependencies()");

  bool soft_state_dependencies = pb_config.soft_state_dependencies();

  auto p(hostdependency::hostdependencies.equal_range(name()));
  for (hostdependency_mmap::const_iterator it{p.first}, end{p.second};
       it != end; ++it) {
    hostdependency* dep{it->second.get()};
    /* Only check dependencies of the desired type (notification or execution)
     */
    if (dep->get_dependency_type() != dependency_type)
      continue;

    /* Find the host we depend on */
    if (!dep->master_host_ptr)
      continue;

    /* Skip this dependency if it has a timeperiod and the current time is
     * not valid */
    time_t current_time{std::time(nullptr)};
    if (!dep->get_dependency_period().empty() &&
        !check_time_against_period(current_time, dep->dependency_period_ptr))
      return true;

    /* Get the status to use (use last hard state if it's currently in a soft
     * state) */
    host_state state =
        (dep->master_host_ptr->get_state_type() == notifier::soft &&
         !soft_state_dependencies)
            ? dep->master_host_ptr->get_last_hard_state()
            : dep->master_host_ptr->get_current_state();

    /* Is the host we depend on in state that fails the dependency tests? */
    if (dep->get_fail_on(state))
      return false;

    if (state == host::state_up && !dep->master_host_ptr->has_been_checked() &&
        dep->get_fail_on_pending())
      return false;

    /* Immediate dependencies ok at this point - check parent dependencies if
     * necessary */
    if (dep->get_inherits_parent()) {
      if (!dep->master_host_ptr->authorized_by_dependencies(dependency_type))
        return false;
    }
  }
  return true;
}

/* check freshness of host results */
void host::check_result_freshness() {
  time_t current_time = 0L;

  bool check_host_freshness;
  check_host_freshness = pb_config.check_host_freshness();

  engine_logger(dbg_functions, basic) << "check_host_result_freshness()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_host_result_freshness()");
  engine_logger(dbg_checks, most)
      << "Attempting to check the freshness of host check results...";
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "Attempting to check the freshness of host check results...");

  /* bail out if we're not supposed to be checking freshness */
  if (!check_host_freshness) {
    engine_logger(dbg_checks, most) << "Host freshness checking is disabled.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Host freshness checking is disabled.");
    return;
  }

  /* get the current time */
  time(&current_time);

  /* check all hosts... */
  for (host_map::iterator it{host::hosts.begin()}, end{host::hosts.end()};
       it != end; ++it) {
    /* skip hosts we shouldn't be checking for freshness */
    if (!it->second->check_freshness_enabled())
      continue;

    /* skip hosts that have both active and passive checks disabled */
    if (!it->second->active_checks_enabled() &&
        !it->second->passive_checks_enabled())
      continue;

    /* skip hosts that are currently executing (problems here will be caught by
     * orphaned host check) */
    if (it->second->get_is_executing())
      continue;

    /* skip hosts that are already being freshened */
    if (it->second->get_is_being_freshened())
      continue;

    // See if the time is right...
    {
      timezone_locker lock(it->second->get_timezone());
      if (!check_time_against_period(current_time,
                                     it->second->check_period_ptr))
        continue;
    }

    /* the results for the last check of this host are stale */
    if (!it->second->is_result_fresh(current_time, true)) {
      /* set the freshen flag */
      it->second->set_is_being_freshened(true);

      /* schedule an immediate forced check of the host */
      it->second->schedule_check(
          current_time,
          CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK);
    }
  }
}

/**
 *  Adjusts current host check attempt before a new check is performed.
 *
 * @param is_active Boolean telling if the check is active or not.
 *
 */
void host::adjust_check_attempt(bool is_active) {
  engine_logger(dbg_functions, basic) << "adjust_host_check_attempt_3x()";
  SPDLOG_LOGGER_TRACE(functions_logger, "adjust_host_check_attempt_3x()");

  engine_logger(dbg_checks, most)
      << "Adjusting check attempt number for host '" << name()
      << "': current attempt=" << get_current_attempt() << "/"
      << max_check_attempts()
      << ", state=" << static_cast<uint32_t>(_current_state)
      << ", state type=" << get_state_type();
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "Adjusting check attempt number for host '{}': current attempt= {}/{}, "
      "state= {}, state type= {}",
      name(), get_current_attempt(), max_check_attempts(),
      static_cast<uint32_t>(_current_state),
      static_cast<uint32_t>(get_state_type()));
  /* if host is in a hard state, reset current attempt number */
  if (get_state_type() == notifier::hard)
    set_current_attempt(1);

  /* if host is in a soft UP state, reset current attempt number (active checks
   * only) */
  else if (is_active && get_state_type() == notifier::soft &&
           _current_state == host::state_up)
    set_current_attempt(1);

  /* increment current attempt number */
  else if (get_current_attempt() < max_check_attempts())
    set_current_attempt(get_current_attempt() + 1);

  engine_logger(dbg_checks, most)
      << "New check attempt number = " << get_current_attempt();
  SPDLOG_LOGGER_DEBUG(checks_logger, "New check attempt number = {}",
                      get_current_attempt());
}

/* check for hosts that never returned from a check... */
void host::check_for_orphaned() {
  time_t current_time = 0L;
  time_t expected_time = 0L;

  engine_logger(dbg_functions, basic) << "check_for_orphaned_hosts()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_for_orphaned_hosts()");

  int32_t host_check_timeout;
  uint32_t check_reaper_interval;
  host_check_timeout = pb_config.host_check_timeout();
  check_reaper_interval = pb_config.check_reaper_interval();

  /* get the current time */
  time(&current_time);

  /* check all hosts... */
  for (host_map::iterator it{host::hosts.begin()}, end{host::hosts.end()};
       it != end; ++it) {
    /* skip hosts that don't have a set check interval (on-demand checks are
     * missed by the orphan logic) */
    if (it->second->get_next_check() == (time_t)0L)
      continue;

    /* skip hosts that are not currently executing */
    if (!it->second->get_is_executing())
      continue;

    /* determine the time at which the check results should have come in (allow
     * 10 minutes slack time) */
    expected_time =
        (time_t)(it->second->get_next_check() + it->second->get_latency() +
                 host_check_timeout + check_reaper_interval + 600);

    /* this host was supposed to have executed a while ago, but for some reason
     * the results haven't come back in... */
    if (expected_time < current_time) {
      /* log a warning */
      engine_logger(log_runtime_warning, basic)
          << "Warning: The check of host '" << it->second->name()
          << "' looks like it was orphaned (results never came back).  "
             "I'm scheduling an immediate check of the host...";
      SPDLOG_LOGGER_WARN(
          runtime_logger,
          "Warning: The check of host '{}' looks like it was orphaned (results "
          "never came back).  "
          "I'm scheduling an immediate check of the host...",
          it->second->name());

      engine_logger(dbg_checks, more)
          << "Host '" << it->second->name()
          << "' was orphaned, so we're scheduling an immediate check...";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Host '{}' was orphaned, so we're scheduling an immediate check...",
          it->second->name());

      /* decrement the number of running host checks */
      if (currently_running_host_checks > 0)
        currently_running_host_checks--;

      /* disable the executing flag */
      it->second->set_is_executing(false);

      /* schedule an immediate check of the host */
      it->second->schedule_check(current_time, CHECK_OPTION_ORPHAN_CHECK);
    }
  }
}

const std::string& host::get_current_state_as_string() const {
  return tab_host_states[get_current_state()].second;
}

bool host::get_notify_on_current_state() const {
  notification_flag type[]{up, down, unreachable};
  bool retval = get_notify_on(type[get_current_state()]);
  return retval;
}

bool host::is_in_downtime() const {
  return get_scheduled_downtime_depth() > 0;
}

/**
 *  This method resolves pointers involved in this host life. If a pointer
 *  cannot be resolved, an exception is thrown.
 *
 * @param w Warnings given by the method.
 * @param e Errors given by the method. An exception is thrown is at less an
 * error is rised.
 */
void host::resolve(uint32_t& w, uint32_t& e) {
  uint32_t warnings = 0;
  uint32_t errors = 0;

  try {
    notifier::resolve(warnings, errors);
  } catch (std::exception const& e) {
    engine_logger(log_verification_error, basic)
        << "Error: Host '" << name()
        << "' has problem in its notifier part: " << e.what();
    config_logger->error(
        "Error: Host '{}' has problem in its notifier part: {}", name(),
        e.what());
  }

  for (service_map::iterator it_svc{service::services.begin()},
       end_svc{service::services.end()};
       it_svc != end_svc; ++it_svc) {
    if (name() == it_svc->first.first)
      services.insert({it_svc->first, nullptr});
  }

  if (services.empty()) {
    engine_logger(log_verification_error, basic)
        << "Warning: Host '" << name()
        << "' has no services associated with it!";
    config_logger->warn(
        "Warning: Host '{}' has no services associated with it!", name());
    ++w;
  } else {
    for (service_map_unsafe::iterator it{services.begin()}, end{services.end()};
         it != end; ++it) {
      service_map::const_iterator found{service::services.find(it->first)};
      if (found == service::services.end() || !found->second) {
        engine_logger(log_verification_error, basic)
            << "Error: Host '" << name() << "' has a service '"
            << it->first.second << "' that does not exist!";
        config_logger->error(
            "Error: Host '{}' has a service '{}' that does not exist!", name(),
            it->first.second);
        ++errors;
      } else {
        it->second = found->second.get();
      }
    }
  }

  /* check all parent parent host */
  for (auto& [key, sptr_host] : parent_hosts) {
    host_map::const_iterator it_host{host::hosts.find(key)};
    if (it_host == host::hosts.end() || !it_host->second) {
      engine_logger(log_verification_error, basic) << "Error: '" << key
                                                   << "' is not a "
                                                      "valid parent for host '"
                                                   << name() << "'!";
      config_logger->error("Error: '{}' is not a valid parent for host '{}'!",
                           key, name());
      errors++;
    } else {
      sptr_host = it_host->second;
      it_host->second->add_child_host(this);  // add a reverse (child) link to
                                              // make searches faster later on
    }
  }

  /* check for illegal characters in host name */
  if (contains_illegal_object_chars(name().c_str())) {
    engine_logger(log_verification_error, basic)
        << "Error: The name of host '" << name()
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The name of host '{}' contains one or more illegal characters.",
        name());
    errors++;
  }

  // Check for sane recovery options.
  if (get_notifications_enabled() && get_notify_on(notifier::up) &&
      !get_notify_on(notifier::down) && !get_notify_on(notifier::unreachable)) {
    engine_logger(log_verification_error, basic)
        << "Warning: Recovery notification option in host '"
        << get_display_name()
        << "' definition doesn't make any sense - specify down and/or "
           "unreachable options as well";
    config_logger->warn(
        "Warning: Recovery notification option in host '{}' definition doesn't "
        "make any sense - specify down and/or "
        "unreachable options as well",
        get_display_name());
    warnings++;
  }

  w += warnings;
  e += errors;

  if (errors)
    throw engine_error() << "Cannot resolve host '" << name() << "'";
}

timeperiod* host::get_notification_timeperiod() const {
  /* if the service has no notification period, inherit one from the host */
  return get_notification_period_ptr();
}

/**
 * @brief update check command
 *
 * @param cmd
 */
void host::set_check_command_ptr(
    const std::shared_ptr<commands::command>& cmd) {
  std::shared_ptr<commands::command> old = get_check_command_ptr();
  if (cmd == old) {
    return;
  }

  if (old) {
    old->unregister_host_serv(name(), "");
  }
  notifier::set_check_command_ptr(cmd);
  if (cmd) {
    cmd->register_host_serv(name(), "");
  }
}

/**
 * @brief calculate final check command with macros replaced
 *
 * @return std::string
 */
std::string host::get_check_command_line(nagios_macros* macros) {
  grab_host_macros_r(macros, this);
  std::string tmp;
  get_raw_command_line_r(macros, get_check_command_ptr(),
                         check_command().c_str(), tmp, 0);
  return get_check_command_ptr()->process_cmd(macros);
}

/**
 * @brief when host switch to no up state and have to propagate state to
 * services, we push an UNKNOWN check result for each services
 *
 */
void host::_switch_all_services_to_unknown() {
  timeval tv;
  gettimeofday(&tv, nullptr);

  SPDLOG_LOGGER_DEBUG(runtime_logger,
                      "host {} is down => all service states switch to UNKNOWN",
                      name());

  std::string output = fmt::format("host {} is down", name());

  for (auto serv_iter = service::services_by_id.lower_bound({_id, 0});
       serv_iter != service::services_by_id.end() &&
       serv_iter->first.first == _id;
       ++serv_iter) {
    check_result::pointer result = std::make_shared<check_result>(
        service_check, serv_iter->second.get(), checkable::check_active,
        CHECK_OPTION_NONE, false, 0, tv, tv, false, true,
        service::state_unknown, output);
    checks::checker::instance().add_check_result_to_reap(result);
  }
}
