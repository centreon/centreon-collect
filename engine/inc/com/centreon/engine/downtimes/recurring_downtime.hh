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

#ifndef CCE_DOWNTIMES_RECURRING_DOWNTIME_HH
#define CCE_DOWNTIMES_RECURRING_DOWNTIME_HH

#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::downtimes {

/**
 * @brief One occurrence of a recurring downtime period: [start, end).
 */
struct occurrence {
  time_t start;
  time_t end;
  bool fixed;
  uint32_t duration;

  bool operator==(const occurrence& other) const {
    return start == other.start && end == other.end && fixed == other.fixed &&
           duration == other.duration;
  }
};

/**
 * @brief Occurrences of the periods of a rule on the local days before, of and
 * after `now`, kept when start <= now + look_ahead and end > now.
 *
 * Pure function of its arguments, apart from the process timezone that it
 * switches to `tz` (timezone_locker) during the computation.
 *
 * @param rule The recurring downtime rule.
 * @param tz The timezone ("" = the process one), in the TZ variable syntax.
 * @param now The current time.
 * @param look_ahead How long before its start an occurrence is returned.
 *
 * @return The occurrences, sorted by start time.
 */
std::vector<occurrence> compute_occurrences(
    const configuration::Recurringdowntime& rule,
    const std::string& tz,
    time_t now,
    time_t look_ahead = 600);

/**
 * @brief A recurring downtime rule (Centreon "downtime cycle") applied to this
 * Engine. Each run schedules, through downtime_manager, the occurrences that
 * are about to start on its targets, as the SCHEDULE_*_DOWNTIME external
 * commands do. Main loop only.
 *
 * An occurrence (rule, target host or service, start time) is scheduled when
 * all of these hold:
 * - compute_occurrences() returns it: start <= now + 600 and end > now;
 * - its start is not before the rule's `not_before`: the Engine process start
 *   time (program_start) for a rule of the configuration loaded at start, no
 *   limit (0) for a rule added or modified by a reload. So a restart does not
 *   catch up an occurrence that already started (nor brings back one that was
 *   cancelled), while a rule changed at reload takes effect at once, current
 *   occurrence included;
 * - this rule object has not handled it yet in this process: once scheduled,
 *   found already scheduled or refused, it is never scheduled again, so a
 *   cancelled downtime does not come back. The handled set is pruned once the
 *   occurrence has ended; a modified rule is a new object with an empty set;
 * - no downtime with the same target, comment and start time exists (covers
 *   the downtimes restored from retention).
 */
class recurring_downtime {
  const configuration::Recurringdowntime _config;
  const time_t _not_before;
  std::vector<uint64_t> _host_ids;
  std::vector<std::pair<uint64_t, uint64_t>> _service_ids;
  // (start, host_id, service_id) -> end of the handled occurrences.
  std::map<std::tuple<time_t, uint64_t, uint64_t>, time_t> _handled;

  static bool _initial_load_done;

  void _schedule_one(uint64_t host_id,
                     uint64_t service_id,
                     const occurrence& occ,
                     time_t now);

 public:
  static std::map<uint64_t, std::shared_ptr<recurring_downtime>>
      recurring_downtimes;
  static constexpr const char* author = "Downtime cycle";

  recurring_downtime(const configuration::Recurringdowntime& config,
                     time_t not_before);
  recurring_downtime(const recurring_downtime&) = delete;
  recurring_downtime& operator=(const recurring_downtime&) = delete;
  ~recurring_downtime() noexcept = default;

  uint64_t id() const { return _config.downtime_id(); }
  const configuration::Recurringdowntime& config() const { return _config; }
  time_t not_before() const { return _not_before; }
  void set_targets(std::vector<uint64_t>&& host_ids,
                   std::vector<std::pair<uint64_t, uint64_t>>&& service_ids);
  void schedule(time_t now);

  static std::string comment(uint64_t downtime_id);
  static void schedule_all(time_t now);
  static int delete_future_downtimes(uint64_t downtime_id, time_t now);
  static time_t not_before_for_new_rule();
  static void set_initial_load_done();
  static void clear();
};

}  // namespace com::centreon::engine::downtimes

#endif  // !CCE_DOWNTIMES_RECURRING_DOWNTIME_HH
