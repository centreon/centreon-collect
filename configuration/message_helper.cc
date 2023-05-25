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

#include "configuration/message_helper.hh"
#include <absl/strings/str_split.h>
#include "com/centreon/engine/configuration/host.hh"
#include "com/centreon/engine/configuration/service.hh"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

bool fill_pair_string_group(PairStringSet* grp,
                            const absl::string_view& value) {
  auto arr = absl::StrSplit(value, ',');

  bool first = true;
  auto itfirst = arr.begin();
  if (itfirst == arr.end())
    return true;

  do {
    auto itsecond = itfirst;
    ++itsecond;
    if (itsecond == arr.end())
      return false;
    absl::string_view v1 = absl::StripAsciiWhitespace(*itfirst);
    absl::string_view v2 = absl::StripAsciiWhitespace(*itsecond);
    if (first) {
      if (v1[0] == '+') {
        grp->set_additive(true);
        v1 = v1.substr(1);
      }
      first = false;
    }
    bool found = false;
    for (auto& m : grp->data()) {
      if (*itfirst == m.first() && *itsecond == m.second()) {
        found = true;
        break;
      }
    }
    if (!found) {
      auto* p = grp->mutable_data()->Add();
      p->set_first(v1.data(), v1.size());
      p->set_second(v2.data(), v2.size());
    }
    itfirst = itsecond;
    ++itfirst;
  } while (itfirst != arr.end());
  return true;
}

bool fill_pair_string_group(PairStringSet* grp,
                            const absl::string_view& key,
                            const absl::string_view& value) {
  absl::string_view v1 = absl::StripAsciiWhitespace(key);
  absl::string_view v2 = absl::StripAsciiWhitespace(value);
  bool found = false;
  for (auto& m : grp->data()) {
    if (v1 == m.first() && v2 == m.second()) {
      found = true;
      break;
    }
  }
  if (!found) {
    auto* p = grp->mutable_data()->Add();
    p->set_first(v1.data(), v1.size());
    p->set_second(v2.data(), v2.size());
  }
  return true;
}

void fill_string_group(StringSet* grp, const absl::string_view& value) {
  auto arr = absl::StrSplit(value, ',');
  bool first = true;
  for (absl::string_view d : arr) {
    d = absl::StripAsciiWhitespace(d);
    if (first) {
      if (d[0] == '+') {
        grp->set_additive(true);
        d = d.substr(1);
      }
      first = false;
    }
    bool found = false;
    for (auto& v : grp->data()) {
      if (v == d) {
        found = true;
        break;
      }
    }
    if (!found)
      grp->add_data(d.data(), d.size());
  }
}

void fill_string_group(StringList* grp, const absl::string_view& value) {
  auto arr = absl::StrSplit(value, ',');
  bool first = true;
  for (absl::string_view d : arr) {
    d = absl::StripAsciiWhitespace(d);
    if (first) {
      if (d[0] == '+') {
        grp->set_additive(true);
        d = d.substr(1);
      }
      first = false;
    }
    grp->add_data(d.data(), d.size());
  }
}

/**
 * @brief Parse host notification options as string and set an uint32_t to
 * the corresponding values.
 *
 * @param options A pointer to the uint32_t to set/
 * @param value A string of options seperated by a comma.
 *
 * @return True on success.
 */
bool fill_host_notification_options(uint32_t* options,
                                    const absl::string_view& value) {
  uint32_t tmp_options = host::none;
  auto arr = absl::StrSplit(value, ',');
  for (auto& v : arr) {
    absl::string_view value = absl::StripAsciiWhitespace(v);
    if (value == "d" || value == "down")
      tmp_options |= configuration::host::down;
    else if (value == "u" || value == "unreachable")
      tmp_options |= configuration::host::unreachable;
    else if (value == "r" || value == "recovery")
      tmp_options |= configuration::host::up;
    else if (value == "f" || value == "flapping")
      tmp_options |= configuration::host::flapping;
    else if (value == "s" || value == "downtime")
      tmp_options |= configuration::host::downtime;
    else if (value == "n" || value == "none")
      tmp_options = configuration::host::none;
    else if (value == "a" || value == "all")
      tmp_options = configuration::host::down |
                    configuration::host::unreachable | configuration::host::up |
                    configuration::host::flapping |
                    configuration::host::downtime;
    else
      return false;
  }
  *options = tmp_options;
  return true;
}

/**
 * @brief Parse host notification options as string and set an uint32_t to
 * the corresponding values.
 *
 * @param options A pointer to the uint32_t to set/
 * @param value A string of options seperated by a comma.
 *
 * @return True on success.
 */
bool fill_service_notification_options(uint32_t* options,
                                       const absl::string_view& value) {
  uint32_t tmp_options = service::none;
  auto arr = absl::StrSplit(value, ',');
  for (auto& v : arr) {
    absl::string_view value = absl::StripAsciiWhitespace(v);
    if (value == "u" || value == "unknown")
      tmp_options |= service::unknown;
    else if (value == "w" || value == "warning")
      tmp_options |= service::warning;
    else if (value == "c" || value == "critical")
      tmp_options |= service::critical;
    else if (value == "r" || value == "recovery")
      tmp_options |= service::ok;
    else if (value == "f" || value == "flapping")
      tmp_options |= service::flapping;
    else if (value == "s" || value == "downtime")
      tmp_options |= service::downtime;
    else if (value == "n" || value == "none")
      tmp_options = service::none;
    else if (value == "a" || value == "all")
      tmp_options = service::unknown | service::warning | service::critical |
                    service::ok | service::flapping | service::downtime;
    else
      return false;
  }
  *options = tmp_options;
  return true;
}

absl::string_view message_helper::validate_key(
    const absl::string_view& key) const {
  absl::string_view retval;
  auto it = _correspondence.find(key);
  if (it != _correspondence.end())
    retval = it->second;
  else
    retval = key;
  return retval;
}

bool message_helper::insert_customvariable(absl::string_view key,
                                           absl::string_view value) {
  return false;
}

}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com
