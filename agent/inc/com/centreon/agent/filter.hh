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

#include "com/centreon/exceptions/msg_fmt.hh"
#include "filter.hh"
namespace com::centreon::agent {

namespace filters {
class filter_combinator;
};

void wstring_to_string(std::wstring_view in_str, std::string* out_str);
void string_to_wstring(std::string_view in_str, std::wstring* out_str);

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
using checker_builder = std::function<void(filter*)>;
using visitor = std::function<void(const filter*)>;

/**
 * @brief abstract filter base class
 *
 */
class filter {
 public:
  enum class filter_type : int {
    label_compare_to_value,
    label_compare_to_string,
    label_in,
    filter_combinator
  };

 private:
  filter_type _type;

 protected:
  checker _checker;

 public:
  filter(filter_type type) : _type(type) {}
  filter(const filter&) = default;
  virtual ~filter() = default;

  filter& operator=(const filter&) = default;

  filter_type get_type() const { return _type; }

  virtual void dump(std::ostream& s) const = 0;

  virtual std::unique_ptr<filter> clone() const = 0;

  virtual bool check(const testable& t) const { return _checker(t); }

  virtual void visit(const visitor& visitr) const { visitr(this); }

  virtual void apply_checker(const checker_builder& checker_builder) {
    checker_builder(this);
  }

  template <class checker_ope>
  void set_checker(checker_ope&& ope) {
    _checker = std::forward<checker_ope>(ope);
  }

  static bool create_filter(std::string_view filter_str,
                            const std::shared_ptr<spdlog::logger>& logger,
                            filters::filter_combinator* filter,
                            bool use_wchar = false,
                            bool debug = false);
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

/*************************************************************************
 *                                                                       *
 *                          label_compare_to_value                       *
 *                                                                       *
 *************************************************************************/

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
        _comparison(compare) {}

  label_compare_to_value() : filter(filter_type::label_compare_to_value) {}

  std::unique_ptr<filter> clone() const override {
    return std::make_unique<label_compare_to_value>(*this);
  }

  void dump(std::ostream& s) const override;

  const std::string& get_label() const { return _label; }
  double get_value() const { return _value; }
  const std::string& get_unit() const { return _unit; }
  comparison get_comparison() const { return _comparison; }

  void change_threshold_to_abs();

  void calc_duration();

  template <class value_getter>
  void set_checker_from_getter(value_getter&& getter);
};

/**
 * @brief this helper function will create a checker from a getter
 *
 * @tparam value_getter operator() must return a double
 * @param getter lambda or struct with operator() returning a double
 */
template <class value_getter>
void label_compare_to_value::set_checker_from_getter(value_getter&& getter) {
  switch (_comparison) {
    case comparison::equal:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val == gettr(t); };
      break;
    case comparison::not_equal:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val != gettr(t); };
      break;
    case comparison::greater_than:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val < gettr(t); };
      break;
    case comparison::greater_than_or_equal:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val <= gettr(t); };
      break;
    case comparison::less_than:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val > gettr(t); };
      break;
    case comparison::less_than_or_equal:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val >= gettr(t); };
      break;
  }
}

/*************************************************************************
 *                                                                       *
 *                          label_compare_to_string                      *
 *                                                                       *
 *************************************************************************/

enum class string_comparison : int { equal, not_equal };

/**
 * @brief The goal of this filter is to check a string equal or not equal
 * values must be quoted
 * example: foo == 'tutu'
 */
template <typename char_t>
class label_compare_to_string : public filter {
 public:
 private:
  std::string _label;
  std::basic_string<char_t> _value;

  string_comparison _comparison;

 public:
  label_compare_to_string(std::string&& label,
                          string_comparison compare,
                          std::string&& value)
      : filter(filter_type::label_compare_to_string),
        _label(std::move(label)),
        _comparison(compare) {
    if constexpr (std::is_same_v<char, char_t>) {
      _value = std::move(value);
    } else {
      string_to_wstring(value, &_value);
    }
  }

  label_compare_to_string() : filter(filter_type::label_compare_to_string) {}

  std::unique_ptr<filter> clone() const override {
    return std::make_unique<label_compare_to_string<char_t>>(*this);
  }

  void dump(std::ostream& s) const override;

  const std::string& get_label() const { return _label; }
  const std::basic_string<char_t>& get_value() const { return _value; }
  string_comparison get_comparison() const { return _comparison; }

  template <class value_getter>
  void set_checker_from_getter(value_getter&& getter);
};

/**
 * @brief this helper function will create a checker from a getter
 *
 * @tparam value_getter operator() must return a double
 * @param getter lambda or struct with operator() returning a double
 */
template <typename char_t>
template <class value_getter>
void label_compare_to_string<char_t>::set_checker_from_getter(
    value_getter&& getter) {
  static_assert(
      std::is_same_v<char_t, typename std::invoke_result_t<
                                 value_getter, const testable&>::value_type>,
      "label_compare_to_string: char types are different");
  switch (_comparison) {
    case string_comparison::equal:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val == gettr(t); };
      break;
    case string_comparison::not_equal:
      _checker = [val = _value, gettr = std::move(getter)](
                     const testable& t) -> bool { return val != gettr(t); };
      break;
  }
}

/*************************************************************************
 *                                                                       *
 *                               label_in                                *
 *                                                                       *
 *************************************************************************/

enum class in_not { in, not_in };

/**
 * @brief test if a value is in a set of values
 * example: foo in (titi, 'tutu', tata)
 *
 */
template <typename char_t>
class label_in : public filter {
 public:
  using char_type = char_t;
  using string_type = std::basic_string<char_t>;

 private:
  absl::flat_hash_set<string_type> _values;
  std::string _label;
  in_not _rule;

 public:
  label_in() : filter(filter_type::label_in) {}

  label_in(std::string&& label, in_not rule, std::vector<std::string>&& values);

  const absl::flat_hash_set<string_type>& get_values() const { return _values; }
  const std::string& get_label() const { return _label; }
  in_not get_rule() const { return _rule; }

  std::unique_ptr<filter> clone() const override {
    return std::make_unique<label_in>(*this);
  }

  void dump(std::ostream& s) const override;

  template <class value_getter>
  void set_checker_from_getter(value_getter&& getter);

  template <class number_getter>
  void set_checker_from_number_getter(number_getter&& getter);
};

/**
 * @brief this helper function will create a checker from a getter
 *
 * @tparam value_getter return a string
 * @param getter
 */
template <typename char_t>
template <class value_getter>
void label_in<char_t>::set_checker_from_getter(value_getter&& getter) {
  static_assert(
      std::is_same_v<char_t, typename std::invoke_result_t<
                                 value_getter, const testable&>::value_type>,
      "label_in: char types are different");
  if (_rule == in_not::in) {
    _checker = [values = &_values,
                gettr = std::move(getter)](const testable& t) -> bool {
      auto to_test = gettr(t);
      return values->contains(to_test);
    };
  } else
    _checker = [values = &_values,
                gettr = std::move(getter)](const testable& t) -> bool {
      return !values->contains(gettr(t));
    };
}

template <typename number_type, in_not rule, typename getter_type>
class numeric_in {
  std::set<number_type> _values;
  getter_type _getter;

 public:
  template <class label_in_class>
  numeric_in(const label_in_class& parent, getter_type&& value_getter)
      : _getter(std::move(value_getter)) {
    if constexpr (std::is_same_v<char, typename label_in_class::char_type>) {
      for (const auto& val : parent.get_values()) {
        number_type to_ins;
        if (!absl::SimpleAtoi(val, &to_ins)) {
          throw exceptions::msg_fmt("{} unable to convert '{}' to a number",
                                    parent.get_label(), val);
        }
        _values.insert(to_ins);
      }
    } else {
      std::string conv;
      for (const auto& val : parent.get_values()) {
        wstring_to_string(val, &conv);
        number_type to_ins;
        if (!absl::SimpleAtoi(conv, &to_ins)) {
          throw exceptions::msg_fmt("{} unable to convert '{}' to a number",
                                    parent.get_label(), conv);
        }
        _values.insert(to_ins);
      }
    }
  }

  bool operator()(const testable& t) const {
    if constexpr (rule == in_not::in) {
      return _values.find(_getter(t)) != _values.end();
    } else {
      return _values.find(_getter(t)) == _values.end();
    }
  }
};

template <typename char_t>
template <class number_getter>
void label_in<char_t>::set_checker_from_number_getter(number_getter&& getter) {
  if (_rule == in_not::in) {
    _checker = numeric_in<std::invoke_result_t<number_getter, const testable&>,
                          in_not::in, number_getter>(*this, std::move(getter));
  } else {
    _checker =
        numeric_in<std::invoke_result_t<number_getter, const testable&>,
                   in_not::not_in, number_getter>(*this, std::move(getter));
  }
}

/*************************************************************************
 *                                                                       *
 *                          filter_combinator                            *
 *                                                                       *
 *************************************************************************/

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

  template <typename char_t>
  filter_ptr _move_filter(label_in<char_t>&& filt) {
    return std::make_unique<label_in<char_t>>(std::move(filt));
  }

  template <typename char_t>
  filter_ptr _move_filter(label_compare_to_string<char_t>&& filt) {
    return std::make_unique<label_compare_to_string<char_t>>(std::move(filt));
  }

  template <typename char_t>
  filter_ptr _move_filter(std::variant<label_compare_to_value,
                                       label_compare_to_string<char_t>,
                                       label_in<char_t>,
                                       filter_combinator>&& filt) {
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

  void visit(const visitor& visitr) const override;

  void apply_checker(const checker_builder& checker_builder) override;

  void dump(std::ostream& s) const override;
};

/**
 * @brief constructor called by boost::parse
 * @param sub_filters a tuple containing the first filter(variant) and a vector
 * of tuples (operator, variant)
 */
template <typename T>
filter_combinator::filter_combinator(T&& sub_filters)
    : filter(filter_type::filter_combinator) {
  // as and has not the same priority as or, we need to handle it
  // so we will group all ands together and this will be an or on these ands

  std::vector<std::vector<filter_ptr>> ands;
  std::vector<filter_ptr> ors;

  // we will work on previous as operator is contained in the second argument
  filter_ptr previous = _move_filter(std::move(std::get<0>(sub_filters)));

  // only one filter => no need of a combinator
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

  if (ors.empty() && ands.size() == 1) {  // ony one and
    _filters = std::move(*ands.begin());
    _logical = logical_operator::filter_and;
  } else {  // several ors that may contain some ands
    _filters = std::move(ors);
    _logical = logical_operator::filter_or;
    for (auto&& and_unit : ands) {
      _filters.push_back(std::make_unique<filter_combinator>(
          logical_operator::filter_and, std::move(and_unit)));
    }
  }
}

}  // namespace filters

}  // namespace com::centreon::agent

#endif
