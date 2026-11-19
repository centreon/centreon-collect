/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "common/engine_conf/recurringdowntime_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

namespace {
/**
 * @brief Parse "HH:MM" into minutes since midnight; 24:00 is accepted.
 */
bool parse_hhmm(std::string_view value, uint32_t* minutes) {
  std::vector<std::string_view> parts = absl::StrSplit(value, ':');
  uint32_t h, m;
  if (parts.size() != 2 || parts[0].empty() || parts[1].size() != 2 ||
      !absl::SimpleAtoi(parts[0], &h) || !absl::SimpleAtoi(parts[1], &m))
    return false;
  if (h > 24 || m > 59 || (h == 24 && m != 0))
    return false;
  *minutes = h * 60 + m;
  return true;
}

/**
 * @brief Parse a comma separated list of integers in [min, max].
 */
bool parse_days(std::string_view value,
                uint32_t min,
                uint32_t max,
                RecurringdowntimePeriod* period) {
  if (value.empty())
    return false;
  for (std::string_view d : absl::StrSplit(value, ',')) {
    uint32_t day;
    if (!absl::SimpleAtoi(d, &day) || day < min || day > max)
      return false;
    period->add_days(day);
  }
  return true;
}
}  // namespace

/**
 * @brief Constructor from a Recurringdowntime object.
 *
 * @param obj The object on which this helper works. The helper is not the
 * owner of this object.
 */
recurringdowntime_helper::recurringdowntime_helper(Recurringdowntime* obj)
    : message_helper(object_type::recurringdowntime,
                     obj,
                     {},
                     Recurringdowntime::descriptor()->field_count()) {
  obj->mutable_obj()->set_register_(true);
}

/**
 * @brief Parse a `period` value:
 * `<weekly:d,...|monthly:n,...|nth:<first|second|third|fourth|last>:d>
 * <HH:MM>-<HH:MM> <fixed|flexible:<seconds>>`.
 *
 * @param value The value of the `period` key.
 * @param period The period to fill.
 *
 * @return true on success.
 */
bool recurringdowntime_helper::parse_period(std::string_view value,
                                            RecurringdowntimePeriod* period) {
  std::vector<std::string_view> parts =
      absl::StrSplit(value, absl::ByAnyChar(" \t"), absl::SkipEmpty());
  if (parts.size() != 3)
    return false;

  std::vector<std::string_view> days = absl::StrSplit(parts[0], ':');
  if (days.size() == 2 && days[0] == "weekly") {
    period->set_kind(RecurringdowntimePeriod::weekly);
    if (!parse_days(days[1], 1, 7, period))
      return false;
  } else if (days.size() == 2 && days[0] == "monthly") {
    period->set_kind(RecurringdowntimePeriod::monthly);
    if (!parse_days(days[1], 1, 31, period))
      return false;
  } else if (days.size() == 3 && days[0] == "nth") {
    period->set_kind(RecurringdowntimePeriod::nth);
    if (days[1] == "first")
      period->set_rank(1);
    else if (days[1] == "second")
      period->set_rank(2);
    else if (days[1] == "third")
      period->set_rank(3);
    else if (days[1] == "fourth")
      period->set_rank(4);
    else if (days[1] == "last")
      period->set_rank(-1);
    else
      return false;
    uint32_t day;
    if (!absl::SimpleAtoi(days[2], &day) || day < 1 || day > 7)
      return false;
    period->add_days(day);
  } else
    return false;

  std::pair<std::string_view, std::string_view> range =
      absl::StrSplit(parts[1], absl::MaxSplits('-', 1));
  uint32_t start, end;
  if (!parse_hhmm(range.first, &start) || !parse_hhmm(range.second, &end) ||
      end <= start)
    return false;
  period->set_start_minute(start);
  period->set_end_minute(end);

  if (parts[2] == "fixed") {
    period->set_fixed(true);
    period->set_duration(0);
  } else if (absl::StartsWith(parts[2], "flexible:")) {
    uint32_t duration;
    // 0 is accepted: Centreon writes it for a flexible period without duration.
    if (!absl::SimpleAtoi(parts[2].substr(9), &duration))
      return false;
    period->set_fixed(false);
    period->set_duration(duration);
  } else
    return false;
  return true;
}

/**
 * @brief Parse the keys of a recurringdowntime that need a special syntax.
 *
 * @param key The key to parse.
 * @param value The value corresponding to the key
 *
 * @return true if the key was handled here.
 */
bool recurringdowntime_helper::hook(std::string_view key,
                                    std::string_view value) {
  Recurringdowntime* obj = static_cast<Recurringdowntime*>(mut_obj());
  key = validate_key(key);

  if (key == "period") {
    RecurringdowntimePeriod period;
    if (!parse_period(value, &period))
      return false;
    obj->add_periods()->Swap(&period);
    return true;
  } else if (key == "host_ids") {
    for (std::string_view h :
         absl::StrSplit(value, ',', absl::SkipWhitespace())) {
      uint64_t host_id;
      if (!absl::SimpleAtoi(h, &host_id) || host_id == 0)
        return false;
      obj->add_host_ids(host_id);
    }
    return true;
  } else if (key == "service_ids") {
    for (std::string_view s :
         absl::StrSplit(value, ',', absl::SkipWhitespace())) {
      std::pair<std::string_view, std::string_view> p =
          absl::StrSplit(s, absl::MaxSplits(':', 1));
      uint64_t host_id, service_id;
      if (!absl::SimpleAtoi(p.first, &host_id) ||
          !absl::SimpleAtoi(p.second, &service_id) || host_id == 0 ||
          service_id == 0)
        return false;
      HostServiceId* id = obj->add_service_ids();
      id->set_host_id(host_id);
      id->set_service_id(service_id);
    }
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Recurringdowntime object.
 *
 * @param err An error counter.
 */
void recurringdowntime_helper::check_validity(error_cnt& err) const {
  const Recurringdowntime* o = static_cast<const Recurringdowntime*>(obj());

  if (o->downtime_id() == 0) {
    ++err.config_errors;
    throw msg_fmt("Recurring downtime has no id (property 'downtime_id')");
  }
  if (o->periods().empty()) {
    ++err.config_errors;
    throw msg_fmt("Recurring downtime {} has no period (property 'period')",
                  o->downtime_id());
  }
  if (o->host_ids().empty() && o->service_ids().empty()) {
    ++err.config_errors;
    throw msg_fmt(
        "Recurring downtime {} has no target (properties 'host_ids' and "
        "'service_ids')",
        o->downtime_id());
  }
}
}  // namespace com::centreon::engine::configuration
