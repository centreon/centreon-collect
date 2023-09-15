/*
 * Copyright 2014, 2021-2023 Centreon
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

#include "com/centreon/broker/bam/bool_service.hh"

#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 */
bool_service::bool_service(uint32_t host_id,
                           uint32_t service_id,
                           const std::shared_ptr<spdlog::logger>& logger)
    : bool_value(logger),
      _host_id(host_id),
      _service_id(service_id),
      _state_hard(0),
      _state_known(false),
      _in_downtime(false) {}

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
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  _logger = logger;
  SPDLOG_LOGGER_TRACE(_logger,
                      "bool_service: service update with neb::service_status");
  if (status && status->host_id == _host_id &&
      status->service_id == _service_id) {
    bool new_in_downtime = status->downtime_depth > 0;
    bool changed = _state_hard != status->last_hard_state || !_state_known ||
                   _in_downtime != new_in_downtime;
    if (changed) {
      _state_hard = status->last_hard_state;
      _state_known = true;
      _in_downtime = new_in_downtime;
      notify_parents_of_change(visitor);
    }
  }
}

/**
 *  Notify of service update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void bool_service::service_update(
    const std::shared_ptr<neb::pb_service>& svc,
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  _logger = logger;
  auto& o = svc->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "bool_service: service ({},{}) updated with "
                      "neb::pb_service hard state: {}, downtime: {}",
                      o.host_id(), o.service_id(), o.last_hard_state(),
                      o.scheduled_downtime_depth());
  if (o.host_id() == _host_id && o.service_id() == _service_id) {
    bool new_in_downtime = o.scheduled_downtime_depth() > 0;
    bool changed = _state_hard != o.last_hard_state() || !_state_known ||
                   _in_downtime != new_in_downtime;
    if (changed) {
      _state_hard = o.last_hard_state();
      _state_known = true;
      _in_downtime = new_in_downtime;
      log_v2::bam()->trace("bool_service: updated with state: {}", _state_hard);
      notify_parents_of_change(visitor);
    }
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
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  _logger = logger;
  auto& o = status->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "bool_service: service ({},{}) updated with "
                      "neb::pb_service_status hard state: {}, downtime: {}",
                      o.host_id(), o.service_id(), o.last_hard_state(),
                      o.scheduled_downtime_depth());
  if (o.host_id() == _host_id && o.service_id() == _service_id) {
    bool new_in_downtime = o.scheduled_downtime_depth() > 0;
    if (_state_hard != o.last_hard_state() || !_state_known ||
        _in_downtime != new_in_downtime) {
      _state_hard = o.last_hard_state();
      _state_known = true;
      _in_downtime = new_in_downtime;
      log_v2::bam()->trace("bool_service: updated with state: {}", _state_hard);
      notify_parents_of_change(visitor);
    }
  }
}

/**
 *  Get the hard value.
 *
 *  @return Hard value.
 */
double bool_service::value_hard() const {
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
  _logger->trace("BAM: bool_service::state_known: {}", _state_known);
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

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 */
void bool_service::update_from(computable* child [[maybe_unused]],
                               io::stream* visitor) {
  log_v2::bam()->trace("bool_service::update_from");
  notify_parents_of_change(visitor);
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_service::object_info() const {
  return fmt::format("BOOL Service ({}, {})\nknown: {}\nvalue: {}",
                     get_host_id(), get_service_id(),
                     state_known() ? "true" : "false", value_hard());
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void bool_service::dump(std::ofstream& output) const {
  dump_parents(output);
}
