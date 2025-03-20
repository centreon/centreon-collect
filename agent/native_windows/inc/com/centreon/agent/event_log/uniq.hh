/**
 * Copyright 2025 Centreon
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

#ifndef CENTREON_AGENT_CHECK_EVENT_LOG_UNIQ_HH
#define CENTREON_AGENT_CHECK_EVENT_LOG_UNIQ_HH

#include "event_log/data.hh"

namespace com::centreon::agent::event_log {

/**
 * @brief Comparator for event.
 *
 * On output formatting, we have to group events along some fields choosen by
 * user. This class is responsible for hashing and comparing events along these
 * fields.
 */
class event_comparator {
  using field_event_hasher = std::function<size_t(const event*)>;
  using field_event_compare = std::function<bool(const event*, const event*)>;

  std::vector<field_event_hasher> _hash;
  std::vector<field_event_compare> _compare;

 public:
  event_comparator() = delete;

  event_comparator(std::string_view fields,
                   const std::shared_ptr<spdlog::logger>& logger);

  bool operator()(const event* left, const event* right) const;
  std::size_t operator()(const event* evt) const;
};

}  // namespace com::centreon::agent::event_log

#endif
