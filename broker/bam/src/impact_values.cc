/**
* Copyright 2014 Centreon
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

#include "com/centreon/broker/bam/impact_values.hh"

using namespace com::centreon::broker::bam;

static constexpr double eps = 0.000001;

/**
 *  Constructor.
 *
 *  @param[in] nominal          Nominal impact.
 *  @param[in] acknowledgement  Part of impact induced by an
 *                              acknowledgement.
 *  @param[in] downtime         Part of impact induced by a downtime.
 */
impact_values::impact_values(double nominal,
                             double acknowledgement,
                             double downtime,
                             state state)
    : _acknowledgement(acknowledgement),
      _downtime(downtime),
      _nominal(nominal),
      _state{state} {}

/**
 *  Destructor.
 */
impact_values::~impact_values() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
impact_values& impact_values::operator=(impact_values const& other) {
  if (this != &other) {
    _acknowledgement = other._acknowledgement;
    _downtime = other._downtime;
    _nominal = other._nominal;
    _state = other._state;
  }
  return *this;
}

/**
 *  Comparison operator.
 *
 *  @param[in] other   Object to compare to.
 *
 *  @return True if equal.
 */
bool impact_values::operator==(impact_values const& other) const throw() {
  if (this == &other)
    return true;
  return std::abs(_acknowledgement - other._acknowledgement) < eps &&
         std::abs(_downtime - other._downtime) < eps &&
         std::abs(_nominal - other._nominal) < eps && _state == other._state;
}

/**
 *  Comparison operator.
 *
 *  @param[in] other   Object to compare to.
 *
 *  @return True if not equal.
 */
bool impact_values::operator!=(impact_values const& other) const throw() {
  return std::abs(_acknowledgement - other._acknowledgement) >= eps ||
         std::abs(_downtime - other._downtime) >= eps ||
         std::abs(_nominal - other._nominal) >= eps || _state != other._state;
}

/**
 *  Get impact induced by acknowledgement.
 *
 *  @return Impact induced by some acknowledgement.
 */
double impact_values::get_acknowledgement() const {
  return _acknowledgement;
}

/**
 *  Get impact induced by downtime.
 *
 *  @return Impact induced by some downtime.
 */
double impact_values::get_downtime() const {
  return _downtime;
}

/**
 *  Get nominal impact.
 *
 *  @return Nominal impact.
 */
double impact_values::get_nominal() const {
  return _nominal;
}

/**
 *  Get impact state.
 *
 *  @return State impact.
 */
state impact_values::get_state() const {
  return _state;
}

/**
 *  Set impact induced by acknowledgement.
 *
 *  @param[in] acknowledgement  Impact induced by some acknowledgement.
 */
void impact_values::set_acknowledgement(double acknowledgement) {
  _acknowledgement = acknowledgement;
}

/**
 *  Set impact induced by downtime.
 *
 *  @param[in] downtime  Impact induced by some downtime.
 */
void impact_values::set_downtime(double downtime) {
  _downtime = downtime;
}

/**
 *  Set nominal impact.
 *
 *  @param[in] nominal  Nominal impact.
 */
void impact_values::set_nominal(double nominal) {
  _nominal = nominal;
}

/**
 *  Set impact state.
 *
 *  @param[in] state  State impact.
 */
void impact_values::set_state(state state) {
  _state = state;
}
