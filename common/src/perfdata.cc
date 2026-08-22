/**
 * Copyright 2011-2013,2017-2021 Centreon
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

#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string_view>
#include <cmath>

#include "perfdata.hh"

using namespace com::centreon::common;

/**
 *  Default constructor.
 */
perfdata::perfdata()
    : _critical(NAN),
      _critical_low(NAN),
      _critical_mode(false),
      _max(NAN),
      _min(NAN),
      _value(NAN),
      _value_type(gauge),
      _warning(NAN),
      _warning_low(NAN),
      _warning_mode(false) {}

/**
 *  Comparison helper.
 *
 *  @param[in] a First value.
 *  @param[in] b Second value.
 *
 *  @return true if a and b are equal.
 */
static inline bool float_equal(float a, float b) {
  return (std::isnan(a) && std::isnan(b)) ||
         (std::isinf(a) && std::isinf(b) &&
          std::signbit(a) == std::signbit(b)) ||
         (std::isfinite(a) && std::isfinite(b) &&
          fabs(a - b) <= 0.01 * fabs(a));
}

namespace com::centreon::common {
/**
 *  Compare two perfdata objects.
 *
 *  @param[in] left  First object.
 *  @param[in] right Second object.
 *
 *  @return true if both objects are equal.
 */
bool operator==(perfdata const& left, perfdata const& right) {
  return float_equal(left.critical(), right.critical()) &&
         float_equal(left.critical_low(), right.critical_low()) &&
         left.critical_mode() == right.critical_mode() &&
         float_equal(left.max(), right.max()) &&
         float_equal(left.min(), right.min()) && left.name() == right.name() &&
         left.unit() == right.unit() &&
         float_equal(left.value(), right.value()) &&
         left.value_type() == right.value_type() &&
         float_equal(left.warning(), right.warning()) &&
         float_equal(left.warning_low(), right.warning_low()) &&
         left.warning_mode() == right.warning_mode();
}

/**
 *  Compare two perfdata objects.
 *
 *  @param[in] left  First object.
 *  @param[in] right Second object.
 *
 *  @return true if both objects are inequal.
 */
bool operator!=(perfdata const& left, perfdata const& right) {
  return !(left == right);
}

}  // namespace com::centreon::common

/**
 * @brief in case of db insertions we need to ensure that name can be stored in
 * table With it, you can reduce name size
 *
 * @param new_size
 */
void perfdata::resize_name(size_t new_size) {
  _name.resize(new_size);
}

/**
 * @brief idem of resize_name
 *
 * @param new_size
 */
void perfdata::resize_unit(size_t new_size) {
  _unit.resize(new_size);
}
namespace {

/* The grammar this parser implements, as the previous one did and as the unit
 * tests pin it down:
 *
 *   output   := blank* metric (blank+ metric)* blank*
 *   metric   := name '=' value unit? (';' warn (';' crit (';' min (';' max)?)?)?)?
 *   name     := quoted | plain | typed
 *   quoted   := "'" anything-but-a-quote "'"      -- may contain blanks
 *   typed    := ('a'|'c'|'d'|'g') '[' label ']'   -- lowercase only
 *   range    := '@'? ('~' | number) (':' number?)?
 *   number   := decimal, comma accepted as the decimal separator,
 *               'nan' and 'inf' accepted, sign optional
 *
 * Everything below works on std::string_view. The parser used to walk a
 * NUL-terminated char* with strchr, strspn and strcspn, and locating the decimal
 * comma with an unbounded strchr cost it dearly: seven calls per metric, each
 * reading to the end of the whole output. Measured before the rewrite, on a
 * 3000-metric output: 2240 ns per metric where an output whose numbers all
 * carried a comma -- so where the search stopped immediately -- cost 286 ns.
 * The cost was the distance to the next comma, not the comma.
 *
 * Hence the one rule that keeps this file honest: every search is bounded by the
 * field it belongs to. A metric is delimited once, its fields are split on ';'
 * once, and nothing ever looks past them.
 */

/**
 * @brief Tell whether a byte is an ASCII blank.
 *
 * absl rather than isspace(): the latter takes an int and is undefined for a
 * negative char, which is every byte above 127 on this platform -- and metric
 * names do carry UTF-8. The previous parser worked around that with an explicit
 * "byte >= 128" clause in its name scan.
 *
 * @param c The byte.
 *
 * @return True if c is a space, a tab, a newline, a carriage return, a form
 * feed or a vertical tab.
 */
inline bool is_blank(char c) {
  return absl::ascii_isspace(static_cast<unsigned char>(c));
}

/**
 * @brief Remove the leading and trailing blanks of a view.
 *
 * @param v The view.
 *
 * @return The trimmed view, which may be empty.
 */
/* Not absl::StripAsciiWhitespace(), which does exactly this on exactly the same
 * character set -- and is 2.4 to 4.5 times slower. Measured on 2026-08-21 with
 * common/benchmark/ascii_trim.cc, which keeps the comparison: 0.43 ns against
 * 1.72 for a field with nothing to strip, 1.07 against 3.21 for blanks at both
 * ends. Abseil passes ascii_isspace to std::find_if_not as a callable, walks the
 * end with reverse iterators, and builds two intermediate views; the loop below
 * inlines the table lookup and mutates the view in place.
 *
 * The stake is a few percent of a parse -- trim runs some six times per metric,
 * out of about 225 ns -- so this is a small win kept for a documented reason,
 * not a claim that hand-written beats the library in general. */
inline std::string_view trim(std::string_view v) {
  while (!v.empty() && is_blank(v.front()))
    v.remove_prefix(1);
  while (!v.empty() && is_blank(v.back()))
    v.remove_suffix(1);
  return v;
}

/**
 * @brief The numeric prefix of a field, delimited.
 */
struct number_extent {
  /** Bytes making up the number, 0 if the field does not start with one. */
  size_t length;
  /** Offset of the decimal comma, npos if the separator was a '.' or absent. */
  size_t comma;
};

/**
 * @brief Delimit the longest numeric prefix of a field.
 *
 * Needed because the value and its unit are written with nothing in between --
 * "12.5MB" -- so the number has to be delimited before it can be converted.
 * Accepts what strtof accepted: an optional sign, digits with '.' or ',' as the
 * decimal separator, an optional exponent, and the words "inf", "infinity" and
 * "nan".
 *
 * The position of a decimal comma is reported along with the length, because
 * this scan is the one that reads the separator: to_float() would otherwise
 * search the same bytes a second time to find it, on every number, whereas
 * commas are emitted by nothing but a plugin running under a locale that uses
 * them.
 *
 * @param field The field, blanks already trimmed.
 *
 * @return The extent of the number.
 */
number_extent delimit_number(std::string_view field) {
  size_t i = 0;
  if (i < field.size() && (field[i] == '+' || field[i] == '-'))
    ++i;

  /* "inf", "infinity" and "nan", which the tests use and plugins emit. Compared
   * case-insensitively, as strtof did. */
  const std::string_view rest = field.substr(i);
  for (std::string_view word : {std::string_view("infinity"),
                                std::string_view("inf"),
                                std::string_view("nan")}) {
    if (rest.size() >= word.size() &&
        absl::EqualsIgnoreCase(rest.substr(0, word.size()), word))
      return {i + word.size(), std::string_view::npos};
  }

  size_t comma = std::string_view::npos;
  const size_t digits_begin = i;
  while (i < field.size() && absl::ascii_isdigit(field[i]))
    ++i;
  if (i < field.size() && (field[i] == '.' || field[i] == ',')) {
    if (field[i] == ',')
      comma = i;
    ++i;
    while (i < field.size() && absl::ascii_isdigit(field[i]))
      ++i;
  }
  if (i == digits_begin)
    return {0, std::string_view::npos};

  /* An exponent counts only if it is complete: "12e" is the number 12 followed
   * by the unit "e", which is what strtof made of it too. */
  if (i < field.size() && (field[i] == 'e' || field[i] == 'E')) {
    size_t j = i + 1;
    if (j < field.size() && (field[j] == '+' || field[j] == '-'))
      ++j;
    const size_t exponent_digits = j;
    while (j < field.size() && absl::ascii_isdigit(field[j]))
      ++j;
    if (j > exponent_digits)
      i = j;
  }
  return {i, comma};
}

/**
 * @brief Convert the numeric prefix of a field.
 *
 * @param[in]  field    The field, blanks already trimmed.
 * @param[out] consumed How many bytes the number took, 0 if there was none.
 *
 * @return The value, NaN if the field does not start with a number.
 */
float to_float(std::string_view field, size_t* consumed) {
  const number_extent extent = delimit_number(field);
  *consumed = extent.length;
  if (!extent.length)
    return NAN;

  std::string_view number = field.substr(0, extent.length);

  /* A decimal comma is rewritten into a dot, in a buffer on the stack: the
   * previous parser built a std::string reaching all the way to the comma it had
   * found, which is what made an output carrying a single far-away comma cost
   * more than quadratically. A number longer than this buffer is not a number,
   * so the fallback is to fail rather than to allocate. Declared outside the
   * branch because the view below outlives it. */
  std::array<char, 64> fixed;
  if (extent.comma != std::string_view::npos) {
    if (extent.length > fixed.size())
      return NAN;
    std::memcpy(fixed.data(), number.data(), extent.length);
    fixed[extent.comma] = '.';
    number = std::string_view(fixed.data(), extent.length);
  }

  float value;
  if (!absl::SimpleAtof(number, &value))
    return NAN;
  return value;
}

/**
 * @brief Parse a threshold range.
 *
 * The grammar is Nagios': an optional '@', then either '~' for minus infinity
 * or a number, then optionally ':' and another number. Its quirks are preserved
 * as they were, because the tests pin them and plugins rely on them:
 *
 * * without a ':', the number read is the *high* bound and the low one becomes
 *   0 -- unless it is NaN, in which case it stays NaN;
 * * with a ':' and nothing usable after it, the high bound is plus infinity.
 *
 * @param[in]  field     The range field, which may be empty.
 * @param[out] low       The low bound.
 * @param[out] high      The high bound.
 * @param[out] inclusive Whether the range was prefixed with '@'.
 */
void parse_range(std::string_view field,
                 float* low,
                 float* high,
                 bool* inclusive) {
  field = trim(field);
  *inclusive = false;
  if (!field.empty() && field.front() == '@') {
    *inclusive = true;
    field.remove_prefix(1);
  }

  float low_value;
  if (!field.empty() && field.front() == '~') {
    low_value = -std::numeric_limits<float>::infinity();
    field.remove_prefix(1);
  } else {
    size_t consumed = 0;
    low_value = to_float(field, &consumed);
    field.remove_prefix(consumed);
  }

  float high_value;
  if (field.empty() || field.front() != ':') {
    high_value = low_value;
    if (!std::isnan(low_value))
      low_value = 0.0f;
  } else {
    field.remove_prefix(1);
    size_t consumed = 0;
    high_value = to_float(field, &consumed);
    if (!consumed)
      high_value = std::numeric_limits<float>::infinity();
  }

  *low = low_value;
  *high = high_value;
}

/**
 * @brief Split off the next ';'-separated field of a metric.
 *
 * @param[in,out] fields What is left of the metric after the '='. Advanced past
 *                       the field returned, and past its separator.
 *
 * @return The field, which may be empty -- ";;" is two empty fields, and the
 * grammar allows them.
 */
std::string_view next_field(std::string_view* fields) {
  const size_t semicolon = fields->find(';');
  if (semicolon == std::string_view::npos) {
    const std::string_view field = *fields;
    *fields = std::string_view();
    return field;
  }
  const std::string_view field = fields->substr(0, semicolon);
  fields->remove_prefix(semicolon + 1);
  return field;
}

/**
 * @brief Strip the quotes and the type prefix of a metric name.
 *
 * @param[in,out] name The raw name, quotes and type prefix included.
 *
 * @return The data type the prefix announced, gauge if it announced none.
 */
perfdata::data_type strip_name(std::string_view* name) {
  /* One leading and one trailing quote, then blanks: a name written
   * "'  \n time'" keeps its inner blanks and loses its outer ones. */
  if (!name->empty() && name->front() == '\'')
    name->remove_prefix(1);
  if (!name->empty() && name->back() == '\'')
    name->remove_suffix(1);
  *name = trim(*name);

  /* Lowercase only, and only when the name ends with ']': "g[foo]" is a gauge
   * named foo, "G[foo]" is a metric named "G[foo]". That is what the previous
   * strncmp did, and widening it would silently rename metrics. */
  /* Three bytes is enough to strip: "g[]" leaves an empty name, which the caller
   * then rejects as a metric without a name -- which is what the previous parser
   * did with it too. Requiring four would have made "g[]=1" a metric literally
   * called "g[]". */
  if (name->size() >= 3 && name->back() == ']' && (*name)[1] == '[') {
    perfdata::data_type type;
    switch (name->front()) {
      case 'a':
        type = perfdata::data_type::absolute;
        break;
      case 'c':
        type = perfdata::data_type::counter;
        break;
      case 'd':
        type = perfdata::data_type::derive;
        break;
      case 'g':
        type = perfdata::data_type::gauge;
        break;
      default:
        return perfdata::data_type::gauge;
    }
    name->remove_prefix(2);
    name->remove_suffix(1);
    return type;
  }
  return perfdata::data_type::gauge;
}

/**
 * @brief Quote the first bytes of a view, for an error message.
 *
 * @param v     The view.
 * @param count How many bytes to show at most.
 *
 * @return A view of at most count bytes. The previous parser built a
 * fmt::string_view of ten bytes without checking that ten were left, and read
 * past the end of the output on a short one.
 */
std::string_view excerpt(std::string_view v, size_t count = 10) {
  return v.substr(0, std::min(count, v.size()));
}

}  // namespace

/**
 * @brief Parse perfdata string as given by plugin.
 *
 * @param host_id The host id of the service with this perfdata
 * @param service_id The service id of the service with this perfdata
 * @param str The perfdata string to parse
 * @param logger The logger to complain to
 *
 * @return A list of perfdata
 */
std::list<perfdata> perfdata::parse_perfdata(
    uint32_t host_id,
    uint32_t service_id,
    std::string_view str,
    const std::shared_ptr<spdlog::logger>& logger) {
  std::list<perfdata> retval;
  /* The names seen so far, as views into str: the caller's buffer outlives this
   * call, and nothing here copies a name twice. */
  absl::flat_hash_set<std::string_view> seen_names;

  auto id = [host_id, service_id] {
    if (host_id || service_id)
      return fmt::format("({}:{})", host_id, service_id);
    else
      return std::string();
  };

  std::string_view remaining = trim(str);
  logger->debug("storage: parsing service {} perfdata string '{}'", id(),
                remaining);

  while (!remaining.empty()) {
    /* The name, up to the '=' or a blank, quotes honoured: a quoted name may
     * contain both. */
    bool in_quote = false;
    size_t name_end = 0;
    while (name_end < remaining.size()) {
      const char c = remaining[name_end];
      if (c == '\'')
        in_quote = !in_quote;
      else if (!in_quote && (c == '=' || is_blank(c)))
        break;
      ++name_end;
    }

    std::string_view raw_name = remaining.substr(0, name_end);
    remaining.remove_prefix(name_end);

    perfdata p;
    std::string_view name = raw_name;
    p._value_type = strip_name(&name);

    bool error = false;
    if (name.empty()) {
      logger->error("In service {}, metric name empty before '{}...'", id(),
                    excerpt(raw_name));
      error = true;
    } else if (seen_names.contains(name)) {
      logger->warn(
          "storage: The metric '{}' appears several times in the output "
          "\"{}\": you will lose any new occurence of this metric",
          name, str);
      error = true;
    }

    /* Whatever the name was, the '=' has to be there. An output such as
     * "metric1= 10" -- a blank between the sign and the value -- is rejected as
     * it always was, and the metric after it is still read. */
    if (remaining.empty() || remaining.front() != '=') {
      logger->warn(
          "invalid perfdata format in service {}: equal sign not present or "
          "misplaced '{}'",
          id(), excerpt(remaining));
      error = true;
    } else {
      remaining.remove_prefix(1);
    }

    /* The whole metric, delimited once: everything below searches inside this
     * view and never past it. This is what the rewrite is about. */
    const size_t metric_end =
        std::find_if(remaining.begin(), remaining.end(), is_blank) -
        remaining.begin();
    std::string_view fields = remaining.substr(0, metric_end);
    remaining.remove_prefix(metric_end);

    if (error) {
      remaining = trim(remaining);
      continue;
    }

    /* Value and unit share the first field, with nothing in between. */
    const std::string_view first = next_field(&fields);
    size_t consumed = 0;
    p.value(to_float(first, &consumed));
    if (std::isnan(p.value())) {
      logger->warn(
          "storage: invalid perfdata format in service {}: no numeric value "
          "after equal sign '{}'",
          id(), excerpt(first));
      remaining = trim(remaining);
      continue;
    }
    p._unit.assign(first.substr(consumed));

    {
      float low, high;
      bool mode;
      parse_range(next_field(&fields), &low, &high, &mode);
      p.warning(high);
      p.warning_low(low);
      p.warning_mode(mode);
    }
    {
      float low, high;
      bool mode;
      parse_range(next_field(&fields), &low, &high, &mode);
      p.critical(high);
      p.critical_low(low);
      p.critical_mode(mode);
    }
    {
      size_t ignored = 0;
      p.min(to_float(trim(next_field(&fields)), &ignored));
      p.max(to_float(trim(next_field(&fields)), &ignored));
    }

    p._name.assign(name);

    logger->debug(
        "storage: got new perfdata (name={}, value={}, unit={}, warning={}, "
        "critical={}, min={}, max={})",
        p.name(), p.value(), p.unit(), p.warning(), p.critical(), p.min(),
        p.max());

    seen_names.insert(name);
    retval.push_back(std::move(p));

    remaining = trim(remaining);
  }
  return retval;
}
