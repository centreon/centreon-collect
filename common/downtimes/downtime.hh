/**
 * Copyright 2011 - 2013, 2026 Centreon (https://www.centreon.com/)
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

#ifndef CCC_DOWNTIMES_DOWNTIME_HH
#define CCC_DOWNTIMES_DOWNTIME_HH

namespace com::centreon::common::downtimes {

class downtime {
 public:
  enum type { service_downtime = 1, host_downtime = 2, any_downtime = 3 };

 protected:
  const uint64_t _host_id;
  const uint64_t _service_id;
  time_t _entry_time;
  std::string _author;
  std::string _comment;
  time_t _start_time;
  time_t _end_time;
  bool _fixed;
  uint64_t _triggered_by;
  uint32_t _duration;
  uint64_t _downtime_id;
  bool _in_effect;
  uint64_t _comment_id;
  int _start_flex_downtime;
  bool _incremented_pending_downtime;
  std::shared_ptr<spdlog::logger> _logger;

  void _set_in_effect(bool in_effect);
  uint64_t _get_comment_id() const;

 public:
  downtime(uint64_t host_id,
           uint64_t service_id,
           time_t entry_time,
           const std::string& author,
           const std::string& comment,
           time_t start_time,
           time_t end_time,
           bool fixed,
           uint64_t triggered_by,
           uint32_t duration,
           uint64_t downtime_id,
           const std::shared_ptr<spdlog::logger>& logger);
  downtime(downtime const&) = delete;
  downtime(downtime&&) = delete;
  ~downtime();

  type get_type() const;
  uint64_t service_id() const;
  bool is_stale() const;
  void notify_broker_load();
  bool unschedule();
  bool subscribe();
  bool handle();
  uint64_t host_id() const;
  void print(std::ostream& os) const;
  void retention(std::ostream& os) const;
  const std::string& get_author() const;
  const std::string& get_comment() const;
  uint64_t get_downtime_id() const;
  uint64_t get_triggered_by() const;
  bool is_fixed() const;
  time_t get_entry_time() const;
  time_t get_start_time() const;
  time_t get_end_time() const;
  uint32_t get_duration() const;
  bool is_in_effect() const;
  void start_flex_downtime();
};

}  // namespace com::centreon::common::downtimes

bool handle_scheduled_downtime_by_id(uint64_t downtime_id);

std::ostream& operator<<(std::ostream& os,
                         com::centreon::common::downtimes::downtime const& dt);

#endif  // !CCC_DOWNTIMES_DOWNTIME_HH
