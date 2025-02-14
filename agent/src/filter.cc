/**
 * Copyright 2024 Centreon
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
#include <type_traits>

using namespace com::centreon::agent;
using namespace com::centreon::agent::filters;

namespace std {
std::ostream& operator<<(std::ostream& s,
                         const com::centreon::agent::filter& filt) {
  filt.dump(s);
  return s;
}
}  // namespace std

/*************************************************************************
 *                                                                       *
 *                          label_compare_to_value                        *
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

bool filter_combinator::check(testable& t) const {
  if (_logical == logical_operator::filter_and) {
    for (auto& subfilter : _filters) {
      if (!subfilter->check(t)) {
        return false;
      }
    }
    return true;
  } else {
    for (auto& subfilter : _filters) {
      if (subfilter->check(t)) {
        return true;
      }
    }
    return false;
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
}  // namespace com::centreon::agent::filters