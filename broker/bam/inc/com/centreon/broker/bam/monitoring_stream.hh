/*
** Copyright 2014-2021 Centreon
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

#ifndef CCB_BAM_MONITORING_STREAM_HH
#define CCB_BAM_MONITORING_STREAM_HH

#include <absl/hash/hash.h>
#include "com/centreon/broker/bam/configuration/applier/state.hh"
#include "com/centreon/broker/database/mysql_stmt.hh"
#include "com/centreon/broker/database_config.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/mysql.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace bam {
/**
 *  @class monitoring_stream monitoring_stream.hh
 * "com/centreon/broker/bam/monitoring_stream.hh"
 *  @brief bam monitoring_stream.
 *
 *  Handle perfdata and insert proper informations in index_data and
 *  metrics table of a centbam DB.
 */
class monitoring_stream : public io::stream {
  const std::string _ext_cmd_file;
  configuration::applier::state _applier;
  /* This mutex is to protect writes to the external command named pipe. */
  mutable std::mutex _ext_cmd_file_m;

  ba_svc_mapping _ba_mapping;
  mutable std::mutex _statusm;
  mysql _mysql;
  database::mysql_stmt _ba_update;
  database::mysql_stmt _kpi_update;
  int32_t _pending_events;
  database_config _storage_db_cfg;
  std::shared_ptr<persistent_cache> _cache;

  /* Each time a forced check is done on a service, we store the service in
   * the _forced_svc_checks table. And a timer is scheduled to execute the
   * post in 5s. As services are stored in a set, even if the are added several
   * times, they will be stored in the set once.
   * * the timer is used to schedule the post in 5s.
   * * the mutex is needed to manage the set.
   * * the set contains the services to force (hostname, service description) */
  asio::steady_timer _forced_svc_checks_timer;
  std::mutex _forced_svc_checks_m;
  std::unordered_set<std::pair<std::string, std::string>,
                     absl::Hash<std::pair<std::string, std::string>>>
      _forced_svc_checks;
  std::unordered_set<std::pair<std::string, std::string>,
                     absl::Hash<std::pair<std::string, std::string>>>
      _timer_forced_svc_checks;
  bool _stopped = false;

  void _check_replication();
  void _prepare();
  void _rebuild();
  void _write_external_command(const std::string& cmd);
  void _write_forced_svc_check(const std::string& host,
                               const std::string& description);
  void _explicitly_send_forced_svc_checks(const asio::error_code& ec);

  void _read_cache();
  void _write_cache();

 public:
  monitoring_stream(std::string const& ext_cmd_file,
                    database_config const& db_cfg,
                    database_config const& storage_db_cfg,
                    std::shared_ptr<persistent_cache> cache);
  ~monitoring_stream();
  monitoring_stream(const monitoring_stream&) = delete;
  monitoring_stream& operator=(const monitoring_stream&) = delete;
  int32_t flush() override;
  int32_t stop() override;
  void initialize();
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void update() override final;
  int write(std::shared_ptr<io::data> const& d) override;
};
}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_MONITORING_STREAM_HH
