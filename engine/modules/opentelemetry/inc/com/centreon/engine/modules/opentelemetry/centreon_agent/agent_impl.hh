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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_IMPL_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_IMPL_HH

#include "agent_stat.hh"

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_data_point.hh"

#include "com/centreon/engine/modules/opentelemetry/conf_helper.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

/**
 * @brief this class manages connection with centreon monitoring agent
 * reverse connection or no
 *
 * @tparam bireactor_class (grpc::bireactor<,>)
 */
template <class bireactor_class>
class agent_impl
    : public bireactor_class,
      public std::enable_shared_from_this<agent_impl<bireactor_class>> {
  std::shared_ptr<boost::asio::io_context> _io_context;
  const std::string_view _class_name;
  const bool _reversed;
  const std::chrono::system_clock::time_point _exp_time;

  whitelist_cache _whitelist_cache;

  agent_config::pointer _conf ABSL_GUARDED_BY(_protect);
  bool _credentials_encrypted ABSL_GUARDED_BY(_protect);

  metric_handler _metric_handler;

  std::shared_ptr<agent::MessageFromAgent> _agent_info
      ABSL_GUARDED_BY(_protect);
  std::shared_ptr<agent::MessageToAgent> _last_sent_config
      ABSL_GUARDED_BY(_protect);

  static std::set<std::shared_ptr<agent_impl>>* _instances
      ABSL_GUARDED_BY(_instances_m);
  static absl::Mutex _instances_m;

  bool _write_pending;
  std::deque<std::shared_ptr<agent::MessageToAgent>> _write_queue
      ABSL_GUARDED_BY(_protect);
  std::shared_ptr<agent::MessageFromAgent> _read_current
      ABSL_GUARDED_BY(_protect);

  void _calc_and_send_config_if_needed();

  virtual const std::string& get_peer() const = 0;

  void _write(const std::shared_ptr<agent::MessageToAgent>& request);

 protected:
  std::shared_ptr<spdlog::logger> _logger;
  bool _alive ABSL_GUARDED_BY(_protect);

  agent_stat::pointer _stats;

  mutable absl::Mutex _protect;

 public:
  agent_impl(const std::shared_ptr<boost::asio::io_context>& io_context,
             const std::string_view class_name,
             const agent_config::pointer& conf,
             const metric_handler& handler,
             const std::shared_ptr<spdlog::logger>& logger,
             bool reversed,
             const agent_stat::pointer& stats);

  agent_impl(const std::shared_ptr<boost::asio::io_context>& io_context,
             const std::string_view class_name,
             const agent_config::pointer& conf,
             const metric_handler& handler,
             const std::shared_ptr<spdlog::logger>& logger,
             bool reversed,
             const agent_stat::pointer& stats,
             const std::chrono::system_clock::time_point& exp_time);

  virtual ~agent_impl();

  void calc_and_send_config_if_needed(const agent_config::pointer& new_conf);

  static void all_agent_calc_and_send_config_if_needed(
      const agent_config::pointer& new_conf);

  static void update_config();

  void on_request(const std::shared_ptr<agent::MessageFromAgent>& request);

  static void register_stream(const std::shared_ptr<agent_impl>& strm);

  void start_read();

  void start_write();

  // bireactor part
  void OnReadDone(bool ok) override;

  virtual void on_error() = 0;

  void OnWriteDone(bool ok) override;

  // server version
  void OnDone();
  // client version
  void OnDone(const ::grpc::Status& /*s*/);

  virtual void shutdown();

  static void shutdown_all();
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
