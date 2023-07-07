/*
** Copyright 2014-2016, 2021, 2023 Centreon
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

#include "com/centreon/broker/bam/bool_binary_operator.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::bam;

static constexpr double eps = 0.000001;

/**
 *  Notification of child update.
 *
 *  @param[in] child     Child that got updated.
 *  @param[out] visitor  Visitor.
 *
 *  @return              True if the values of this object were modified.
 */
bool bool_binary_operator::child_has_update(computable* child,
                                            io::stream* visitor) {
  (void)visitor;
  bool retval = false;

  // Check operation members values.
  if (child) {
    if (child == _left.get()) {
      if (_left->state_known() != _state_known ||
          std::abs(_left_hard - _left->value_hard()) > ::eps) {
        log_v2::bam()->trace(
            "{}::child_has_update: on left: old state known: {} - new state "
            "known: {} - old value: {} - new value: {}",
            typeid(*this).name(), _state_known, _left->state_known(),
            _left_hard, _left->value_hard());
        _update_state();
        retval = true;
      } else
        log_v2::bam()->trace(
            "{}::child_has_update: bool_binary_operator: child_has_update: no "
            "on left - state known: {} - value: {}",
            typeid(*this).name(), _state_known, _left_hard);
    } else if (child == _right.get()) {
      if (_right->state_known() != _state_known ||
          std::abs(_right_hard - _right->value_hard()) > ::eps) {
        log_v2::bam()->trace(
            "{}::child_has_update on right: old state known: {} - new state "
            "known: {} - old value: {} - new value: {}",
            typeid(*this).name(), _state_known, _right->state_known(),
            _right_hard, _right->value_hard());
        _update_state();
        retval = true;
      } else
        log_v2::bam()->trace(
            "{}::child_has_update: bool_binary_operator: child_has_update: no "
            "on right",
            typeid(*this).name());
    }
  }

  return retval;
}

/**
 * @brief Check that the children are known and in that case propagate their
 * values to _left_hard and _right_hard. Otherwise these attributes have no
 * meaning.
 *
 * After a call to this internal function, the _state_known attribute has the
 * good value.
 */
void bool_binary_operator::_update_state() {
  if (_left && _right) {
    _state_known = _left->state_known() && _right->state_known();
    log_v2::bam()->trace(
        "{}::_update_state: bool binary operator: state updated? {}",
        typeid(*this).name(), _state_known ? "yes" : "no");
    if (_state_known) {
      _left_hard = _left->value_hard();
      _right_hard = _right->value_hard();
      _in_downtime = _left->in_downtime() || _right->in_downtime();
      log_v2::bam()->trace(
          "{}::_update_state: bool binary operator: new left value: {} - new "
          "right value: {} "
          "- downtime: {}",
          typeid(*this).name(), _left_hard, _right_hard,
          _in_downtime ? "yes" : "no");
    }
  } else {
    _state_known = false;
    log_v2::bam()->trace(
        "{}::_update_state: bool binary operator: some children are empty",
        typeid(*this).name());
  }
}

/**
 *  Set left member.
 *
 *  @param[in] left Left member of the boolean operator.
 */
void bool_binary_operator::set_left(std::shared_ptr<bool_value> const& left) {
  _left = left;
  _update_state();
}

/**
 *  Set right member.
 *
 *  @param[in] right Right member of the boolean operator.
 */
void bool_binary_operator::set_right(std::shared_ptr<bool_value> const& right) {
  _right = right;
  _update_state();
}

/**
 *  Get if the state is known, i.e has been computed at least once.
 *
 *  @return  True if the state is known.
 */
bool bool_binary_operator::state_known() const {
  return _state_known;
}

/**
 *  Is this expression in downtime?
 *
 *  @return  True if this expression is in downtime.
 */
bool bool_binary_operator::in_downtime() const {
  return _in_downtime;
}
