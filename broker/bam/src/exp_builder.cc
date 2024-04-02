/**
 * Copyright 2016 Centreon
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

#include "com/centreon/broker/bam/exp_builder.hh"

#include "com/centreon/broker/bam/bool_and.hh"
#include "com/centreon/broker/bam/bool_constant.hh"
#include "com/centreon/broker/bam/bool_equal.hh"
#include "com/centreon/broker/bam/bool_less_than.hh"
#include "com/centreon/broker/bam/bool_more_than.hh"
#include "com/centreon/broker/bam/bool_not.hh"
#include "com/centreon/broker/bam/bool_not_equal.hh"
#include "com/centreon/broker/bam/bool_operation.hh"
#include "com/centreon/broker/bam/bool_or.hh"
#include "com/centreon/broker/bam/bool_service.hh"
#include "com/centreon/broker/bam/bool_xor.hh"
#include "com/centreon/broker/bam/exp_parser.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Constructor.
 *
 *  @param[in] postfix  Postfix notation of the expression. Likely
 *                      generated by exp_parser.
 *  @param[in] mapping  Map host names and service descriptions to IDs.
 *
 *  @see exp_parser
 */
exp_builder::exp_builder(exp_parser::notation const& postfix,
                         hst_svc_mapping const& mapping,
                         const std::shared_ptr<spdlog::logger>& logger)
    : _logger(logger), _mapping(mapping) {
  // Browse all tokens.
  for (exp_parser::notation::const_iterator it(postfix.begin()),
       end(postfix.end());
       it != end; ++it) {
    // Operators.
    if (exp_parser::is_operator(*it)) {
      // Unary operators.
      if (*it == "-u") {
        // XXX
      } else if (*it == "!") {
        bool_value::ptr arg(_pop_operand());
        any_operand exp(std::make_shared<bool_not>(arg, _logger), "");
        arg->add_parent(exp.first);
        _operands.push(exp);
      }
      // Binary operators.
      else {
        bool_binary_operator::ptr binary;
        if (*it == "&&" || *it == "AND")
          binary = std::make_shared<bool_and>(_logger);
        else if (*it == "||" || *it == "OR")
          binary = std::make_shared<bool_or>(_logger);
        else if (*it == "^" || *it == "XOR")
          binary = std::make_shared<bool_xor>(_logger);
        else if (*it == "==" || *it == "IS")
          binary = std::make_shared<bool_equal>(_logger);
        else if (*it == "!=" || *it == "NOT")
          binary = std::make_shared<bool_not_equal>(_logger);
        else if (*it == ">")
          binary = std::make_shared<bool_more_than>(true, _logger);
        else if (*it == ">=")
          binary = std::make_shared<bool_more_than>(false, _logger);
        else if (*it == "<")
          binary = std::make_shared<bool_less_than>(true, _logger);
        else if (*it == "<=")
          binary = std::make_shared<bool_less_than>(false, _logger);
        else if (*it == "+" || *it == "-" || *it == "*" || *it == "/" ||
                 *it == "%")
          binary = std::make_shared<bool_operation>(*it, _logger);
        else
          throw msg_fmt(
              "unsupported operator {} found while parsing expression", *it);
        bool_value::ptr right(_pop_operand());
        bool_value::ptr left(_pop_operand());
        left->add_parent(binary);
        right->add_parent(binary);
        binary->set_left(left);
        binary->set_right(right);
        _operands.push(any_operand(binary, ""));
      }
    }
    // "Static" function calls (status or metrics retrieval).
    else if (_is_static_function(*it)) {
      // Arity should be placed after function name.
      std::string func(*it);
      if (++it == end)
        throw msg_fmt(
            "internal expression parsing error: no arity placed after function "
            "name in postfix notation");
      int arity(std::strtol(it->c_str(), nullptr, 0));

      // Host status.
      if (func == "HOSTSTATUS") {
        // Arity check.
        _check_arity("HOSTSTATUS()", 1, arity);

        // XXX
        _pop_string();
      }
      // Service status.
      else if (func == "SERVICESTATUS") {
        // Arity check.
        _check_arity("SERVICESTATUS()", 2, arity);
        std::string svc(_pop_string());
        std::string hst(_pop_string());

        // Find host and service IDs.
        std::pair<uint32_t, uint32_t> ids(_mapping.get_service_id(hst, svc));
        if (!ids.first || !ids.second)
          throw msg_fmt("could not find ID of service '{}' and/or of host '{}'",
                        svc, hst);

        // Build object.
        bool_service::ptr obj{
            std::make_shared<bool_service>(ids.first, ids.second, _logger)};

        // Store it in the operand stack and within the service list.
        _operands.push(any_operand(obj, ""));
        _services.push_back(obj);
      }
      // Single metric.
      else if (func == "METRIC") {
        // Arity check.
        _check_arity("METRIC()", 3, arity);

        // XXX
        _pop_string();
        _pop_string();
        _pop_string();
      }
      // Multiple metrics.
      else if (func == "METRICS") {
        // Arity check.
        _check_arity("METRICS()", 1, arity);

        // XXX
        _pop_string();
      }
      // Call.
      else if (func == "CALL") {
        // Arity check.
        _check_arity("CALL()", 1, arity);

        // XXX
        _pop_string();
      }
      // Unsupported function.
      else
        throw msg_fmt("unsupported static function '{}'", func);
    }
    // Classical function call.
    else if (exp_parser::is_function(*it)) {
      // Arity should be placed after function name.
      if (++it == end)
        throw msg_fmt(
            "internal expression parsing "
            "error: no arity placed after function name in "
            "postfix notation");
    }
    // Operand (will be evaluated when poped).
    else {
      any_operand op(bool_value::ptr(), *it);
      _operands.push(op);
    }
  }

  // The sole remaining operand should be the tree root.
  _tree = _pop_operand();
  if (!_operands.empty())
    throw msg_fmt("unable to build an expression: incorrect syntax");
}

/**
 *  Get calls existing in the expression.
 *
 *  @return The call list.
 */
exp_builder::list_call const& exp_builder::get_calls() const {
  return _calls;
}

/**
 *  Get services existing in the expression.
 *
 *  @return The services list.
 */
exp_builder::list_service const& exp_builder::get_services() const {
  return _services;
}

/**
 *  Get expression tree.
 *
 *  @return The expression tree.
 */
bool_value::ptr exp_builder::get_tree() const {
  return _tree;
}

/**
 *  Check the arity of a function or an operator.
 *
 *  @param[in] func  Function name or operator.
 *  @param[in] expected  Expected arity.
 *  @param[in] given     Number of arguments given.
 */
void exp_builder::_check_arity(std::string const& func,
                               int expected,
                               int given) {
  if (expected != given)
    throw msg_fmt(
        "invalid argument count for {}: it expects {}"
        " arguments, {} given",
        func, expected, given);
}

/**
 *  Check if a string is a static function (status or metric retrieval).
 *
 *  @param[in] str  String to check.
 *
 *  @return True if the string is a static function.
 */
bool exp_builder::_is_static_function(std::string const& str) const {
  return str == "HOSTSTATUS" || str == "SERVICESTATUS" || str == "METRICS" ||
         str == "METRIC" || str == "CALL";
}

/**
 *  Pop an operand off the operand stack.
 *
 *  If the operand has not yet been evaluated it is converted to its
 *  numerical value.
 *
 *  @return The ready-to-use operand.
 */
bool_value::ptr exp_builder::_pop_operand() {
  // Check that operand exist.
  if (_operands.empty())
    throw msg_fmt(
        "syntax error: operand is missing for "
        "operator or function");

  // Check if operand needs to be converted.
  bool_value::ptr retval;
  if (!_operands.top().first) {
    std::string& value_str(_operands.top().second);
    double value;
    if (value_str == "OK")
      value = 0;
    else if (value_str == "WARNING")
      value = 1;
    else if (value_str == "CRITICAL")
      value = 2;
    else if (value_str == "UNKNOWN")
      value = 3;
    else if (value_str == "UP")
      value = 0;
    else if (value_str == "DOWN")
      value = 1;
    else if (value_str == "UNREACHABLE")
      value = 2;
    else
      value = std::strtod(value_str.c_str(), nullptr);
    retval.reset(new bool_constant(value, _logger));
  } else
    retval = _operands.top().first;

  // Pop operand off the stack.
  _operands.pop();
  return retval;
}

/**
 *  Pop a string off the operand stack.
 *
 *  Throw if operand is not a string.
 *
 *  @return The next string in the operand stack.
 */
std::string exp_builder::_pop_string() {
  // Check that operand exists.
  if (_operands.empty())
    throw msg_fmt(
        "syntax error: operand is missing for "
        "operator or function");

  // Check that operand is a string.
  if (_operands.top().first || _operands.top().second.empty())
    throw msg_fmt("syntax error: operand was expected to be a string");

  // Retval.
  std::string retval(_operands.top().second);
  _operands.pop();
  return retval;
}
