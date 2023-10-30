/*
 * Copyright 2014-2023 Centreon
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

#include "com/centreon/broker/bam/bool_operation.hh"

#include <cmath>

using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 *
 *  @param[in] op  The operation in string format.
 */
bool_operation::bool_operation(std::string const& op)
    : _type((op == "+")   ? addition
            : (op == "-") ? substraction
            : (op == "*") ? multiplication
            : (op == "/") ? division
            : (op == "%") ? modulo
                          : addition) {}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_operation::value_hard() const {
  switch (_type) {
    case addition:
      return _left_hard + _right_hard;
    case substraction:
      return _left_hard - _right_hard;
    case multiplication:
      return _left_hard * _right_hard;
    case division:
      if (std::fabs(_right_hard) < COMPARE_EPSILON)
        return NAN;
      return _left_hard / _right_hard;
    case modulo: {
      long long left_val(static_cast<long long>(_left_hard));
      long long right_val(static_cast<long long>(_right_hard));
      if (right_val == 0)
        return NAN;
      return left_val % right_val;
    }
  }
  return NAN;
}

bool bool_operation::boolean_value() const {
  switch (_type) {
    case addition:
      return _left_hard + _right_hard;
    case substraction:
      return _left_hard - _right_hard;
    case multiplication:
      return std::fabs(_left_hard * _right_hard) > COMPARE_EPSILON;
    case division:
      if (std::fabs(_right_hard) < COMPARE_EPSILON)
        return false;
      return _left_hard / _right_hard;
    case modulo: {
      long long left_val(static_cast<long long>(_left_hard));
      long long right_val(static_cast<long long>(_right_hard));
      if (right_val == 0)
        return false;
      return left_val % right_val;
    }
  }
  return false;
}
/**
 *  Is the state known?
 *
 *  @return  True if the state is known.
 */
bool bool_operation::state_known() const {
  bool known = bool_binary_operator::state_known();
  if (known && (_type == division || _type == modulo) &&
      std::fabs(_right_hard) < COMPARE_EPSILON)
    return false;
  else
    return known;
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_operation::object_info() const {
  const char* op;
  switch (_type) {
    case addition:
      op = "PLUS";
      break;
    case substraction:
      op = "MINUS";
      break;
    case multiplication:
      op = "MUL";
      break;
    case division:
      op = "DIV";
      break;
    case modulo:
      op = "MODULO";
      break;
    default:
      return "unknown operation";
  }
  return fmt::format(
      "{} {:p}\nknown: {}\nvalue: {}", op, static_cast<const void*>(this),
      state_known() ? "true" : "false", boolean_value() ? "true" : "false");
}
