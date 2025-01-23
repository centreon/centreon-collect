/*
 * Copyright 2014-2023 Centreon
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

#ifndef CCB_BAM_MONITORING_STREAM_HH
#define CCB_BAM_MONITORING_STREAM_HH

#include <absl/hash/hash.h>

#include "com/centreon/broker/bam/configuration/applier/state.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/sql/database_config.hh"
#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/sql/mysql_multi_insert.hh"
#include "com/centreon/broker/sql/mysql_stmt.hh"

namespace com::centreon::broker {

namespace extcmd {
using pb_ba_info =
    io::protobuf<BaInfo, make_type(io::extcmd, extcmd::de_ba_info)>;
}
namespace bam {
/**
 *  @class monitoring_stream monitoring_stream.hh
 * "com/centreon/broker/bam/monitoring_stream.hh"
 *  @brief bam monitoring_stream.
 *
 *  Handle perfdata and insert proper informations in index_data and
 *  metrics table of a centbam DB.
 *
 *  This class also sends external commands to centengine, two kinds of
 *  commands:
 *  * **forced service checks** because each BA is represented by a service on
 *  centengine side and when a BA changes of state, the service has also to be
 *  updated.
 *  * **downtimes** because if an inherited downtime is set on a BA, we have to
 *  apply a "downtime" on its corresponding service.
 *
 *  Forced checks can be numerous and we see sometimes many forced checks on the
 *  same service, this is because of the tree structure of BAs. To avoid asking
 *  centengine too many forced checks, broker stores them in the
 *  _forced_svc_checks set, keeping them during 5s and each time a new check is
 *  added to that set, the scheduling is reset to a new duration of 5s. Then
 *  thanks to the structure of a set we are sure each service will receive a
 *  forced check only once. As the timer that sends messages is in another
 *  thread, we have to protect _forced_svc_checks with a mutex
 *  _forced_svc_checks_m.
 *  There is also another set named _timer_forced_svc_checks. This one is the
 *  property of the timer. When it is launched, _forced_svc_checks data are
 *  transfered to _timer_forced_svc_checks. We keep this set as attribute in
 *  case of the timer fails to send messages. The function is not blocking and
 *  will just make a new attempt in 5s.
 */
class monitoring_stream : public io::stream {
  const std::string _ext_cmd_file;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

  configuration::applier::state _applier;
  /* This mutex is to protect writes to the external command named pipe. */
  mutable std::mutex _ext_cmd_file_m;

  ba_svc_mapping _ba_mapping;
  mutable std::mutex _statusm;
  mysql _mysql;
  unsigned _conf_queries_per_transaction;
  std::unique_ptr<database::bulk_or_multi> _ba_query;
  std::unique_ptr<database::bulk_or_multi> _kpi_query;

  uint32_t _pending_events;
  unsigned _pending_request;
  database_config _storage_db_cfg;
  std::shared_ptr<persistent_cache> _cache;

  asio::steady_timer _forced_svc_checks_timer;
  std::mutex _forced_svc_checks_m;
  std::unordered_set<std::pair<std::string, std::string>,
                     absl::Hash<std::pair<std::string, std::string>>>
      _forced_svc_checks;
  std::unordered_set<std::pair<std::string, std::string>,
                     absl::Hash<std::pair<std::string, std::string>>>
      _timer_forced_svc_checks;
  time_t _last_forced_svc_check;
  bool _forced_svc_checks_timer_stopped;

  void _write_forced_svc_check(const std::string& host,
                               const std::string& description);
  void _explicitly_send_forced_svc_checks(const boost::system::error_code& ec);

  void _prepare();
  void _rebuild();
  void _update_status(std::string const& status);
  void _write_external_command(const std::string& cmd);

  void _read_cache();
  void _write_cache();
  void _execute();

 public:
  monitoring_stream(std::string const& ext_cmd_file,
                    database_config const& db_cfg,
                    database_config const& storage_db_cfg,
                    std::shared_ptr<persistent_cache> cache,
                    const std::shared_ptr<spdlog::logger>& logger);
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
}  // namespace com::centreon::broker

#endif  // !CCB_BAM_MONITORING_STREAM_HH
