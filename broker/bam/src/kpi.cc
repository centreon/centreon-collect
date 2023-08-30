/**
* Copyright 2014-2015, 2021 Centreon
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

#include "com/centreon/broker/bam/kpi.hh"

#include "com/centreon/broker/bam/ba.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

static constexpr double eps = 0.000001;

/**
 *  Constructor.
 */
kpi::kpi(uint32_t kpi_id,
         uint32_t ba_id,
         const std::string& name,
         const std::shared_ptr<spdlog::logger>& logger)
    : computable(logger), _id(kpi_id), _ba_id(ba_id), _name(name) {}

/**
 *  Get KPI ID.
 *
 *  @return KPI ID.
 */
uint32_t kpi::get_id() const {
  return _id;
}

/**
 *  Get BA ID impacted by KPI.
 *
 *  @return BA ID.
 */
uint32_t kpi::get_ba_id() const {
  return _ba_id;
}

/**
 *  Get the last state change.
 *
 *  @return Last state change.
 */
timestamp kpi::get_last_state_change() const {
  return _event ? timestamp(_event->start_time()) : timestamp::now();
}

/**
 *  Set the initial event of the kpi.
 *
 *  @param[in] e  The kpi event.
 */
void kpi::set_initial_event(const KpiEvent& e) {
  log_v2::bam()->trace("bam: kpi::set_initial_event");
  if (!_event) {
    _event = e;
    impact_values impacts;
    impact_hard(impacts);
    double new_impact_level =
        _event->in_downtime() ? impacts.get_downtime() : impacts.get_nominal();
    // If the new impact is not equal to the impact saved in the initial event,
    // then close the initial event and open a new event.
    if (std::abs(new_impact_level - _event->impact_level()) >= eps &&
        _event->impact_level() != -1) {
      time_t now = ::time(nullptr);
      KpiEvent new_event(e);
      new_event.set_end_time(now);
      _initial_events.push_back(new_event);
      new_event.set_end_time(e.end_time());
      new_event.set_start_time(now);
      _initial_events.push_back(new_event);
      _event = std::move(new_event);
    } else
      _initial_events.push_back(*_event);
    _event->set_impact_level(new_impact_level);
  }
}

/**
 * Commit the initial events of this kpi.
 *
 *  @param[in] visitor  The visitor.
 */
void kpi::commit_initial_events(io::stream* visitor) {
  if (_initial_events.empty())
    return;

  if (visitor) {
    for (KpiEvent& to_write : _initial_events)
      visitor->write(std::make_shared<pb_kpi_event>(std::move(to_write)));
  }
  _initial_events.clear();
}

/**
 *  Is this kpi in downtime?
 *
 *  @return  Default value: false.
 */
bool kpi::in_downtime() const {
  return false;
}

/**
 * @brief initialized optional _event member
 *
 */
void kpi::_event_init() {
  if (!_event) {
    _event = KpiEvent();
    _event->set_kpi_id(_id);
    _event->set_ba_id(_ba_id);
  }
}
