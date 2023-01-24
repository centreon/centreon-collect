/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "configuration/timeperiod_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Timeperiod object.
 *
 * @param obj The Timeperiod object on which this helper works. The helper is
 * not the owner of this object.
 */
timeperiod_helper::timeperiod_helper(Timeperiod* obj)
    : message_helper(object_type::timeperiod, obj, {}, 7) {
  _init();
}

/**
 * @brief For several keys, the parser of Timeperiod objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool timeperiod_helper::hook(const absl::string_view& key,
                             const absl::string_view& value) {
  Timeperiod* obj = static_cast<Timeperiod*>(mut_obj());
  auto get_timerange = [](const absl::string_view& value, auto* day) -> bool {
    auto arr = absl::StrSplit(value, ',');
    for (auto& d : arr) {
      std::pair<absl::string_view, absl::string_view> v =
          absl::StrSplit(d, '-');
      TimeRange tr;
      std::pair<absl::string_view, absl::string_view> p =
          absl::StrSplit(v.first, ':');
      uint32_t h, m;
      if (!absl::SimpleAtoi(p.first, &h) || !absl::SimpleAtoi(p.second, &m))
        return false;
      tr.set_range_start(h * 3600 + m * 60);
      p = absl::StrSplit(v.second, ':');
      if (!absl::SimpleAtoi(p.first, &h) || !absl::SimpleAtoi(p.second, &m))
        return false;
      tr.set_range_end(h * 3600 + m * 60);
      day->Add(std::move(tr));
    }
    return true;
  };

  if (key == "exclude") {
    fill_string_group(obj->mutable_exclude(), value);
    return true;
  } else if (key == "sunday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_sunday());
  else if (key == "monday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_monday());
  else if (key == "tuesday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_tuesday());
  else if (key == "wednesday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_wednesday());
  else if (key == "thursday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_thursday());
  else if (key == "friday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_friday());
  else if (key == "saturday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_saturday());
  return false;
}

/**
 * @brief Check the validity of the Timeperiod object.
 */
void timeperiod_helper::check_validity() const {
  const Timeperiod* o = static_cast<const Timeperiod*>(obj());

  if (o->timeperiod_name().empty())
    throw msg_fmt("Time period has no name (property 'timeperiod_name')");
}
void timeperiod_helper::_init() {}

}  // namespace com::centreon::engine::configuration