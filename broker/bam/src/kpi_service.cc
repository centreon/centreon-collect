/**
 * Copyright 2014-2015, 2021-2023 Centreon
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

#include "com/centreon/broker/bam/kpi_service.hh"
#include "com/centreon/broker/bam/service_state.hh"

#include <cassert>

#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

using log_v2 = com::centreon::common::log_v2::log_v2;

static constexpr bool time_is_undefined(uint64_t t) {
  return t == 0 || t == static_cast<uint64_t>(-1);
}

/**
 *  Default constructor.
 */
kpi_service::kpi_service(uint32_t kpi_id,
                         uint32_t ba_id,
                         uint32_t host_id,
                         uint32_t service_id,
                         const std::string& host_serv,
                         const std::shared_ptr<spdlog::logger>& logger)
    : kpi(kpi_id, ba_id, host_serv, logger),
      _host_id(host_id),
      _service_id(service_id),
      _acknowledged(false),
      _downtimed(false),
      _impacts{0.0},
      _last_check(0),
      _state_hard{state_ok},
      _state_soft{state_ok},
      _state_type(0) {
  assert(_host_id);
}

/**
 *  Get host ID.
 *
 *  @return Host ID.
 */
uint32_t kpi_service::get_host_id() const {
  return _host_id;
}

/**
 *  Get the impact applied when service is CRITICAL.
 *
 *  @return The impact applied when service is CRITICAL.
 */
double kpi_service::get_impact_critical() const {
  return _impacts[2];
}

/**
 *  Get the impact applied when service is UNKNOWN.
 *
 *  @return The impact applied when service is UNKNOWN.
 */
double kpi_service::get_impact_unknown() const {
  return _impacts[3];
}

/**
 *  Get the impact applied when service is WARNING.
 *
 *  @return The impact applied when service is WARNING.
 */
double kpi_service::get_impact_warning() const {
  return _impacts[1];
}

/**
 *  Get the service ID.
 *
 *  @return Service ID.
 */
uint32_t kpi_service::get_service_id() const {
  return _service_id;
}

/**
 *  Get the hard state of the service.
 *
 *  @return Hard state of the service.
 */
state kpi_service::get_state_hard() const {
  return _state_hard;
}

/**
 *  Get the soft state of the service.
 *
 *  @return Soft state of the service.
 */
state kpi_service::get_state_soft() const {
  return _state_soft;
}

/**
 *  Get current state type.
 *
 *  @return State type.
 */
short kpi_service::get_state_type() const {
  return _state_type;
}

/**
 *  Compute impact implied by the hard service state.
 *
 *  @param[out] impact Impacts implied by the hard service state.
 */
void kpi_service::impact_hard(impact_values& impact) {
  _fill_impact(impact, _state_hard);
}

/**
 *  Compute impact implied by the soft service state.
 *
 *  @param[out] impact Impacts implied by the soft service state.
 */
void kpi_service::impact_soft(impact_values& impact) {
  _fill_impact(impact, _state_soft);
}

/**
 *  Check if service is in downtime.
 *
 *  @return True if the service is in downtime.
 */
bool kpi_service::in_downtime() const {
  return _downtimed;
}

/**
 *  Check if service is acknowledged.
 *
 *  @return True if the service is acknowledged.
 */
bool kpi_service::is_acknowledged() const {
  return _acknowledged;
}

void kpi_service::service_update(const service_state& s) {
  // Log message.
  _logger->debug("BAM: Service KPI {} is restored from persistent cache", _id);

  // Update information.
  if (!time_is_undefined(s.last_check)) {
    _last_check = s.last_check;
    _logger->trace(
        "service kpi {} last check updated with status last check {}", _id,
        s.last_check);
  }
  bool changed = _state_hard != static_cast<state>(s.last_hard_state) ||
                 _state_soft != static_cast<state>(s.current_state) ||
                 _state_type != s.state_type || _acknowledged != s.acknowledged;

  _state_hard = static_cast<state>(s.last_hard_state);
  _state_soft = static_cast<state>(s.current_state);
  _state_type = s.state_type;
  _acknowledged = s.acknowledged;

  // Propagate change.
  if (changed)
    notify_parents_of_change(nullptr);
}

/**
 *  Service got updated !
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(
    const std::shared_ptr<neb::service_status>& status,
    io::stream* visitor) {
  if (status && status->host_id == _host_id &&
      status->service_id == _service_id) {
    // Log message.
    _logger->debug(
        "BAM: KPI {} is getting notified of service ({}, {}) update (state: "
        "{} hard state: {})",
        _id, _host_id, _service_id, status->current_state,
        status->last_hard_state);

    // Update information.
    if (status->last_check.is_null()) {
      if (_last_check.is_null()) {
        _last_check = status->last_update;
        _logger->trace(
            "service kpi {} last check updated with status last update {}", _id,
            static_cast<time_t>(status->last_update));
      }
    } else {
      _last_check = status->last_check;
      _logger->trace(
          "service kpi {} last check updated with status last check {}", _id,
          static_cast<time_t>(status->last_check));
    }
    bool changed = _state_hard != static_cast<state>(status->last_hard_state) ||
                   _state_soft != static_cast<state>(status->current_state) ||
                   _state_type != status->state_type;

    _output = status->output;
    _perfdata = status->perf_data;
    _state_hard = static_cast<state>(status->last_hard_state);
    _state_soft = static_cast<state>(status->current_state);
    _state_type = status->state_type;

    // Generate status event.
    visit(visitor);

    // Propagate change.
    if (changed)
      notify_parents_of_change(visitor);
  }
}

/**
 *  Service got updated !
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(const std::shared_ptr<neb::pb_service>& status,
                                 io::stream* visitor) {
  if (status && status->obj().host_id() == _host_id &&
      status->obj().service_id() == _service_id) {
    auto& o = status->obj();
    // Log message.
    _logger->debug(
        "BAM: KPI {} is getting notified of service ({}, {}) update (state: "
        "{})",
        _id, _host_id, _service_id, static_cast<uint32_t>(o.state()));

    // Update information.
    if (o.last_check() == 0 || o.last_check() == -1) {
      if (_last_check.is_null()) {
        _last_check = std::time(nullptr);
        _logger->trace(
            "service kpi {} last check updated with status last update {}", _id,
            static_cast<time_t>(_last_check));
      }
    } else {
      _last_check = o.last_check();
      _logger->trace(
          "service kpi {} last check updated with status last check {}", _id,
          o.last_check());
    }
    bool changed = _state_hard != static_cast<state>(o.last_hard_state()) ||
                   _state_soft != static_cast<state>(o.state()) ||
                   _state_type != o.state_type();
    _output = o.output();
    _perfdata = o.perfdata();
    _state_hard = static_cast<state>(o.last_hard_state());
    _state_soft = static_cast<state>(o.state());
    _state_type = o.state_type();

    // Generate status event.
    visit(visitor);

    if (changed)
      notify_parents_of_change(visitor);
  }
}

/**
 *  Service got updated !
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(
    const std::shared_ptr<neb::pb_service_status>& status,
    io::stream* visitor) {
  if (status && status->obj().host_id() == _host_id &&
      status->obj().service_id() == _service_id) {
    auto& o = status->obj();
    // Log message.
    _logger->debug(
        "BAM: KPI {} is getting notified of service ({}, {}) update (state: "
        "{} hard state: {})",
        _id, _host_id, _service_id, static_cast<uint32_t>(o.state()),
        static_cast<uint32_t>(o.state_type()));

    // Update information.
    if (o.last_check() == 0 || o.last_check() == -1) {
      if (_last_check.is_null()) {
        _last_check = std::time(nullptr);
        _logger->trace(
            "service kpi {} last check updated with status last update {}", _id,
            static_cast<time_t>(_last_check));
      }
    } else {
      _last_check = o.last_check();
      _logger->trace(
          "service kpi {} last check updated with status last check {}", _id,
          o.last_check());
    }

    bool changed = _state_hard != static_cast<state>(o.last_hard_state()) ||
                   _state_soft != static_cast<state>(o.state()) ||
                   _state_type != o.state_type();

    _output = o.output();
    _perfdata = o.perfdata();
    _state_hard = static_cast<state>(o.last_hard_state());
    _state_soft = static_cast<state>(o.state());
    _state_type = o.state_type();

    // Generate status event.
    visit(visitor);

    // Propagate change.
    if (changed)
      notify_parents_of_change(visitor);
  }
}

/**
 *  Service got a protobuf acknowledgement.
 *
 *  @param[in]  ack      Acknowledgement.
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(
    const std::shared_ptr<neb::pb_acknowledgement>& ack,
    io::stream* visitor) {
  // Log message.
  _logger->debug(
      "BAM: KPI {} is getting a pb acknowledgement event for service ({}, {}) "
      "entry_time {} ; deletion_time {}",
      _id, _host_id, _service_id, ack->obj().entry_time(),
      ack->obj().deletion_time());

  // Update information.
  bool changed = false;
  if (_acknowledged != time_is_undefined(ack->obj().deletion_time())) {
    changed = true;
    _acknowledged = time_is_undefined(ack->obj().deletion_time());
  }
  // Generate status event.
  visit(visitor);

  if (changed)
    notify_parents_of_change(visitor);
}

/**
 *  Service got an acknowledgement.
 *
 *  @param[in]  ack      Acknowledgement.
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(
    const std::shared_ptr<neb::acknowledgement>& ack,
    io::stream* visitor) {
  // Log message.
  _logger->debug(
      "BAM: KPI {} is getting an acknowledgement event for service ({}, {}) "
      "entry_time {} ; deletion_time {}",
      _id, _host_id, _service_id, ack->entry_time, ack->deletion_time);

  bool changed = false;
  // Update information.
  if (_acknowledged != ack->deletion_time.is_null()) {
    _acknowledged = ack->deletion_time.is_null();
    changed = true;
  }

  // Generate status event.
  visit(visitor);

  if (changed)
    notify_parents_of_change(visitor);
}

/**
 *  Service got a downtime.
 *
 *  @param[in]  dt
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(const std::shared_ptr<neb::downtime>& dt,
                                 io::stream* visitor) {
  _logger->info(
      "kpi_service:service_update on downtime {}: was started {} ; actual end "
      "time {}",
      dt->internal_id, dt->was_started, dt->actual_end_time.get_time_t());
  // Update information.
  bool downtimed = dt->was_started && dt->actual_end_time.is_null();
  bool changed = false;

  if (_downtime_ids.contains(dt->internal_id) && dt->deletion_time.is_null()) {
    _logger->trace("Downtime {} already handled in this kpi service",
                   dt->internal_id);
    return;
  }

  _logger->info(
      "kpi_service:service_update on downtime {}: was started {} ; actual end "
      "time {} ; downtimed {}",
      dt->internal_id, dt->was_started, dt->actual_end_time.get_time_t(),
      downtimed);
  if (downtimed) {
    _logger->trace("adding in kpi service the impacting downtime {}",
                   dt->internal_id);
    _downtime_ids.insert(dt->internal_id);
    if (!_downtimed) {
      _downtimed = true;
      changed = true;
    }
  } else {
    _logger->trace("removing from kpi service the impacting downtime {}",
                   dt->internal_id);
    _downtime_ids.erase(dt->internal_id);
    bool new_downtimed = !_downtime_ids.empty();
    if (new_downtimed != _downtimed) {
      _downtimed = new_downtimed;
      changed = true;
    }
  }

  if (!_event || _event->in_downtime() != _downtimed) {
    _last_check = _downtimed ? dt->actual_start_time : dt->actual_end_time;
    _logger->trace("kpi service {} update, last check set to {}", _id,
                   _last_check);
  }

  // Log message.
  _logger->debug(
      "BAM: KPI {} is getting notified of a downtime ({}) on its service ({}, "
      "{}), in downtime: {} at {}",
      _id, dt->internal_id, _host_id, _service_id, _downtimed, _last_check);

  // Generate status event.
  visit(visitor);

  if (changed)
    notify_parents_of_change(visitor);
}

/**
 *  Service got a downtime (protobuf).
 *
 *  @param[in]  dt
 *  @param[out] visitor  Object that will receive events.
 */
void kpi_service::service_update(const std::shared_ptr<neb::pb_downtime>& dt,
                                 io::stream* visitor) {
  auto& downtime = dt->obj();
  // Update information.
  bool downtimed =
      downtime.started() && time_is_undefined(downtime.actual_end_time());
  bool changed = false;
  if (!_downtimed && downtimed) {
    _downtimed = true;
    changed = true;
  }

  if (_downtime_ids.contains(downtime.id()) &&
      time_is_undefined(downtime.deletion_time())) {
    _logger->trace("Downtime {} already handled in this kpi service",
                   downtime.id());
    return;
  }

  if (downtimed) {
    _logger->trace("adding in kpi service the impacting downtime {}",
                   downtime.id());
    _downtime_ids.insert(downtime.id());
  } else {
    _logger->trace("removing from kpi service the impacting downtime {}",
                   downtime.id());
    _downtime_ids.erase(downtime.id());
    bool new_downtimed = !_downtime_ids.empty();
    if (new_downtimed != _downtimed) {
      _downtimed = !_downtime_ids.empty();
      changed = true;
    }
  }

  if (!_event || _event->in_downtime() != _downtimed) {
    _last_check =
        _downtimed ? downtime.actual_start_time() : downtime.actual_end_time();
    _logger->trace("kpi service {} update, last check set to {}", _id,
                   _last_check);
  }

  // Log message.
  _logger->debug(
      "BAM: KPI {} is getting notified of a downtime ({}) on its service ({}, "
      "{}), in downtime: {} at {}",
      _id, downtime.id(), _host_id, _service_id, _downtimed, _last_check);

  // Generate status event.
  visit(visitor);

  if (changed)
    notify_parents_of_change(visitor);
}

/**
 *  Set service as acknowledged.
 *
 *  @param[in] acknowledged Acknowledged flag.
 */
void kpi_service::set_acknowledged(bool acknowledged) {
  _acknowledged = acknowledged;
}

/**
 *  Set service as downtimed.
 *
 *  @param[in] downtimed Downtimed flag.
 */
void kpi_service::set_downtimed(bool downtimed) {
  _downtimed = downtimed;
}

/**
 *  Set impact implied when service is CRITICAL.
 *
 *  @param[in] impact Impact if service is CRITICAL.
 */
void kpi_service::set_impact_critical(double impact) {
  _impacts[2] = impact;
}

/**
 *  Set impact implied when service is UNKNOWN.
 *
 *  @param[in] impact Impact if service is UNKNOWN.
 */
void kpi_service::set_impact_unknown(double impact) {
  _impacts[3] = impact;
}

/**
 *  Set impact implied when service is WARNING.
 *
 *  @param[in] impact Impact if service is WARNING.
 */
void kpi_service::set_impact_warning(double impact) {
  _impacts[1] = impact;
}

/**
 *  Set hard state.
 *
 *  @param[in] state Service hard state.
 */
void kpi_service::set_state_hard(state state) {
  _state_hard = state;
}

/**
 *  Set soft state.
 *
 *  @param[in] state Service soft state.
 */
void kpi_service::set_state_soft(state state) {
  _state_soft = state;
}

/**
 *  Set state type.
 *
 *  @param[in] type Current state type.
 */
void kpi_service::set_state_type(short type) {
  _state_type = type;
}

/**
 *  Visit service KPI.
 *
 *  @param[out] visitor  Object that will receive status.
 */
void kpi_service::visit(io::stream* visitor) {
  if (visitor) {
    // Commit the initial events saved in the cache.
    commit_initial_events(visitor);

    // Get information.
    impact_values hard_values;
    impact_values soft_values;
    impact_hard(hard_values);
    impact_soft(soft_values);

    // Generate BI events.
    {
      // If no event was cached, create one.
      if (!_event) {
        if (!_last_check.is_null()) {
          _logger->trace("BAM: kpi_service::visit no event => creation of one");
          _open_new_event(visitor, hard_values);
        }
      }
      // If state changed, close event and open a new one.
      else if (_last_check.get_time_t() >=
                   static_cast<time_t>(_event->start_time()) &&
               (_downtimed != _event->in_downtime() ||
                static_cast<State>(_state_hard) != _event->status())) {
        _logger->trace(
            "BAM: kpi_service::visit event needs update downtime: {}, state: "
            "{}",
            _downtimed != _event->in_downtime(),
            static_cast<State>(_state_hard) != _event->status());
        _event->set_end_time(_last_check);
        visitor->write(std::make_shared<pb_kpi_event>(std::move(*_event)));
        _open_new_event(visitor, hard_values);
      }
    }

    // Generate status event.
    {
      _logger->debug("Generating kpi status {} for service", _id);
      auto status{std::make_shared<pb_kpi_status>()};
      KpiStatus& ev(status->mut_obj());
      ev.set_kpi_id(_id);
      ev.set_in_downtime(in_downtime());
      ev.set_level_acknowledgement_hard(hard_values.get_acknowledgement());
      ev.set_level_acknowledgement_soft(soft_values.get_acknowledgement());
      ev.set_level_downtime_hard(hard_values.get_downtime());
      ev.set_level_downtime_soft(soft_values.get_downtime());
      ev.set_level_nominal_hard(hard_values.get_nominal());
      ev.set_level_nominal_soft(soft_values.get_nominal());
      ev.set_state_hard(State(_state_hard));
      ev.set_state_soft(State(_state_soft));
      ev.set_last_state_change(get_last_state_change());
      ev.set_last_impact(_downtimed ? hard_values.get_downtime()
                                    : hard_values.get_nominal());
      _logger->trace(
          "Writing kpi status {}: in downtime: {} ; last state changed: {} ; "
          "state: {}",
          _id, ev.in_downtime(), static_cast<time_t>(ev.last_state_change()),
          static_cast<uint32_t>(ev.state_hard()));
      visitor->write(status);
    }
  }
}

/**
 *  Fill impact values from a state.
 *
 *  @param[out] impact Impacts of the state.
 *  @param[in]  state  Service state.
 */
void kpi_service::_fill_impact(impact_values& impact, state state) {
  if (state < 0 || static_cast<size_t>(state) >= _impacts.size())
    throw msg_fmt("BAM: could not get impact introduced by state {}",
                  static_cast<uint32_t>(state));
  double nominal{_impacts[state]};
  impact.set_nominal(nominal);
  impact.set_acknowledgement(_acknowledged ? 1.0 : 0.0);
  impact.set_downtime(_downtimed ? nominal : 0.0);
  impact.set_state(state);
}

/**
 *  Open a new event for this KPI.
 *
 *  @param[out] visitor  Visitor that will receive events.
 *  @param[in]  impacts  Impact values.
 */
void kpi_service::_open_new_event(io::stream* visitor,
                                  impact_values const& impacts) {
  _event_init();
  _event->set_start_time(_last_check.get_time_t());
  _event->set_end_time(-1);
  _event->set_impact_level(_downtimed ? impacts.get_downtime()
                                      : impacts.get_nominal());
  _event->set_in_downtime(_downtimed);
  _event->set_output(_output);
  _event->set_perfdata(_perfdata);
  _event->set_status(com::centreon::broker::State(_state_hard));
  _logger->trace("BAM: New BI event for kpi {}, ba {}, in downtime {} since {}",
                 _id, _ba_id, _downtimed, _last_check);
  if (visitor) {
    /* We make a real copy because the writing into the DB is asynchronous and
     * so the event could have changed... */
    visitor->write(std::make_shared<pb_kpi_event>(*_event));
  }
}

/**
 *  Set the initial event.
 *
 *  @param[in] e  the event.
 */
void kpi_service::set_initial_event(const KpiEvent& e) {
  kpi::set_initial_event(e);
  _logger->trace(
      "BAM: set initial event from kpi event {} (start time {} ; in downtime "
      "{})",
      _event->kpi_id(), _event->start_time(), _event->in_downtime());
  _last_check = _event->start_time();
  _downtimed = _event->in_downtime();
}

/**
 *  Is this KPI in an ok state?
 *
 *  @return  True if this KPI is in an ok state.
 */
bool kpi_service::ok_state() const {
  return _state_hard == 0;
}

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 */
void kpi_service::update_from(computable* child [[maybe_unused]],
                              io::stream* visitor) {
  _logger->trace("kpi_service::update_from");
  notify_parents_of_change(visitor);
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string kpi_service::object_info() const {
  return fmt::format("KPI {} with service ({}, {})\nstate: {}\ndowntime: {}",
                     get_id(), get_host_id(), get_service_id(),
                     static_cast<uint32_t>(get_state_hard()), _downtimed);
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void kpi_service::dump(std::ofstream& output) const {
  dump_parents(output);
}
