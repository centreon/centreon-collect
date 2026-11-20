/**
 * Copyright 2019 - 2020, 2026 Centreon (https://www.centreon.com/)
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

#ifndef CCC_DOWNTIMES_DOWNTIME_MANAGER_HH
#define CCC_DOWNTIMES_DOWNTIME_MANAGER_HH

#include <optional>

#include "common/downtimes/downtime.hh"
#include "common/downtimes/downtime_callbacks.hh"
#include "common/log_v2/log_v2.hh"

namespace com::centreon::common::downtimes {
using com::centreon::common::log_v2::log_v2;

class downtime_manager {
  static std::unique_ptr<downtime_manager> _instance;
  const std::unique_ptr<downtime_callbacks> _callbacks;

  std::multimap<time_t, std::shared_ptr<downtime>> _scheduled_downtimes;
  uint64_t _next_id = 0u;
  std::shared_ptr<spdlog::logger> _logger =
      log_v2::instance().get(log_v2::DOWNTIMES);

 protected:
  std::shared_ptr<downtime> add_new_downtime(uint64_t host_id,
                                             uint64_t service_id,
                                             time_t entry_time,
                                             const std::string& author,
                                             const std::string& comment_data,
                                             time_t start_time,
                                             time_t end_time,
                                             bool fixed,
                                             uint64_t triggered_by,
                                             uint32_t duration);

 public:
  downtime_manager(std::unique_ptr<downtime_callbacks> callbacks);
  static void load(std::unique_ptr<downtime_callbacks> callbacks) {
    if (!_instance)
      _instance = std::make_unique<downtime_manager>(std::move(callbacks));
  }

  static downtime_manager& instance() {
    assert(_instance);
    return *_instance;
  }

  std::multimap<time_t, std::shared_ptr<downtime>> const&
  get_scheduled_downtimes() const;

  void delete_downtime(uint64_t downtime_id);
  bool unschedule_downtime(uint64_t downtime_id);
  std::shared_ptr<downtime> find_downtime(downtime::type type,
                                          uint64_t downtime_id);
  void activate_pending_flex_host_downtimes(uint64_t host_id);
  void activate_pending_flex_service_downtimes(uint64_t host_id,
                                               uint64_t service_id);
  void add_downtime(const std::shared_ptr<downtime>& dt) noexcept;
  void clear_scheduled_downtimes();
  void delete_expired_downtimes();
  int delete_downtime_by_hostname_service_description_start_time_comment(
      const std::string& hostname,
      const std::string& service_description,
      std::optional<time_t> start_time,
      const std::string& comment);
  void initialize_downtime_data();
  void validate_downtime_data();
  uint64_t get_next_downtime_id();
  bool schedule_downtime(downtime::type type,
                        const uint64_t host_id,
                        const uint64_t service_id,
                        time_t entry_time,
                        const std::string& author,
                        const std::string& comment_data,
                        time_t start_time,
                        time_t end_time,
                        bool fixed,
                        uint64_t triggered_by,
                        uint32_t duration,
                        uint64_t* new_downtime_id);
  downtime_callbacks& callbacks() const;
};
}  // namespace com::centreon::common::downtimes

#endif  // !CCC_DOWNTIMES_DOWNTIME_MANAGER_HH
