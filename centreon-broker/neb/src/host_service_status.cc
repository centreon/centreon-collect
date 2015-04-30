/*
** Copyright 2009-2013,2015 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
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
host_service_status::host_service_status()
  : active_checks_enabled(false),
    check_interval(0.0),
    check_type(0),
    current_check_attempt(0),
    current_state(4), // Pending
    enabled(true),
    execution_time(0.0),
    has_been_checked(false),
    host_id(0),
    is_flapping(false),
    last_check(0),
    last_hard_state(4), // Pending
    last_hard_state_change(0),
    last_state_change(0),
    last_update(0),
    latency(0.0),
    max_check_attempts(0),
    next_check(0),
    obsess_over(false),
    percent_state_change(0.0),
    retry_interval(0.0),
    should_be_scheduled(false),
    state_type(0) {}

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
host_service_status& host_service_status::operator=(host_service_status const& hss) {
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
  active_checks_enabled = hss.active_checks_enabled;
  check_command = hss.check_command;
  check_interval = hss.check_interval;
  check_period = hss.check_period;
  check_type = hss.check_type;
  current_check_attempt = hss.current_check_attempt;
  current_state = hss.current_state;
  enabled = hss.enabled;
  event_handler = hss.event_handler;
  execution_time = hss.execution_time;
  has_been_checked = hss.has_been_checked;
  host_id = hss.host_id;
  is_flapping = hss.is_flapping;
  last_check = hss.last_check;
  last_hard_state = hss.last_hard_state;
  last_hard_state_change = hss.last_hard_state_change;
  last_state_change = hss.last_state_change;
  last_update = hss.last_update;
  latency = hss.latency;
  max_check_attempts = hss.max_check_attempts;
  next_check = hss.next_check;
  obsess_over = hss.obsess_over;
  output = hss.output;
  percent_state_change = hss.percent_state_change;
  perf_data = hss.perf_data;
  retry_interval = hss.retry_interval;
  should_be_scheduled = hss.should_be_scheduled;
  state_type = hss.state_type;
  return ;
}
