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

/**
 *  Extract a real value from a perfdata string.
 *
 *  @param[in,out] str  Pointer to a perfdata string.
 *  @param[in]     skip true to skip semicolon.
 *
 *  @return Extracted real value if successful, NaN otherwise.
 */
static inline float extract_float(char const*& str, bool skip = true) {
  float retval;
  char* tmp;
  if (isspace(*str))
    retval = NAN;
  else {
    char const* comma{strchr(str, ',')};
    if (comma) {
      /* In case of comma decimal separator, we duplicate the number and
       * replace the comma by a point. */
      size_t t = strcspn(comma, " \t\n\r;");
      std::string nb(str, (comma - str) + t);
      nb[comma - str] = '.';
      retval = strtod(nb.c_str(), &tmp);
      if (nb.c_str() == tmp)
        retval = NAN;
      str = str + (tmp - nb.c_str());
    } else {
      retval = strtof(str, &tmp);
      if (str == tmp)
        retval = NAN;
      str = tmp;
    }
    if (skip && (*str == ';'))
      ++str;
  }
  return retval;
}

/**
 *  Extract a range from a perfdata string.
 *
 *  @param[out]    low       Low threshold value.
 *  @param[out]    high      High threshold value.
 *  @param[out]    inclusive true if range is inclusive, false
 *                           otherwise.
 *  @param[in,out] str       Pointer to a perfdata string.
 */
static inline void extract_range(float* low,
                                 float* high,
                                 bool* inclusive,
                                 char const*& str) {
  // Exclusive range ?
  if (*str == '@') {
    *inclusive = true;
    ++str;
  } else
    *inclusive = false;

  // Low threshold value.
  float low_value;
  if ('~' == *str) {
    low_value = -std::numeric_limits<float>::infinity();
    ++str;
  } else
    low_value = extract_float(str);

  // High threshold value.
  float high_value;
  if (*str != ':') {
    high_value = low_value;
    if (!std::isnan(low_value))
      low_value = 0.0;
  } else {
    ++str;
    char const* ptr(str);
    high_value = extract_float(str);
    if (std::isnan(high_value) && ((str == ptr) || (str == (ptr + 1))))
      high_value = std::numeric_limits<float>::infinity();
  }

  // Set values.
  *low = low_value;
  *high = high_value;
}

/**
 * @brief Parse perfdata string as given by plugin.
 *
 * @param host_id The host id of the service with this perfdata
 * @param service_id The service id of the service with this perfdata
 * @param str The perfdata string to parse
 *
 * @return A list of perfdata
 */
std::list<perfdata> perfdata::parse_perfdata(
    uint32_t host_id,
    uint32_t service_id,
    const char* str,
    const std::shared_ptr<spdlog::logger>& logger) {
  absl::flat_hash_set<std::string_view> metric_name;
  std::string_view current_name;
  std::list<perfdata> retval;
  auto id = [host_id, service_id] {
    if (host_id || service_id)
      return fmt::format("({}:{})", host_id, service_id);
    else
      return std::string();
  };

  size_t start = strspn(str, " \n\r\t");
  const char* buf = str + start;

  // Debug message.
  logger->debug("storage: parsing service {} perfdata string '{}'", id(), buf);

  char const* tmp = buf;

  auto skip = [](char const* tmp) -> char const* {
    while (*tmp && !isspace(*tmp))
      ++tmp;
    while (isspace(*tmp))
      ++tmp;
    return tmp;
  };

  while (*tmp) {
    bool error = false;

    // Perfdata object.
    perfdata p;

    // Get metric name.
    bool in_quote{false};
    char const* end{tmp};
    while (*end && (in_quote || (*end != '=' && !isspace(*end)) ||
                    static_cast<unsigned char>(*end) >= 128)) {
      if ('\'' == *end)
        in_quote = !in_quote;
      ++end;
    }

    /* The metric name is in the range s[0;size) */
    char const* s{tmp};
    tmp = end;
    --end;

    // Unquote metric name. Just beginning quotes and ending quotes"'".
    // We also remove spaces by the way.
    if (*s == '\'')
      ++s;
    if (*end == '\'')
      --end;

    while (*s && strchr(" \n\r\t", *s))
      ++s;
    while (end != s && strchr(" \n\r\t", *end))
      --end;

    /* The label is given by s and finishes at end */
    if (*end == ']') {
      if (strncmp(s, "a[", 2) == 0) {
        s += 2;
        --end;
        p._value_type = perfdata::data_type::absolute;
      } else if (strncmp(s, "c[", 2) == 0) {
        s += 2;
        --end;
        p._value_type = perfdata::data_type::counter;
      } else if (strncmp(s, "d[", 2) == 0) {
        s += 2;
        --end;
        p._value_type = perfdata::data_type::derive;
      } else if (strncmp(s, "g[", 2) == 0) {
        s += 2;
        --end;
        p._value_type = perfdata::data_type::gauge;
      }
    }

    if (end - s + 1 > 0) {
      p._name.assign(s, end - s + 1);
      current_name = std::string_view(s, end - s + 1);

      if (metric_name.contains(current_name)) {
        logger->warn(
            "storage: The metric '{}' appears several times in the output "
            "\"{}\": you will lose any new occurence of this metric",
            p.name(), str);
        error = true;
      }
    } else {
      logger->error("In service {}, metric name empty before '{}...'", id(),
                    fmt::string_view(s, 10));
      error = true;
    }

    // Check format.
    if (*tmp != '=') {
      int i;
      for (i = 0; i < 10 && tmp[i]; i++)
        ;
      logger->warn(
          "invalid perfdata format in service {}: equal sign not present or "
          "misplaced '{}'",
          id(), fmt::string_view(s, (tmp - s) + i));
      error = true;
    } else
      ++tmp;

    if (error) {
      tmp = skip(tmp);
      continue;
    }

    // Extract value.
    p.value(extract_float(tmp, false));
    if (std::isnan(p.value())) {
      int i;
      for (i = 0; i < 10 && tmp[i]; i++)
        ;

      logger->warn(
          "storage: invalid perfdata format in service {}: no numeric value "
          "after equal sign "
          "'{}'",
          id(), fmt::string_view(s, (tmp - s) + i));
      tmp = skip(tmp);
      continue;
    }

    // Extract unit.
    size_t t = strcspn(tmp, " \t\n\r;");
    p._unit.assign(tmp, t);
    tmp += t;
    if (*tmp == ';')
      ++tmp;

    // Extract warning.
    {
      float warning_high;
      float warning_low;
      bool warning_mode;
      extract_range(&warning_low, &warning_high, &warning_mode, tmp);
      p.warning(warning_high);
      p.warning_low(warning_low);
      p.warning_mode(warning_mode);
    }

    // Extract critical.
    {
      float critical_high;
      float critical_low;
      bool critical_mode;
      extract_range(&critical_low, &critical_high, &critical_mode, tmp);
      p.critical(critical_high);
      p.critical_low(critical_low);
      p.critical_mode(critical_mode);
    }

    // Extract minimum.
    p.min(extract_float(tmp));

    // Extract maximum.
    p.max(extract_float(tmp));

    // Log new perfdata.
    logger->debug(
        "storage: got new perfdata (name={}, value={}, unit={}, warning={}, "
        "critical={}, min={}, max={})",
        p.name(), p.value(), p.unit(), p.warning(), p.critical(), p.min(),
        p.max());

    // Append to list.
    metric_name.insert(current_name);
    retval.push_back(std::move(p));

    // Skip whitespaces.
    while (isspace(*tmp))
      ++tmp;
  }
  return retval;
}
