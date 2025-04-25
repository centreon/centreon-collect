/**
 * Copyright 2014-2016, 2021-2024 Centreon
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

#include "com/centreon/broker/bam/ba.hh"

#include <fmt/format.h>

#include <cassert>

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/kpi.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

double ba::_normalize(double d) {
  if (d > 100.0)
    d = 100.0;
  else if (d < 0.0)
    d = 0.0;
  return d;
};

static bool time_is_undefined(uint64_t t) {
  return t == 0 || t == static_cast<uint64_t>(-1);
}

/**
 *  Constructor.
 *
 *  @param[in] id the id of this ba.
 *  @param[in] host_id the id of the associated host.
 *  @param[in] service_id the id of the associated service.
 *  @param[in] source Source, i.e. the type of service.
 *  @param[in] generate_virtual_status  Whether or not the BA object
 *                                      should generate statuses of
 *                                      virtual hosts and services.
 *  @param[in] logger The logger used in this ba.
 */
ba::ba(uint32_t id,
       uint32_t host_id,
       uint32_t service_id,
       configuration::ba::state_source source,
       bool generate_virtual_status,
       const std::shared_ptr<spdlog::logger>& logger)
    : computable(logger),
      _id(id),
      _state_source(source),
      _host_id(host_id),
      _service_id(service_id),
      _generate_virtual_status(generate_virtual_status),
      _in_downtime(false),
      _last_kpi_update(0) {
  assert(_host_id);
}

/**
 *  Add impact.
 *
 *  @param[in] impact KPI that will impact BA.
 */
void ba::add_impact(std::shared_ptr<kpi> const& impact) {
  auto it = _impacts.find(impact.get());
  if (it == _impacts.end()) {
    impact_info& ii = _impacts[impact.get()];
    ii.kpi_ptr = impact;
    impact->impact_hard(ii.hard_impact);
    impact->impact_soft(ii.soft_impact);
    ii.in_downtime = impact->in_downtime();
    _apply_impact(impact.get(), ii);
    timestamp last_state_change(impact->get_last_state_change());
    if (!last_state_change.is_null())
      _last_kpi_update = std::max(_last_kpi_update, last_state_change);
  }
}

/**
 *  Get the current BA event.
 *
 *  @return Current BA event, NULL if none is declared.
 */
std::shared_ptr<pb_ba_event> ba::get_ba_event() {
  return _event;
}

/**
 *  Get the BA ID.
 *
 *  @return ID of this BA.
 */
uint32_t ba::get_id() const {
  return _id;
}

/**
 *  Get the id of the host associated to this ba.
 *
 *  @return  An integer representing the value of this id.
 */
uint32_t ba::get_host_id() const {
  return _host_id;
}

/**
 *  Get the id of the service associated to this ba.
 *
 *  @return  An integer representing the value of this id.
 */
uint32_t ba::get_service_id() const {
  return _service_id;
}

/**
 *  @brief Check if the BA is in downtime.
 *
 *  The flag comes from the attached monitoring service.
 *
 *  @return True if the BA is in downtime, false otherwise.
 */
bool ba::in_downtime() const {
  return _in_downtime;
}

/**
 *  Get the time at which the most recent KPI was updated.
 *
 *  @return Time at which the most recent KPI was updated.
 */
timestamp ba::get_last_kpi_update() const {
  return _last_kpi_update;
}

/**
 *  Get the BA name.
 *
 *  @return BA name.
 */
std::string const& ba::get_name() const {
  return _name;
}

/**
 *  Get BA state source.
 *
 *  @return BA state source.
 */
configuration::ba::state_source ba::get_state_source(void) const {
  return _state_source;
}

/**
 *  Remove child impact.
 *
 *  @param[in] impact Impact to remove.
 */
void ba::remove_impact(std::shared_ptr<kpi> const& impact) {
  std::unordered_map<kpi*, impact_info>::iterator it(
      _impacts.find(impact.get()));
  if (it != _impacts.end()) {
    _unapply_impact(it->first, it->second);
    _impacts.erase(it);
  }
}

/**
 *  @brief Set the initial, opened event of this ba.
 *
 *  Useful for recovery after cbd stop.
 *
 *  @param[in] event  The event to set.
 */
void ba::set_initial_event(const pb_ba_event& event) {
  const BaEvent& data = event.obj();
  SPDLOG_LOGGER_TRACE(
      _logger,
      "BAM: ba initial event set (ba_id:{}, start_time:{}, end_time:{}, "
      "in_downtime:{}, status:{})",
      data.ba_id(), data.start_time(), data.end_time(), data.in_downtime(),
      static_cast<uint32_t>(data.status()));

  if (!_event) {
    _event = std::make_shared<pb_ba_event>(event);
    _in_downtime = data.in_downtime();
    SPDLOG_LOGGER_TRACE(_logger, "ba initial event downtime: {}", _in_downtime);
    _last_kpi_update = data.start_time();
    _initial_events.push_back(_event);
  } else {
    SPDLOG_LOGGER_ERROR(
        _logger,
        "BAM: impossible to set ba initial event (ba_id:{}, start_time:{}, "
        "end_time:{}, in_downtime:{}, status:{}): event already defined",
        data.ba_id(), data.start_time(), data.end_time(), data.in_downtime(),
        static_cast<uint32_t>(data.status()));
  }
}

/**
 *  Set the BA name.
 *
 *  @param[in] name  New BA name.
 */
void ba::set_name(std::string const& name) {
  _name = name;
}

/**
 *  @brief Set whether or not BA is valid.
 *
 *  An invalid BA will return an UNKNOWN state.
 *
 *  @param[in] valid  Whether or not BA is valid.
 */
void ba::set_valid(bool valid) {
  _valid = valid;
}

/**
 * @brief Set the inherit kpi downtime flag.
 *
 *  @param[in] value  The value to set.
 */
void ba::set_downtime_behaviour(configuration::ba::downtime_behaviour value) {
  _dt_behaviour = value;
}

/**
 *  Visit BA.
 *
 *  @param[out] visitor  Visitor that will receive BA status and events.
 */
void ba::visit(io::stream* visitor) {
  if (visitor) {
    // Commit initial events.
    _commit_initial_events(visitor);

    // If no event was cached, create one if necessary.
    com::centreon::broker::bam::state hard_state(get_state_hard());
    bool state_changed(false);
    if (!_event) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "BAM: ba::visit no event => creation of one");
      if (_last_kpi_update.is_null())
        _last_kpi_update = time(nullptr);
      _open_new_event(visitor, hard_state);
    }
    // If state changed, close event and open a new one.
    else if (_in_downtime != _event->obj().in_downtime() ||
             com::centreon::broker::State(hard_state) !=
                 _event->obj().status()) {
      SPDLOG_LOGGER_TRACE(
          _logger,
          "BAM: ba current event needs update? downtime?: {}, state?: {} ; "
          "dt:{}, state:{} ",
          _in_downtime != _event->obj().in_downtime(),
          com::centreon::broker::State(hard_state) != _event->obj().status(),
          _in_downtime, static_cast<uint32_t>(hard_state));
      state_changed = true;
      _event->mut_obj().set_end_time(_last_kpi_update);
      visitor->write(std::static_pointer_cast<io::data>(_event));
      _event.reset();
      _open_new_event(visitor, hard_state);
    }

    // Generate BA status event.
    auto status{_generate_ba_status(state_changed)};
    visitor->write(status);

    // Generate virtual service status event.
    if (_generate_virtual_status) {
      auto status{_generate_virtual_service_status()};
      visitor->write(status);
    }
  }
}

/**
 *  @brief Notify BA of a downtime
 *
 *  Used to watch for downtime.
 *
 *  @param dt       Downtime of the service.
 *  @param visitor  Visitor that will receive events.
 */
void ba::service_update(const std::shared_ptr<neb::downtime>& dt,
                        io::stream* visitor) {
  if (dt->host_id == _host_id && dt->service_id == _service_id) {
    // Log message.
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "BAM: BA {} '{}' is getting notified of a downtime on its service ({}, "
        "{})",
        _id, _name, _host_id, _service_id);

    // Check if there was a change.
    bool in_downtime(dt->was_started && dt->actual_end_time.is_null());
    if (_in_downtime != in_downtime) {
      SPDLOG_LOGGER_TRACE(_logger, "ba: service_update downtime: {}",
                          _in_downtime);
      _in_downtime = in_downtime;

      // Generate status event.
      visit(visitor);

      notify_parents_of_change(visitor);
    }
  } else
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "BAM: BA {} '{}' has got an invalid downtime event. This should never "
        "happen. Check your database: got (host {}, service {}) expected ({}, "
        "{})",
        _id, _name, dt->host_id, dt->service_id, _host_id, _service_id);
}

/**
 *  @brief Notify BA of a downtime (protobuf)
 *
 *  Used to watch for downtime.
 *
 *  @param dt       Downtime of the service.
 *  @param visitor  Visitor that will receive events.
 */
void ba::service_update(const std::shared_ptr<neb::pb_downtime>& dt,
                        io::stream* visitor) {
  auto& downtime = dt->obj();
  assert(downtime.host_id() == _host_id &&
         downtime.service_id() == _service_id);

  // Log message.
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM: BA {} '{}' is getting notified of a downtime (pb) on its service "
      "({}, {})",
      _id, _name, _host_id, _service_id);

  // Check if there was a change.
  bool in_downtime(downtime.started() &&
                   time_is_undefined(downtime.actual_end_time()));
  if (_in_downtime != in_downtime) {
    SPDLOG_LOGGER_TRACE(_logger, "ba: service_update downtime: {}",
                        _in_downtime);
    _in_downtime = in_downtime;

    // Generate status event.
    visit(visitor);

    notify_parents_of_change(visitor);
  }
}

/**
 *  Save the inherited downtime to the cache.
 *
 *  @param[in] cache  The cache.
 */
void ba::save_inherited_downtime(persistent_cache& cache) const {
  if (_inherited_downtime)
    cache.add(
        std::make_shared<pb_inherited_downtime>(_inherited_downtime->obj()));
}

/**
 *  Set the inherited downtime of this ba.
 *
 *  @param[in] dwn  The inherited downtime.
 */
void ba::set_inherited_downtime(const pb_inherited_downtime& dwn) {
  _inherited_downtime.reset(new pb_inherited_downtime(dwn.obj()));
  if (_inherited_downtime->obj().in_downtime())
    _in_downtime = true;
}

/**
 *  Set the inherited downtime of this ba.
 *
 *  @param[in] dwn  The inherited downtime.
 */
void ba::set_inherited_downtime(const inherited_downtime& dwn) {
  _inherited_downtime.reset(new pb_inherited_downtime);
  _inherited_downtime->mut_obj().set_ba_id(dwn.ba_id);
  _inherited_downtime->mut_obj().set_in_downtime(dwn.in_downtime);
  if (_inherited_downtime->obj().in_downtime())
    _in_downtime = true;
}

/**
 *  Open a new event for this BA.
 *
 *  @param[out] visitor             Visitor that will receive events.
 *  @param[in]  service_hard_state  Hard state of virtual BA service.
 */
void ba::_open_new_event(io::stream* visitor,
                         com::centreon::broker::bam::state service_hard_state) {
  SPDLOG_LOGGER_TRACE(_logger, "new pb_ba_event on ba {} with downtime = {}",
                      _id, _in_downtime);
  _event = std::make_shared<pb_ba_event>();
  BaEvent& data = _event->mut_obj();
  data.set_ba_id(_id);
  data.set_first_level(_level_hard < 0 ? 0 : _level_hard);
  data.set_in_downtime(_in_downtime);
  data.set_status(com::centreon::broker::State(service_hard_state));
  data.set_start_time(_last_kpi_update.get_time_t());
  data.set_end_time(-1);
  if (visitor) {
    std::shared_ptr<io::data> be{std::make_shared<pb_ba_event>(*_event)};
    visitor->write(be);
  }
}

/**
 * Commit the initial events of this ba.
 *
 *  @param[in] visitor  The visitor.
 */
void ba::_commit_initial_events(io::stream* visitor) {
  if (_initial_events.empty())
    return;

  if (visitor) {
    for (const auto& evt : _initial_events)
      visitor->write(evt);
  }
  _initial_events.clear();
}

/**
 *  Compute the inherited downtime.
 */
void ba::_compute_inherited_downtime(io::stream* visitor) {
  // kpi downtime heritance deactived. Do nothing.
  if (_dt_behaviour != configuration::ba::dt_inherit) {
    SPDLOG_LOGGER_TRACE(_logger, "ba: BA {} doesn't inherite downtimes", _id);
    return;
  }

  // Check if every impacting child KPIs are in downtime.
  bool every_kpi_in_downtime(!_impacts.empty());
  for (std::unordered_map<kpi*, impact_info>::const_iterator
           it = _impacts.begin(),
           end = _impacts.end();
       it != end; ++it) {
    if (!it->first->ok_state() && !it->first->in_downtime()) {
      SPDLOG_LOGGER_TRACE(
          _logger,
          "ba: every kpi in downtime ? no, kpi {} is not ok and not in "
          "downtime",
          it->first->get_id());
      every_kpi_in_downtime = false;
      break;
    }
  }

  // Case 1: state not ok, every child in downtime, no actual downtime.
  //         Put the BA in downtime.
  bool s_ok{get_state_hard() == state_ok};
  if (!s_ok && every_kpi_in_downtime && !_inherited_downtime) {
    _inherited_downtime = std::make_unique<pb_inherited_downtime>();
    _inherited_downtime->mut_obj().set_ba_id(_id);
    _inherited_downtime->mut_obj().set_in_downtime(true);
    SPDLOG_LOGGER_TRACE(_logger,
                        "ba: inherited downtime computation downtime true");
    _in_downtime = true;

    if (visitor)
      visitor->write(
          std::make_shared<pb_inherited_downtime>(_inherited_downtime->obj()));
  }
  // Case 2: state ok or not every kpi in downtime, actual downtime.
  //         Remove the downtime.
  else if ((s_ok || !every_kpi_in_downtime) && _inherited_downtime) {
    _inherited_downtime->mut_obj().set_in_downtime(false);
    SPDLOG_LOGGER_TRACE(_logger,
                        "ba: inherited downtime computation downtime false");
    _in_downtime = false;

    if (visitor)
      visitor->write(std::move(_inherited_downtime));
    _inherited_downtime.reset();
  } else
    SPDLOG_LOGGER_TRACE(
        _logger, "ba: inherited downtime computation downtime not changed ({})",
        _in_downtime);
}

std::shared_ptr<io::data> ba::_generate_virtual_service_status() const {
  auto bbdo = config::applier::state::instance().get_bbdo_version();
  if (bbdo.major_v < 3) {
    auto status{std::make_shared<neb::service_status>()};
    status->active_checks_enabled = false;
    status->check_interval = 0.0;
    status->check_type = 1;  // Passive.
    status->current_check_attempt = 1;
    status->current_state = 0;
    status->enabled = true;
    status->event_handler_enabled = false;
    status->execution_time = 0.0;
    status->flap_detection_enabled = false;
    status->has_been_checked = true;
    status->host_id = _host_id;
    status->downtime_depth = _in_downtime;
    status->is_flapping = false;
    if (_event)
      status->last_check = _event->obj().start_time();
    else
      status->last_check = _last_kpi_update;
    status->last_hard_state = get_state_hard();
    status->last_hard_state_change = status->last_check;
    status->last_state_change = status->last_check;
    status->last_update = time(nullptr);
    status->latency = 0.0;
    status->max_check_attempts = 1;
    status->obsess_over = false;
    status->output = get_output();
    status->perf_data = fmt::format(
        "BA_Level={}%;{};{};0;100", static_cast<int>(_normalize(_level_hard)),
        static_cast<int>(_level_warning), static_cast<int>(_level_critical));
    status->retry_interval = 0;
    status->service_id = _service_id;
    status->should_be_scheduled = false;
    status->state_type = 1;  // Hard.
    return status;
  } else {
    auto status{std::make_shared<neb::pb_service_status>()};
    auto& o = status->mut_obj();
    o.set_check_type(ServiceStatus_CheckType_PASSIVE);  // Passive.
    o.set_check_attempt(1);
    o.set_state(ServiceStatus_State_OK);
    o.set_checked(true);
    o.set_host_id(_host_id);
    o.set_scheduled_downtime_depth(_in_downtime);
    if (_event)
      o.set_last_check(_event->obj().start_time());
    else
      o.set_last_check(_last_kpi_update);
    o.set_last_hard_state(static_cast<ServiceStatus_State>(get_state_hard()));
    o.set_last_hard_state_change(o.last_check());
    o.set_last_state_change(o.last_check());
    o.set_output(get_output());
    o.set_perfdata(get_perfdata());
    o.set_service_id(_service_id);
    o.set_state_type(ServiceStatus_StateType_HARD);
    return status;
  }
}

/**
 *  Set critical level.
 *
 *  @param[in] level  Critical level.
 */
void ba::set_level_critical(double level) {
  _level_critical = level;
}

/**
 *  Set warning level.
 *
 *  @param[in] level  Warning level.
 */
void ba::set_level_warning(double level) {
  _level_warning = level;
}

/**
 * @brief Update this computable with the child modifications. This function is
 * called only if child has changes. And it will notify its parents only if it
 * also changes.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 */
void ba::update_from(computable* child, io::stream* visitor) {
  _logger->trace("ba::update_from (BA {})", _id);
  // Get impact.
  impact_values new_hard_impact;
  impact_values new_soft_impact;
  kpi* kpi_child = static_cast<kpi*>(child);
  kpi_child->impact_hard(new_hard_impact);
  kpi_child->impact_soft(new_soft_impact);
  bool kpi_in_downtime(kpi_child->in_downtime());
  bool previous_in_downtime = _in_downtime;

  // Logging.
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM: BA {}, '{}' is getting notified of child update (KPI {}, impact "
      "{}, last state change {}, downtime {})",
      _id, _name, kpi_child->get_id(), new_hard_impact.get_nominal(),
      kpi_child->get_last_state_change(), kpi_in_downtime);

  timestamp last_state_change(kpi_child->get_last_state_change());
  if (!last_state_change.is_null())
    _last_kpi_update = std::max(_last_kpi_update, last_state_change);

  // Apply new data.
  SPDLOG_LOGGER_TRACE(_logger, "BAM: BA {} updated from KPI {}", _id,
                      kpi_child->get_id());
  bool changed = _apply_changes(kpi_child, new_hard_impact, new_soft_impact,
                                kpi_in_downtime);
  SPDLOG_LOGGER_TRACE(_logger, "BA {} has changed: {}", _id, changed);

  // Check for inherited downtimes.
  _compute_inherited_downtime(visitor);

  // Generate status event.
  visit(visitor);

  if (changed || _in_downtime != previous_in_downtime)
    notify_parents_of_change(visitor);
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string ba::object_info() const {
  return fmt::format("BA {}\nname: {}\nstate: {}\ndowntime: {}", _id, _name,
                     static_cast<uint32_t>(get_state_hard()), _in_downtime);
}

/**
 * @brief Dump the BA to a file named filename.
 *
 * @param filename Name of the file (currently a graphviz dot file).
 */
void ba::dump(const std::string& filename) const {
  std::ofstream output{filename};
  if (output) {
    output << "digraph {\n";
    dump(output);
    output << "}\n";
  } else
    _logger->error("Unable to open the file '{}' to write BA {} info", filename,
                   _id);
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void ba::dump(std::ofstream& output) const {
  for (auto& ki : _impacts) {
    output << fmt::format("\"{}\" -> \"{}\"\n", object_info(),
                          ki.first->object_info());
    ki.first->dump(output);
  }
  dump_parents(output);
}

/**
 *
 *  Get the hard impact introduced by acknowledged KPI.
 *
 *  @return Hard impact introduced by acknowledged KPI.
 */
int32_t ba::get_ack_impact_hard() const {
  return _acknowledgement_count;
}
