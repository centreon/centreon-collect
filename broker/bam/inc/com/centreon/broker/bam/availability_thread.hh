/**
 * Copyright 2014 - 2021-2024 Centreon
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

#ifndef CCB_BAM_AVAILABILITY_THREAD_HH
#define CCB_BAM_AVAILABILITY_THREAD_HH

#include "com/centreon/broker/bam/availability_builder.hh"
#include "com/centreon/broker/bam/timeperiod_map.hh"
#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/sql/database_config.hh"
#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/time/timeperiod.hh"
#include "com/centreon/broker/timestamp.hh"

namespace com::centreon::broker {

class database_query;

namespace bam {
/**
 *  @class availability_thread availability_thread.hh
 * "com/centreon/broker/bam/availability_thread.hh"
 *  @brief Availability thread
 *
 */
class availability_thread final {
 public:
  availability_thread(database_config const& db_cfg,
                      timeperiod_map& shared_map,
                      const std::shared_ptr<spdlog::logger>& logger);
  ~availability_thread();
  availability_thread(availability_thread const&) = delete;
  availability_thread& operator=(availability_thread const&) const = delete;

  virtual void run();
  void terminate();
  void start_and_wait();

  void lock();
  void unlock();

  void rebuild_availabilities(std::string const& bas_to_rebuild);
  void wait();

 private:
  void _delete_all_availabilities();
  void _build_availabilities(time_t midnight);
  void _build_daily_availabilities(int thread_id,
                                   time_t day_start,
                                   time_t day_end);
  void _write_availability(int thread_id,
                           availability_builder const& builder,
                           uint32_t ba_id,
                           time_t day_start,
                           uint32_t timeperiod_id);

  time_t _compute_next_midnight();
  void _open_database();
  void _close_database();

  std::thread _thread;

  // Checked from master
  bool _started_flag;

  std::unique_ptr<mysql> _mysql;
  database_config _db_cfg;
  timeperiod_map& _shared_tps;

  std::mutex _mutex;
  bool _should_exit;
  bool _should_rebuild_all;
  std::string _bas_to_rebuild;
  std::condition_variable _wait;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;
};
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_AVAILABILITY_THREAD_HH
