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

#ifndef CENTREON_AGENT_FILTER_HH
#define CENTREON_AGENT_FILTER_HH

#include <tuple>
#include <type_traits>
#include <variant>
namespace com::centreon::agent {

struct testable {};

using checker = std::function<bool(const testable&)>;

/**
 * @brief abstract filter base class
 *
 */
class filter {
 public:
  enum class filter_type : int { label_compare_to_value, filter_combinator };

 private:
  checker _checker;
  filter_type _type;

 public:
  filter(filter_type type) : _type(type) {}
  virtual ~filter() = default;

  filter_type get_type() const { return _type; }

  template <class checker_class>
  void apply_checker(checker_class&& checker) {
    _checker = std::forward<checker_class>(checker);
  }

  virtual void dump(std::ostream& s) const = 0;

  virtual filter* clone() const = 0;

  static filter parse(const std::string_view& filter);

  bool check(const testable& t) const { return _checker(t); }
};

}  // namespace com::centreon::agent

namespace std {
std::ostream& operator<<(std::ostream& s,
                         const com::centreon::agent::filter& filt);
}  // namespace std

namespace com::centreon::agent {

namespace filters {
class label_compare_to_value : public filter {
 public:
  enum class comparison : int {
    equal,
    not_equal,
    greater_than,
    greater_than_or_equal,
    less_than,
    less_than_or_equal
  };

 private:
  std::string _label;
  double _value;
  std::string _unit;

  comparison _comparison;

 public:
  label_compare_to_value(std::string&& label,
                         comparison compare,
                         double value,
                         std::string&& unit);

  label_compare_to_value(double value,
                         std::string&& unit,
                         comparison compare,
                         std::string&& label)
      : filter(filter_type::label_compare_to_value),
        _label(std::move(label)),
        _value(value),
        _unit(std::move(unit)),
        _comparison(compare) {
    dump(std::cout);
  }

  label_compare_to_value() : filter(filter_type::label_compare_to_value) {}

  filter* clone() const override { return new label_compare_to_value(*this); }

  void dump(std::ostream& s) const override;

  const std::string& get_label() const { return _label; }
  double get_value() const { return _value; }
  const std::string& get_unit() const { return _unit; }
  comparison get_comparison() const { return _comparison; }
};

class filter_combinator : public filter {
 public:
  enum class logical_operator : int { filter_and, filter_or };

  using compar_combi = std::variant<label_compare_to_value, filter_combinator>;
  using ope_compar_combi = std::tuple<logical_operator, compar_combi>;
  using several_ope_compar_combi =
      std::tuple<compar_combi, std::vector<ope_compar_combi>>;

 private:
  logical_operator _logical;

  using filter_vector = std::vector<filter*>;
  filter_vector _filters;

  filter_combinator(logical_operator ope, filter_vector&& sub_filters)
      : filter(filter_type::filter_combinator),
        _logical(ope),
        _filters(std::move(sub_filters)) {}

  filter* _move_filter(filter_combinator&& filt) {
    if (filt._filters.size() == 1) {
      filter* ret = filt._filters[0];
      filt._filters.clear();
      return ret;
    }
    return new filter_combinator(std::move(filt));
  }
  filter* _move_filter(label_compare_to_value&& filt) {
    return new label_compare_to_value(std::move(filt));
  }
  filter* _move_filter(
      std::variant<label_compare_to_value, filter_combinator>&& filt) {
    return std::visit(
        [this](auto&& arg) { return _move_filter(std::move(arg)); }, filt);
  }

 public:
  filter_combinator() : filter(filter_type::filter_combinator) {}
  filter_combinator(const filter_combinator&);
  filter_combinator(filter_combinator&);
  filter_combinator(filter_combinator&&) = default;
  filter_combinator& operator=(const filter_combinator&);
  filter_combinator& operator=(filter_combinator&&) = default;

  template <typename T>
  filter_combinator(T&& sub_filters);

  ~filter_combinator();

  filter* clone() const override { return new filter_combinator(*this); }
  void dump(std::ostream& s) const override;
};

template <typename T>
filter_combinator::filter_combinator(T&& sub_filters)
    : filter(filter_type::filter_combinator) {
  // as and has not the same priority as or, we need to handle it
  // so we will group all ands together and this will be an or on these ands

  std::vector<std::vector<filter*>> ands;
  std::vector<filter*> ors;

  filter* previous = _move_filter(std::move(std::get<0>(sub_filters)));

  if (std::get<1>(sub_filters).empty()) {
    if (previous->get_type() == filter::filter_type::filter_combinator) {
      _filters = std::move(static_cast<filter_combinator*>(previous)->_filters);
      _logical = static_cast<filter_combinator*>(previous)->_logical;
      delete previous;
    } else {
      _filters.push_back(previous);
      _logical = logical_operator::filter_or;
    }
    return;
  }
  bool parsing_and = false;
  for (auto follow_iter = std::get<1>(sub_filters).begin();
       follow_iter != std::get<1>(sub_filters).end(); ++follow_iter) {
    if (std::get<0>(*follow_iter) == logical_operator::filter_and) {
      if (parsing_and) {  // yet and construct
        ands.rbegin()->push_back(previous);
      } else {  // or => and
        parsing_and = true;
        ands.resize(ands.size() + 1);
        ands.rbegin()->push_back(previous);
      }
    } else {
      if (parsing_and) {  // and => or
        parsing_and = false;
        ands.rbegin()->push_back(previous);
      } else {
        ors.push_back(previous);
      }
    }
    previous = _move_filter(std::move(std::get<1>(*follow_iter)));
  }
  // the last one
  auto last = std::get<1>(sub_filters).rbegin();
  if (std::get<0>(*last) == logical_operator::filter_and) {
    ands.rbegin()->push_back(previous);
  } else {
    ors.push_back(previous);
  }

  if (ors.empty()) {
    _filters = std::move(*ands.begin());
    _logical = logical_operator::filter_and;
  } else {
    _filters = std::move(ors);
    _logical = logical_operator::filter_or;
    for (auto&& and_unit : ands) {
      _filters.push_back(new filter_combinator(logical_operator::filter_and,
                                               std::move(and_unit)));
    }
  }

  dump(std::cout);
}

}  // namespace filters

}  // namespace com::centreon::agent

#endif
