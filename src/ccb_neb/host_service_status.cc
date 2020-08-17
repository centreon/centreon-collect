/*
** Copyright 2009-2013,2015 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/neb/host_service_status.hh"

using namespace com::centreon::broker::neb;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  @brief Default constructor.
 *
 *  Initialize members to 0, NULL or equivalent.
 */
host_service_status::host_service_status(uint32_t type)
    : status(type,
             false,
             false,
             false),
      acknowledged(false),
      acknowledgement_type(0),
      active_checks_enabled(false),
      check_command(""),
      check_interval(0.0),
      check_period(""),
      check_type(0),
      current_check_attempt(0),
      current_state(4),  // Pending
      downtime_depth(0),
      event_handler(""),
      enabled(true),
      execution_time(0.0),
      has_been_checked(false),
      host_id(0),
      is_flapping(false),
      last_check(0),
      last_hard_state(4),  // Pending
      last_hard_state_change(0),
      last_notification(0),
      last_state_change(0),
      last_update(0),
      latency(0.0),
      max_check_attempts(0),
      next_check(0),
      next_notification(0),
      no_more_notifications(false),
      notification_number(0),
      obsess_over(false),
      output(""),
      passive_checks_enabled(false),
      percent_state_change(0.0),
      perf_data(""),
      retry_interval(0.0),
      should_be_scheduled(false),
      state_type(0) {}

host_service_status::host_service_status(uint32_t type,
                                         bool acknowledged,
                                         short acknowledgement_type,
                                         bool active_checks_enabled,
                                         std::string check_command,
                                         double check_interval,
                                         std::string check_period,
                                         short check_type,
                                         short current_check_attempt,
                                         short current_state,
                                         short downtime_depth,
                                         std::string event_handler,
                                         bool event_handler_enabled,
                                         double execution_time,
                                         bool flap_detection_enabled,
                                         bool has_been_checked,
                                         uint32_t host_id,
                                         bool is_flapping,
                                         timestamp last_check,
                                         short last_hard_state,
                                         timestamp last_hard_state_change,
                                         timestamp last_notification,
                                         timestamp last_state_change,
                                         timestamp last_update,
                                         double latency,
                                         short max_check_attempts,
                                         timestamp next_check,
                                         timestamp next_notification,
                                         bool no_more_notifications,
                                         short notification_number,
                                         bool obsess_over,
                                         std::string output,
                                         bool passive_checks_enabled,
                                         double percent_state_change,
                                         std::string perf_data,
                                         double retry_interval,
                                         bool should_be_scheduled,
                                         short state_type)
    : status(type,
             event_handler_enabled,
             flap_detection_enabled,
             notifications_enabled),
      acknowledged(acknowledged),
      acknowledgement_type(acknowledgement_type),
      active_checks_enabled(active_checks_enabled),
      check_command(check_command),
      check_interval(check_interval),
      check_period(check_period),
      check_type(check_type),
      current_check_attempt(current_check_attempt),
      current_state(current_state),
      downtime_depth(downtime_depth),
      enabled(true),
      event_handler(event_handler),
      execution_time(execution_time),
      has_been_checked(has_been_checked),
      host_id(host_id),
      is_flapping(is_flapping),
      last_check(last_check),
      last_hard_state(last_hard_state),
      last_hard_state_change(last_hard_state_change),
      last_notification(last_notification),
      last_state_change(last_state_change),
      last_update(last_update),
      latency(latency),
      max_check_attempts(max_check_attempts),
      next_check(next_check),
      next_notification(next_notification),
      no_more_notifications(no_more_notifications),
      notification_number(notification_number),
      obsess_over(obsess_over),
      output(output),
      passive_checks_enabled(passive_checks_enabled),
      percent_state_change(percent_state_change),
      perf_data(perf_data),
      retry_interval(retry_interval),
      should_be_scheduled(should_be_scheduled),
      state_type(state_type) {}

/**
 *  @brief Copy constructor.
 *
 *  Copy internal data of the given object to the current instance.
 *
 *  @param[in] hss Object to copy.
 */
host_service_status::host_service_status(host_service_status const& hss)
    : status(hss) {
  _internal_copy(hss);
}

/**
 *  Destructor.
 */
host_service_status::~host_service_status() {}

/**
 *  @brief Assignment operator.
 *
 *  Copy internal data of the given object to the current instance.
 *
 *  @param[in] hss Object to copy.
 *
 *  @return This object.
 */
host_service_status& host_service_status::operator=(
    host_service_status const& hss) {
  status::operator=(hss);
  _internal_copy(hss);
  return (*this);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  @brief Copy internal data of the given object to the current
 *         instance.
 *
 *  Make a copy of all internal members of host_service_status to the
 *  current instance.
 *
 *  @param[in] hss Object to copy.
 */
void host_service_status::_internal_copy(host_service_status const& hss) {
  acknowledged = hss.acknowledged;
  acknowledgement_type = hss.acknowledgement_type;
  active_checks_enabled = hss.active_checks_enabled;
  check_command = hss.check_command;
  check_interval = hss.check_interval;
  check_period = hss.check_period;
  check_type = hss.check_type;
  current_check_attempt = hss.current_check_attempt;
  current_state = hss.current_state;
  downtime_depth = hss.downtime_depth;
  enabled = hss.enabled;
  event_handler = hss.event_handler;
  execution_time = hss.execution_time;
  has_been_checked = hss.has_been_checked;
  host_id = hss.host_id;
  is_flapping = hss.is_flapping;
  last_check = hss.last_check;
  last_hard_state = hss.last_hard_state;
  last_hard_state_change = hss.last_hard_state_change;
  last_notification = hss.last_notification;
  last_state_change = hss.last_state_change;
  last_update = hss.last_update;
  latency = hss.latency;
  max_check_attempts = hss.max_check_attempts;
  next_check = hss.next_check;
  next_notification = hss.next_notification;
  no_more_notifications = hss.no_more_notifications;
  notification_number = hss.notification_number;
  obsess_over = hss.obsess_over;
  output = hss.output;
  passive_checks_enabled = hss.passive_checks_enabled;
  percent_state_change = hss.percent_state_change;
  perf_data = hss.perf_data;
  retry_interval = hss.retry_interval;
  should_be_scheduled = hss.should_be_scheduled;
  state_type = hss.state_type;
  return;
}
