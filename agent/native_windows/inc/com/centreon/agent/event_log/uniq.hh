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

#ifndef CENTREON_AGENT_CHECK_EVENT_LOG_UNIQ_HH
#define CENTREON_AGENT_CHECK_EVENT_LOG_UNIQ_HH

#include <stdexcept>
#include "event_log/data.hh"

namespace com::centreon::agent::event_log {

/**
 * @brief this class agregate several events that are considered same along
 * event_comparator criterias
 * we will store this agregates in multi_index container indexed by event and
 * get_oldest.
 * In order to improve performance, _time_points is mutable. By doing so, we can
 * add more recent time_point to a const unique_event object and then not
 * recalculate indexes. If new time point is older than _oldest, we must use non
 * const version of add_time_point and recalculate indexes
 */
class unique_event {
  using time_point_set = std::multiset<std::chrono::file_clock::time_point>;
  event _evt;
  mutable time_point_set _time_points;
  std::chrono::file_clock::time_point _oldest;

 public:
  unique_event(event&& evt);
  unique_event(const unique_event&) = delete;
  unique_event& operator=(const unique_event&) = delete;

  const event& get_event() const { return _evt; }

  std::chrono::file_clock::time_point get_oldest() const { return _oldest; }

  void add_time_point(const std::chrono::file_clock::time_point& tp) const {
    if (tp < _oldest) {
      throw std::invalid_argument(
          "usage of add_time_point const can only insert later values ");
    }
    _time_points.insert(tp);
  }

  void add_time_point(const std::chrono::file_clock::time_point& tp) {
    _time_points.insert(tp);
    if (tp < _oldest) {
      _oldest = tp;
    }
  }

  bool empty() const { return _time_points.empty(); }

  size_t size() const { return _time_points.size(); }

  void clean_oldest(std::chrono::file_clock::time_point peremption);
};

class event_comparator {
  using field_event_hasher = std::function<size_t(const event&)>;
  using field_event_compare = std::function<bool(const event&, const event&)>;

  std::vector<field_event_hasher> _hash;
  std::vector<field_event_compare> _compare;

 public:
  event_comparator() = delete;

  event_comparator(std::string_view fields,
                   const std::shared_ptr<spdlog::logger>& logger);

  bool operator()(const event& left, const event& right) const;
  inline bool operator()(const event& left, const unique_event& right) const {
    return this->operator()(left, right.get_event());
  }
  inline bool operator()(const unique_event& left, const event& right) const {
    return this->operator()(left.get_event(), right);
  }
  inline bool operator()(const unique_event& left,
                         const unique_event& right) const {
    return this->operator()(left.get_event(), right.get_event());
  }

  inline std::size_t operator()(const unique_event& evt) const {
    return this->operator()(evt.get_event());
  }
  std::size_t operator()(const event& evt) const;
};

}  // namespace com::centreon::agent::event_log

#endif
