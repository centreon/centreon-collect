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

#include <absl/container/flat_hash_set.h>
#include <memory>
#include <tuple>
#include <type_traits>
#include <variant>
namespace com::centreon::agent {

/**
 * @brief this abstract struct will be used to pass datas to the check process
 * it contains nothing in order to be used by any check on any data
 *
 */
struct testable {};

/**
 * @brief a checker is a function that will check if a testable object is
 * matching the filter
 *
 */
using checker = std::function<bool(const testable&)>;

class filter;

/**
 * @brief a checker_builder is a function that will build a checker from a
 * filter built checker will be stored in filter to perform checks
 *
 */
using checker_builder = std::function<checker(const filter*)>;

/**
 * @brief abstract filter base class
 *
 */
class filter {
 public:
  enum class filter_type : int {
    label_compare_to_value,
    label_in,
    filter_combinator
  };

 private:
  filter_type _type;

 public:
  filter(filter_type type) : _type(type) {}
  filter(const filter&) = default;
  virtual ~filter() = default;

  filter& operator=(const filter&) = default;

  filter_type get_type() const { return _type; }

  virtual void apply_checker(const checker_builder& checker_builder) = 0;

  virtual void dump(std::ostream& s) const = 0;

  virtual std::unique_ptr<filter> clone() const = 0;

  virtual bool check(const testable& t) const = 0;
};

}  // namespace com::centreon::agent

namespace std {
std::ostream& operator<<(std::ostream& s,
                         const com::centreon::agent::filter& filt);
}  // namespace std

namespace com::centreon::agent {

/**
 * @brief The filters namespace contains classes for filtering data based on
 * various criteria.
 */
namespace filters {

/**
 * @brief The goal of this filter is to check a double threshold of a label
 * It embeds label, threshold, unit and comparison
 * example: foo > 10.0
 */
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

  checker _checker;

 public:
  label_compare_to_value(double value,
                         std::string&& unit,
                         comparison compare,
                         std::string&& label);

  label_compare_to_value(std::string&& label,
                         comparison compare,
                         double value,
                         std::string&& unit)
      : filter(filter_type::label_compare_to_value),
        _label(std::move(label)),
        _value(value),
        _unit(std::move(unit)),
        _comparison(compare) {
    dump(std::cout);
  }

  label_compare_to_value() : filter(filter_type::label_compare_to_value) {}

  std::unique_ptr<filter> clone() const override {
    return std::make_unique<label_compare_to_value>(*this);
  }

  void dump(std::ostream& s) const override;

  const std::string& get_label() const { return _label; }
  double get_value() const { return _value; }
  const std::string& get_unit() const { return _unit; }
  comparison get_comparison() const { return _comparison; }

  bool check(const testable& t) const override { return _checker(t); }
  void apply_checker(const checker_builder& checker_builder) override {
    _checker = checker_builder(this);
  }
};

/**
 * @brief test if a value is in a set of values
 * example: foo in (titi, 'tutu', tata)
 *
 */
class label_in : public filter {
 public:
  enum class in_not { in, not_in };

 private:
  absl::flat_hash_set<std::string> _values;
  std::string _label;
  in_not _rule;

  checker _checker;

 public:
  label_in() : filter(filter_type::label_in) {}

  label_in(std::string&& label, in_not rule, std::vector<std::string>&& values);

  const absl::flat_hash_set<std::string>& get_values() const { return _values; }
  const std::string& get_label() const { return _label; }
  in_not get_rule() const { return _rule; }

  std::unique_ptr<filter> clone() const override {
    return std::make_unique<label_in>(*this);
  }

  void dump(std::ostream& s) const override;

  bool check(const testable& t) const override { return _checker(t); }
  void apply_checker(const checker_builder& checker_builder) override {
    _checker = checker_builder(this);
  }
};

/**
 * @brief the glue between filters
 * it will be used to combine filters with logical operators and, or, && ,|| and
 * () Example: foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux
 * in(truc, 'machin')
 *
 */
class filter_combinator : public filter {
 public:
  enum class logical_operator : int { filter_and, filter_or };

  using filter_ptr = std::unique_ptr<filter>;

 private:
  logical_operator _logical;

  using filter_vector = std::vector<filter_ptr>;
  filter_vector _filters;

  filter_ptr _move_filter(filter_combinator&& filt) {
    if (filt._filters.size() == 1) {
      filter_ptr ret = std::move(filt._filters[0]);
      filt._filters.clear();
      return ret;
    }
    return std::make_unique<filter_combinator>(std::move(filt));
  }
  filter_ptr _move_filter(label_compare_to_value&& filt) {
    return std::make_unique<label_compare_to_value>(std::move(filt));
  }
  filter_ptr _move_filter(label_in&& filt) {
    return std::make_unique<label_in>(std::move(filt));
  }

  filter_ptr _move_filter(
      std::variant<label_compare_to_value, label_in, filter_combinator>&&
          filt) {
    return std::visit(
        [this](auto&& arg) { return _move_filter(std::move(arg)); }, filt);
  }

 public:
  filter_combinator() : filter(filter_type::filter_combinator) {}
  filter_combinator(const filter_combinator&);
  filter_combinator(filter_combinator&);
  filter_combinator(filter_combinator&&) = default;

  filter_combinator(logical_operator ope, filter_vector&& sub_filters)
      : filter(filter_type::filter_combinator),
        _logical(ope),
        _filters(std::move(sub_filters)) {}

  filter_combinator& operator=(const filter_combinator&);

  template <typename T>
  filter_combinator(T&& sub_filters);

  std::unique_ptr<filter> clone() const override {
    return std::make_unique<filter_combinator>(*this);
  }

  bool check(const testable& t) const override;
  void apply_checker(const checker_builder& checker_builder) override;

  void dump(std::ostream& s) const override;
};

template <typename T>
filter_combinator::filter_combinator(T&& sub_filters)
    : filter(filter_type::filter_combinator) {
  // as and has not the same priority as or, we need to handle it
  // so we will group all ands together and this will be an or on these ands

  std::vector<std::vector<filter_ptr>> ands;
  std::vector<filter_ptr> ors;

  filter_ptr previous = _move_filter(std::move(std::get<0>(sub_filters)));

  if (std::get<1>(sub_filters).empty()) {
    if (previous->get_type() == filter::filter_type::filter_combinator) {
      _filters = std::move(static_cast<filter_combinator&>(*previous)._filters);
      _logical = static_cast<filter_combinator&>(*previous)._logical;
    } else {
      _filters.emplace_back(std::move(previous));
      _logical = logical_operator::filter_or;
    }
    return;
  }
  bool parsing_and = false;
  for (auto follow_iter = std::get<1>(sub_filters).begin();
       follow_iter != std::get<1>(sub_filters).end(); ++follow_iter) {
    if (std::get<0>(*follow_iter) == logical_operator::filter_and) {
      if (parsing_and) {  // yet and construct
        ands.rbegin()->emplace_back(std::move(previous));
      } else {  // or => and
        parsing_and = true;
        ands.resize(ands.size() + 1);
        ands.rbegin()->emplace_back(std::move(previous));
      }
    } else {
      if (parsing_and) {  // and => or
        parsing_and = false;
        ands.rbegin()->emplace_back(std::move(previous));
      } else {
        ors.emplace_back(std::move(previous));
      }
    }
    previous = _move_filter(std::move(std::get<1>(*follow_iter)));
  }
  // the last one
  auto last = std::get<1>(sub_filters).rbegin();
  if (std::get<0>(*last) == logical_operator::filter_and) {
    ands.rbegin()->emplace_back(std::move(previous));
  } else {
    ors.emplace_back(std::move(previous));
  }

  if (ors.empty()) {
    _filters = std::move(*ands.begin());
    _logical = logical_operator::filter_and;
  } else {
    _filters = std::move(ors);
    _logical = logical_operator::filter_or;
    for (auto&& and_unit : ands) {
      _filters.push_back(std::make_unique<filter_combinator>(
          logical_operator::filter_and, std::move(and_unit)));
    }
  }

  dump(std::cout);
}

}  // namespace filters

}  // namespace com::centreon::agent

#endif
