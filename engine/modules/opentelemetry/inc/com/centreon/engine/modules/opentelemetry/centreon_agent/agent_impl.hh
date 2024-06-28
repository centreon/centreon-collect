/*
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_IMPL_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_IMPL_HH

#include "centreon_agent/agent.grpc.pb.h"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_data_point.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

template <class bireactor_class>
class agent_impl
    : public bireactor_class,
      public std::enable_shared_from_this<agent_impl<bireactor_class>> {
  std::shared_ptr<boost::asio::io_context> _io_context;
  const std::string_view _class_name;

  agent_config::pointer _conf ABSL_GUARDED_BY(_protect);

  metric_handler _metric_handler;

  std::shared_ptr<agent::MessageFromAgent> _agent_info
      ABSL_GUARDED_BY(_protect);
  std::shared_ptr<agent::MessageToAgent> _last_sent_config
      ABSL_GUARDED_BY(_protect);

  static std::set<std::shared_ptr<agent_impl>> _instances
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
  mutable absl::Mutex _protect;

 public:
  agent_impl(const std::shared_ptr<boost::asio::io_context>& io_context,
             const std::string_view class_name,
             const agent_config::pointer& conf,
             const metric_handler& handler,
             const std::shared_ptr<spdlog::logger>& logger);

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
