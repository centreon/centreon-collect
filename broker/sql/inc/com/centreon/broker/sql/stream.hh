/**
 * Copyright 2011-2016 Centreon
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

#ifndef CCB_SQL_STREAM_HH
#define CCB_SQL_STREAM_HH

#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/sql/cleanup.hh"
#include "com/centreon/broker/sql/mysql.hh"

namespace com::centreon::broker {

namespace sql {
/**
 *  @class stream stream.hh "com/centreon/broker/sql/stream.hh"
 *  @brief SQL stream.
 *
 *  Stream events into SQL database.
 */
class stream : public io::stream {
  mysql _mysql;

  // Cache
  database::mysql_stmt _empty_host_groups_delete;
  database::mysql_stmt _empty_service_groups_delete;
  database::mysql_stmt _host_parent_select;
  database::mysql_stmt _host_state_insupdate;
  database::mysql_stmt _instance_status_update;
  database::mysql_stmt _issue_insupdate;
  database::mysql_stmt _issue_parent_insert;
  database::mysql_stmt _issue_parent_update;
  database::mysql_stmt _service_state_insupdate;
  //  cleanup _cleanup_thread;
  int _pending_events;
  mutable std::mutex _stat_mutex;
  bool _stopped;

  void _process_log_issue(std::shared_ptr<io::data> const& e);
  std::shared_ptr<spdlog::logger> _logger_sql;
  std::shared_ptr<spdlog::logger> _logger_storage;

 public:
  stream(database_config const& dbcfg,
         uint32_t cleanup_check_interval,
         uint32_t loop_timeout,
         uint32_t instance_timeout,
         bool with_state_events);
  stream(stream const& other) = delete;
  stream& operator=(stream const& other) = delete;
  ~stream();
  int32_t stop() override;
  int flush() override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void update() override;
  int write(std::shared_ptr<io::data> const& d) override;
  void statistics(nlohmann::json& tree) const override;
};
}  // namespace sql

}  // namespace com::centreon::broker

#endif  // !CCB_SQL_STREAM_HH
