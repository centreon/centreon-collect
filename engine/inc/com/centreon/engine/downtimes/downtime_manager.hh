/*
 * Copyright 2019 - 2020 Centreon (https://www.centreon.com/)
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

#ifndef CCE_DOWNTIMES_DOWTIME_MANAGER_HH
#define CCE_DOWNTIMES_DOWTIME_MANAGER_HH

#include "com/centreon/engine/downtimes/downtime.hh"

namespace com::centreon::engine {

namespace downtimes {

class host_downtime;
class service_downtime;
class downtime_manager {
 public:
  static downtime_manager& instance() {
    static downtime_manager instance;
    return instance;
  }
  std::multimap<time_t, std::shared_ptr<downtime>> const&
  get_scheduled_downtimes() const;

  void delete_downtime(uint64_t downtime_id);
  int unschedule_downtime(uint64_t downtime_id);
  std::shared_ptr<downtime> find_downtime(downtime::type type,
                                          uint64_t downtime_id);
  int check_pending_flex_host_downtime(host* hst);
  int check_pending_flex_service_downtime(service* svc);
  void add_downtime(const std::shared_ptr<downtime>& dt) noexcept;
  void clear_scheduled_downtimes();
  int check_for_expired_downtime();
  int delete_downtime_by_hostname_service_description_start_time_comment(
      std::string const& hostname,
      std::string const& service_description,
      std::pair<bool, time_t> const& start_time,
      std::string const& comment);
  void insert_downtime(std::shared_ptr<downtime> dt);
  void initialize_downtime_data();
  int xdddefault_validate_downtime_data();
  uint64_t get_next_downtime_id();
  int schedule_downtime(downtime::type type,
                        const uint64_t host_id,
                        const uint64_t service_id,
                        time_t entry_time,
                        char const* author,
                        char const* comment_data,
                        time_t start_time,
                        time_t end_time,
                        bool fixed,
                        uint64_t triggered_by,
                        unsigned long duration,
                        uint64_t* new_downtime_id);
  int register_downtime(downtime::type type, uint64_t downtime_id);

 protected:
  std::shared_ptr<host_downtime> add_new_host_downtime(const uint64_t host_id,
                                                       time_t entry_time,
                                                       char const* author,
                                                       char const* comment_data,
                                                       time_t start_time,
                                                       time_t end_time,
                                                       bool fixed,
                                                       uint64_t triggered_by,
                                                       unsigned long duration);
  std::shared_ptr<service_downtime> add_new_service_downtime(
      const uint64_t host_id,
      const uint64_t service_id,
      time_t entry_time,
      const std::string& author,
      const std::string& comment_data,
      time_t start_time,
      time_t end_time,
      bool fixed,
      uint64_t triggered_by,
      unsigned long duration);

 private:
  downtime_manager() = default;
  std::multimap<time_t, std::shared_ptr<downtime>> _scheduled_downtimes;
  uint64_t _next_id;
};
}  // namespace downtimes

}  // namespace com::centreon::engine

#endif  // !CCE_DOWNTIMES_DOWNTIME_MANAGER_HH
