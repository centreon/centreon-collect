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

namespace com::centreon::engine::configuration {

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

}  // namespace com::centreon::engine::configuration
