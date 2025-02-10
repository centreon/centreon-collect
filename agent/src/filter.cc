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

using namespace com::centreon::agent;
using namespace com::centreon::agent::filters;

namespace std {
std::ostream& operator<<(std::ostream& s,
                         const com::centreon::agent::filter& filt) {
  filt.dump(s);
  return s;
}
}  // namespace std

label_compare_to_value::label_compare_to_value(std::string&& label,
                                               comparison compare,
                                               double value,
                                               std::string&& unit)
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

void label_compare_to_value::dump(std::ostream& s) const {
  s << ' ' << " { " << _value << ' ' << _unit << ' ';

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

  s << ' ' << _label << " } " << std::endl;
}

filter_combinator::~filter_combinator() {
  for (auto iter = _filters.begin(); iter != _filters.end(); ++iter) {
    delete *iter;
  }
}

filter_combinator::filter_combinator(const filter_combinator& other)
    : filter(filter_type::filter_combinator) {
  _logical = other._logical;
  for (auto iter = other._filters.begin(); iter != other._filters.end();
       ++iter) {
    _filters.push_back((*iter)->clone());
  }
}

filter_combinator::filter_combinator(filter_combinator& other)
    : filter_combinator(static_cast<const filter_combinator&>(other)) {}

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
