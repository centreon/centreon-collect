/*
** Copyright 2012-2015,2017-2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_UNIFIED_SQL_REBUILDER_HH
#define CCB_UNIFIED_SQL_REBUILDER_HH

#include <memory>

#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/common/pool.hh"

namespace com::centreon::broker {

namespace unified_sql {
class stream;

/**
 *  @class rebuilder rebuilder.hh "com/centreon/broker/unified_sql/rebuilder.hh"
 *  @brief Check for graphs to be rebuild.
 *
 *  Check for graphs to be rebuild at fixed interval.
 *
 *  We don't instantiate a thread to work on the rebuilder. Instead, we use
 *  the asio mechanism with a steady_timer. The main function is
 *  rebuilder::_run(). When the rebuilder is constructed, we instanciate _timer
 *  and ask to execute the _run function when it expires. When the _run()
 *  function finishes, it reschedules the timer to be executed a new time after
 *  _rebuild_check_interval seconds. The rebuild destructor cancels the timer.
 *
 *  Each execution of the timer is done using the thread pool accessible from
 *  the pool object. No new thread is created.
 */
class rebuilder {
  database_config _db_cfg;
  std::shared_ptr<mysql_connection> _connection;
  uint32_t _interval_length;
  uint32_t _rrd_len;

  // Local types.
  struct metric_info {
    std::string metric_name;
    int32_t data_source_type;
    int32_t rrd_retention;
    uint32_t check_interval;
  };

  std::mutex _rebuilding_m;
  std::condition_variable _rebuilding_cv;
  int32_t _rebuilding = 0;

 public:
  rebuilder(database_config const& db_cfg,
            uint32_t interval_length = 60,
            uint32_t rrd_length = 15552000);
  ~rebuilder() noexcept;
  rebuilder(const rebuilder&) = delete;
  rebuilder& operator=(const rebuilder&) = delete;
  void rebuild_graphs(const std::shared_ptr<io::data>& d,
                      const std::shared_ptr<spdlog::logger>& logger);
};
}  // namespace unified_sql

}  // namespace com::centreon::broker

#endif  // !CCB_UNIFIED_SQL_REBUILDER_HH
