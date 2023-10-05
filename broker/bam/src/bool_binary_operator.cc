/**
 * Copyright 2014-2016, 2021, 2023 Centreon
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

#include "com/centreon/broker/bam/bool_binary_operator.hh"

using namespace com::centreon::broker::bam;

static constexpr double eps = 0.000001;

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
    _logger->trace("{}::_update_state: bool binary operator: state updated? {}",
                   typeid(*this).name(), _state_known ? "yes" : "no");
    if (_state_known) {
      _left_hard = _left->value_hard();
      _right_hard = _right->value_hard();
      _in_downtime = _left->in_downtime() || _right->in_downtime();
      _logger->trace(
          "{}::_update_state: bool binary operator: new left value: {} - new "
          "right value: {} - downtime: {}",
          typeid(*this).name(), _left_hard, _right_hard,
          _in_downtime ? "yes" : "no");
    }
  } else {
    _state_known = false;
    _logger->trace(
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
  _logger->trace("{}::set_left", typeid(*this).name());
  _left = left;
  _update_state();
}

/**
 *  Set right member.
 *
 *  @param[in] right Right member of the boolean operator.
 */
void bool_binary_operator::set_right(std::shared_ptr<bool_value> const& right) {
  _logger->trace("{}::set_right", typeid(*this).name());
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

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 * @param logger The logger to use.
 */
void bool_binary_operator::update_from(
    computable* child,
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  logger->trace("bool_binary_operator::update_from");
  // Check operation members values.
  bool changed = false;
  if (child) {
    if (child == _left.get()) {
      if (_left->state_known() != _state_known ||
          std::abs(_left_hard - _left->value_hard()) > ::eps) {
        logger->trace(
            "{}::update_from: on left: old state known: {} - new state "
            "known: {} - old value: {} - new value: {}",
            typeid(*this).name(), _state_known, _left->state_known(),
            _left_hard, _left->value_hard());
        _update_state();
        changed = true;
      } else
        logger->trace(
            "{}::update_from: bool_binary_operator: update_from: no "
            "on left - state known: {} - value: {}",
            typeid(*this).name(), _state_known, _left_hard);
    } else if (child == _right.get()) {
      if (_right->state_known() != _state_known ||
          std::abs(_right_hard - _right->value_hard()) > ::eps) {
        logger->trace(
            "{}::update_from on right: old state known: {} - new state "
            "known: {} - old value: {} - new value: {}",
            typeid(*this).name(), _state_known, _right->state_known(),
            _right_hard, _right->value_hard());
        _update_state();
        changed = true;
      } else
        logger->trace(
            "{}::update_from: bool_binary_operator: update_from: no "
            "on right",
            typeid(*this).name());
    }
  }
  if (changed)
    notify_parents_of_change(visitor, logger);
}

void bool_binary_operator::dump(std::ofstream& output) const {
  output << fmt::format("\"{}\" -> \"{}\"\n", object_info(),
                        _left->object_info());
  output << fmt::format("\"{}\" -> \"{}\"\n", object_info(),
                        _right->object_info());
  _left->dump(output);
  _right->dump(output);
  dump_parents(output);
}
