/*
** Copyright 2014, 2021-2022 Centreon
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

#include "com/centreon/broker/bam/bool_service.hh"
#include <cassert>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 */
bool_service::bool_service(uint32_t host_id, uint32_t service_id)
    : _host_id(host_id),
      _service_id(service_id),
      _state_hard(0),
      _state_known(false),
      _in_downtime(false) {}

/**
 *  @brief Notification of child update.
 *
 *  This method is unused by bool_service because it has no computable
 *  child that could influence its value.
 *
 *  @param[in]  child   Unused.
 *  @param[out] visitor Unused.
 *
 *  @return             True.
 */
bool bool_service::child_has_update(computable* child, io::stream* visitor) {
  (void)child;
  (void)visitor;
  return true;
}

/**
 *  Get the host ID.
 *
 *  @return Host ID.
 */
uint32_t bool_service::get_host_id() const {
  return _host_id;
}

/**
 *  Get the service ID.
 *
 *  @return Service ID.
 */
uint32_t bool_service::get_service_id() const {
  return _service_id;
}

/**
 *  Notify of service update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void bool_service::service_update(
    std::shared_ptr<neb::service_status> const& status,
    io::stream* visitor) {
  log_v2::bam()->trace("bool_service: service update with neb::service_status");
  if (status && status->host_id == _host_id &&
      status->service_id == _service_id) {
    _state_hard = status->last_hard_state;
    _state_known = true;
    _in_downtime = (status->downtime_depth > 0);
    propagate_update(visitor);
  }
}

/**
 *  Notify of service update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void bool_service::service_update(const std::shared_ptr<neb::pb_service>& svc,
                                  io::stream* visitor) {
  auto& o = svc->obj();
  SPDLOG_LOGGER_TRACE(log_v2::bam(),
                      "bool_service: service ({},{}) updated with "
                      "neb::pb_service hard state: {}, downtime: {}",
                      o.host_id(), o.service_id(), o.last_hard_state(),
                      o.scheduled_downtime_depth());
  if (o.host_id() == _host_id && o.service_id() == _service_id) {
    _state_hard = o.last_hard_state();
    _state_known = true;
    _in_downtime = o.scheduled_downtime_depth() > 0;
    log_v2::bam()->trace("bool_service: updated with state: {}", _state_hard);
    propagate_update(visitor);
  }
}

/**
 *  Notify of service update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void bool_service::service_update(
    const std::shared_ptr<neb::pb_service_status>& status,
    io::stream* visitor) {
  auto& o = status->obj();
  log_v2::bam()->trace(
                      "bool_service: service ({},{}) updated with "
                      "neb::pb_service_status hard state: {}, downtime: {}",
                      o.host_id(), o.service_id(), o.last_hard_state(),
                      o.scheduled_downtime_depth());
  if (o.host_id() == _host_id && o.service_id() == _service_id) {
    _state_hard = o.last_hard_state();
    _state_known = true;
    _in_downtime = o.scheduled_downtime_depth() > 0;
    log_v2::bam()->trace("bool_service: updated with state: {}", _state_hard);
    propagate_update(visitor);
  }
}

/**
 *  Get the hard value.
 *
 *  @return Hard value.
 */
double bool_service::value_hard() {
  return _state_hard;
}

/**
 * @brief Get the value as a boolean
 *
 * @return True or false.
 */
bool bool_service::boolean_value() const {
  return _state_hard;
}

/**
 *  Get if the state is known, i.e has been computed at least once.
 *
 *  @return  True if the state is known.
 */
bool bool_service::state_known() const {
  log_v2::bam()->trace("BAM: bool_service::state_known: {}", _state_known);
  return _state_known;
}

/**
 *  Get is this boolean service is in downtime.
 *
 *  @return  True if in downtime.
 */
bool bool_service::in_downtime() const {
  return _in_downtime;
}
