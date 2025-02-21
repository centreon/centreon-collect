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

#include "check_event_log_uniq.hh"
#include "re2/re2.h"
#include "spdlog/spdlog.h"

using namespace com::centreon::agent::check_event_log_detail;

re2::RE2 field_regex("\\${([^\\${}]+)}");

event_comparator::event_comparator(
    std::string_view fields,
    const std::shared_ptr<spdlog::logger>& logger) {
  std::string_view field;
  while (RE2::FindAndConsume(&fields, field_regex, &field)) {
    if (field == "source" || field == "provider") {
      _hash.emplace_back([](const event& evt) -> size_t {
        return absl::Hash<std::string>()(evt.provider());
      });
      _compare.emplace_back([](const event& left, const event& right) -> bool {
        return left.provider() == right.provider();
      });
    } else if (field == "id") {
      _hash.emplace_back([](const event& evt) -> size_t { return evt.id(); });
      _compare.emplace_back([](const event& left, const event& right) -> bool {
        return left.id() == right.id();
      });
    } else if (field == "message" || field == "message") {
      _hash.emplace_back([](const event& evt) -> size_t {
        return absl::Hash<std::string>()(evt.message());
      });
      _compare.emplace_back([](const event& left, const event& right) -> bool {
        return left.message() == right.message();
      });
    } else if (field == "channel" || field == "channel") {
      _hash.emplace_back([](const event& evt) -> size_t {
        return absl::Hash<std::string>()(evt.channel());
      });
      _compare.emplace_back([](const event& left, const event& right) -> bool {
        return left.channel() == right.channel();
      });
    }
  }
  if (_compare.empty()) {
    SPDLOG_LOGGER_DEBUG(logger, "no unique sort for output");
  }
}

bool event_comparator::operator()(const event& left, const event& right) const {
  for (const auto& comp : _compare) {
    if (!comp(left, right)) {
      return false;
    }
  }
  return true;
}

std::size_t event_comparator::operator()(const event& to_hash) const {
  std::size_t ret = 0;
  for (const auto hash : _hash) {
    ret += hash(to_hash);
  }
  return ret;
}
