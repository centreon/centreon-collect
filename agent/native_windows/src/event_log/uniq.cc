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

#include "event_log/uniq.hh"
#include <cstdint>

using namespace com::centreon::agent::event_log;

re2::RE2 field_regex("\\${([^\\${}]+)}");

/**
 * @brief Constructor for the event_comparator class.
 *
 * This constructor initializes an event_comparator object with the provided
 * fields and logger.
 * It creates a vector of hash and compare predicates
 *
 * @param fields The fields to compare as '${id} ${source}'.
 * @param logger The logger instance for logging.
 */
event_comparator::event_comparator(
    std::string_view fields,
    const std::shared_ptr<spdlog::logger>& logger) {
  std::string_view field;
  while (RE2::FindAndConsume(&fields, field_regex, &field)) {
    if (field == "source" || field == "provider") {
      _hash.emplace_back([](const event* evt) -> size_t {
        return absl::Hash<std::string>()(evt->get_provider());
      });
      _compare.emplace_back([](const event* left, const event* right) -> bool {
        return left->get_provider() == right->get_provider();
      });
    } else if (field == "id") {
      _hash.emplace_back(
          [](const event* evt) -> size_t { return evt->get_event_id(); });
      _compare.emplace_back([](const event* left, const event* right) -> bool {
        return left->get_event_id() == right->get_event_id();
      });
    } else if (field == "message") {
      _hash.emplace_back([](const event* evt) -> size_t {
        return absl::Hash<std::string>()(evt->get_message());
      });
      _compare.emplace_back([](const event* left, const event* right) -> bool {
        return left->get_message() == right->get_message();
      });
    } else if (field == "channel") {
      _hash.emplace_back([](const event* evt) -> size_t {
        return absl::Hash<std::string>()(evt->get_channel());
      });
      _compare.emplace_back([](const event* left, const event* right) -> bool {
        return left->get_channel() == right->get_channel();
      });
    }
  }
  if (_compare.empty()) {
    SPDLOG_LOGGER_DEBUG(logger, "no unique sort for output");
    _hash.emplace_back([](const event* evt) -> size_t {
      return reinterpret_cast<std::uintptr_t>(evt);
    });
    _compare.emplace_back([](const event* left, const event* right) -> bool {
      return left == right;
    });
  }
}

/**
 * @brief Compare two events.
 *
 * This method compares two events based on the fields provided in the
 * constructor.
 *
 * @param left The left event to compare.
 * @param right The right event to compare.
 * @return True if the events are equal, false otherwise.
 */
bool event_comparator::operator()(const event* left, const event* right) const {
  for (const auto& comp : _compare) {
    if (!comp(left, right)) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Hash an event.
 *
 * This method hashes an event based on the fields provided in the constructor.
 *
 * @param to_hash The event to hash.
 * @return The hash value of the event.
 */
std::size_t event_comparator::operator()(const event* to_hash) const {
  std::size_t ret = 0;
  for (const auto hash : _hash) {
    ret += hash(to_hash);
  }
  return ret;
}
