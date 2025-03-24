/**
 * Copyright 2025 Centreon
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

#include "filter.hh"
#include "filter_rules.cc"

using namespace com::centreon::agent;
using namespace com::centreon::agent::filters;
namespace bp = boost::parser;

namespace std {
std::ostream& operator<<(std::ostream& s,
                         const com::centreon::agent::filter& filt) {
  filt.dump(s);
  return s;
}
}  // namespace std

void com::centreon::agent::wstring_to_string(std::wstring_view in_str,
                                             std::string* out_str) {
  out_str->clear();
  out_str->reserve(in_str.size());
  for (wchar_t c : in_str) {
    out_str->push_back(c);
  }
}

void com::centreon::agent::string_to_wstring(std::string_view in_str,
                                             std::wstring* out_str) {
  out_str->clear();
  out_str->reserve(in_str.size());
  for (wchar_t c : in_str) {
    out_str->push_back(c);
  }
}

bool filter::create_filter(std::string_view filter_str,
                           const std::shared_ptr<spdlog::logger>& logger,
                           filters::filter_combinator* filter,
                           bool use_wchar,
                           bool debug) {
  bp::trace dbg = debug ? bp::trace::on : bp::trace::off;
  filter_str = absl::StripLeadingAsciiWhitespace(filter_str);
  filter_str = absl::StripTrailingAsciiWhitespace(filter_str);

  auto err_handler = [filter_str, logger](const std::string& msg) {
    SPDLOG_LOGGER_ERROR(logger, "Fail to parse {}: {}", filter_str, msg);
  };

  auto warn_handler = [filter_str, logger](const std::string& msg) {
    SPDLOG_LOGGER_WARN(logger, "When parsing {}, a potential issue: {}",
                       filter_str, msg);
  };

  bp::callback_error_handler cb_handler("filter.cc", err_handler, warn_handler);
  if (use_wchar) {
    return bp::parse(
        filter_str,
        bp::with_error_handler(filter_combinator_rule_w, cb_handler), *filter,
        dbg);
  } else {
    return bp::parse(filter_str,
                     bp::with_error_handler(filter_combinator_rule, cb_handler),
                     *filter, dbg);
  }
}

bool filter::check(const testable& t) const {
  bool ret = _checker(t);
  if (_logger) {
    SPDLOG_LOGGER_TRACE(_logger, "filter {} check {}", *this, ret);
  }
  return ret;
}

/*************************************************************************
 *                                                                       *
 *                          label_compare_to_value                       *
 *                                                                       *
 *************************************************************************/

/**
 * @brief Construct a new label compare to value::label compare to value object
 *
 * @param value threshold value
 * @param unit %,s,....
 * @param compare <=, != ...
 * @param label id of the data to test
 */
label_compare_to_value::label_compare_to_value(double value,
                                               std::string&& unit,
                                               comparison compare,
                                               std::string&& label)
    : filter(filter_type::label_compare_to_value),
      _label(std::move(label)),
      _value(value),
      _unit(std::move(unit)),
      _comparison(compare) {
  // as string is in reversed order, we need to reverse comparison
  if (_comparison == comparison::less_than) {
    _comparison = comparison::greater_than;
  } else if (_comparison == comparison::greater_than) {
    _comparison = comparison::less_than;
  } else if (_comparison == comparison::less_than_or_equal) {
    _comparison = comparison::greater_than_or_equal;
  } else if (_comparison == comparison::greater_than_or_equal) {
    _comparison = comparison::less_than_or_equal;
  }
  dump(std::cout);
}

/**
 * @brief dump object to std::ostream
 *
 * @param s
 */
void label_compare_to_value::dump(std::ostream& s) const {
  s << ' ' << " { " << _label << ' ';

  switch (_comparison) {
    case comparison::less_than:
      s << "<";
      break;
    case comparison::greater_than:
      s << ">";
      break;
    case comparison::less_than_or_equal:
      s << "<=";
      break;
    case comparison::greater_than_or_equal:
      s << ">=";
      break;
    case comparison::equal:
      s << "==";
      break;
    case comparison::not_equal:
      s << "!=";
      break;
    default:
      s << "unknown comparison";
      break;
  }

  s << _value << ' ' << _unit << " } " << std::endl;
}

/**
 * @brief compare value in seconds (accept 1w2d30s)
 */
void label_compare_to_value::calc_duration() {
  if (!_unit.empty()) {
    switch (_unit[0]) {
      case 'w':
      case 'W':
        _value *= 86400 * 24;
        break;
      case 'd':
      case 'D':
        _value *= 86400;
        break;
      case 'h':
      case 'H':
        _value *= 3600;
        break;
      case 'm':
      case 'M':
        _value *= 60;
        break;
      default:
        break;
    }
  }

  _unit = "s";
}

/**
 * @brief compare value in bytes (accept 1G2M30K)
 */
void label_compare_to_value::calc_giga_mega_kilo() {
  if (!_unit.empty()) {
    switch (_unit[0]) {
      case 'g':
      case 'G':
        _value *= 1024 * 1024 * 1024;
        break;
      case 'm':
      case 'M':
        _value *= 1024 * 1024;
        break;
      case 'k':
      case 'K':
        _value *= 1024;
        break;
      default:
        break;
    }
  }

  _unit = "b";
}

/**
 * @brief change threshold to absolute value and reverse comparison
 */
void label_compare_to_value::change_threshold_to_abs() {
  if (_value >= 0)
    return;
  _value = abs(_value);
  if (_comparison == comparison::greater_than) {
    _comparison = comparison::less_than;
  } else if (_comparison == comparison::greater_than_or_equal) {
    _comparison = comparison::less_than_or_equal;
  } else if (_comparison == comparison::less_than) {
    _comparison = comparison::greater_than;
  } else if (_comparison == comparison::less_than_or_equal) {
    _comparison = comparison::greater_than_or_equal;
  }
}

/*************************************************************************
 *                                                                       *
 *                          label_compare_to_string                      *
 *                                                                       *
 *************************************************************************/

/**
 * @brief dump object to std::ostream
 *
 * @param s
 */
template <typename char_t>
void label_compare_to_string<char_t>::dump(std::ostream& s) const {
  s << ' ' << " { " << _label << ' ';

  switch (_comparison) {
    case string_comparison::equal:
      s << "==";
      break;
    case string_comparison::not_equal:
      s << "!=";
      break;
    default:
      s << "unknown comparison";
      break;
  }

  if constexpr (std::is_same_v<char, char_t>) {
    s << _value;
  } else {
    std::string conv;
    wstring_to_string(_value, &conv);
    s << conv;
  }
  s << " } " << std::endl;
}

/*************************************************************************
 *                                                                       *
 *                               label_in                                *
 *                                                                       *
 *************************************************************************/

/**
 * @brief Construct a new label in::label in object
 *
 * @param label id of the data to test
 * @param rule in or not_in
 * @param values allowed or forbidden values
 */
template <typename char_t>
label_in<char_t>::label_in(std::string&& label,
                           in_not rule,
                           std::vector<std::string>&& values)
    : filter(filter_type::label_in), _label(std::move(label)), _rule(rule) {
  if constexpr (std::is_same_v<char_t, char>) {
    for (auto&& value : values) {
      _values.insert(std::move(value));
    }
  } else {
    for (const std::string& value : values) {
      _values.emplace(value.begin(), value.end());
    }
  }
}

/**
 * @brief dump object to stream s
 *
 * @param s
 */
template <typename char_t>
void label_in<char_t>::dump(std::ostream& s) const {
  s << " { " << _label << ' ' << (_rule == in_not::in ? "in" : "not_in")
    << " (";
  if constexpr (std::is_same_v<char_t, char>) {
    for (const auto& value : _values) {
      s << value << ", ";
    }
  } else {
#pragma warning(push)
#pragma warning(disable : 4244)
    for (const auto& value : _values) {
      s << std::string(value.begin(), value.end()) << ", ";
    }
#pragma warning(pop)
  }

  s << ") }";
}

/*************************************************************************
 *                                                                       *
 *                          filter_combinator                            *
 *                                                                       *
 *************************************************************************/

/**
 * @brief Copy Constructor
 *
 * @param other
 */
filter_combinator::filter_combinator(const filter_combinator& other)
    : filter(filter_type::filter_combinator) {
  _logical = other._logical;
  for (auto iter = other._filters.begin(); iter != other._filters.end();
       ++iter) {
    _filters.push_back((*iter)->clone());
  }
}

/**
 * @brief Another Copy Constructor
 *
 * @param other
 */
filter_combinator::filter_combinator(filter_combinator& other)
    : filter_combinator(static_cast<const filter_combinator&>(other)) {}

/**
 * @brief = copy
 *
 * @param other
 * @return filter_combinator&
 */
filter_combinator& filter_combinator::operator=(
    const filter_combinator& other) {
  if (this != &other) {
    _logical = other._logical;
    for (auto iter = other._filters.begin(); iter != other._filters.end();
         ++iter) {
      _filters.push_back((*iter)->clone());
    }
  }
  return *this;
}

/**
 * @brief use sub filter check results and perform logical operation
 *
 * @param t
 */
bool filter_combinator::check(const testable& t) const {
  if (_logical == logical_operator::filter_and) {
    for (auto& subfilter : _filters) {
      if (subfilter->get_enabled() && !subfilter->check(t)) {
        return false;
      }
    }
    return true;
  } else {
    for (auto& subfilter : _filters) {
      if (subfilter->get_enabled() && subfilter->check(t)) {
        return true;
      }
    }
    return false;
  }
}

/**
 * @brief visit all subfilters
 */
void filter_combinator::visit(const visitor& visitr) const {
  for (auto& subfilter : _filters) {
    subfilter->visit(visitr);
  }
}

/**
 * @brief set logger for all subfilters
 *
 * @param logger
 */
void filter_combinator::set_logger(
    const std::shared_ptr<spdlog::logger>& logger) {
  filter::set_logger(logger);
  for (auto& subfilter : _filters) {
    subfilter->set_logger(logger);
  }
}

/**
 * @brief apply checker to all subfilters
 *
 * @param checker_builder
 */
void filter_combinator::apply_checker(const checker_builder& checker_builder) {
  for (auto& subfilter : _filters) {
    subfilter->apply_checker(checker_builder);
  }
}

/**
 * @brief dump object and subobjects
 *
 * @param s
 */
void filter_combinator::dump(std::ostream& s) const {
  bool first = true;
  s << " ( ";
  for (auto iter = _filters.begin(); iter != _filters.end(); ++iter) {
    if (first) {
      first = false;
    } else {
      if (_logical == logical_operator::filter_and) {
        s << " && ";
      } else {
        s << " || ";
      }
    }
    (*iter)->dump(s);
  }
  s << " ) ";
}

namespace com::centreon::agent::filters {
template class label_in<char>;
template class label_in<wchar_t>;
template class label_compare_to_string<char>;
template class label_compare_to_string<wchar_t>;
}  // namespace com::centreon::agent::filters