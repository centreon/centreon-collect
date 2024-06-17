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

#ifndef CENTREON_AGENT_SCHEDULER_HH
#define CENTREON_AGENT_SCHEDULER_HH

#include "check.hh"

namespace com::centreon::agent {

using export_metric_request =
    ::opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest;
using export_metric_request_ptr = std::shared_ptr<export_metric_request>;

/**
 * @brief the core of the agent
 * It has to create check object with chck_builder passed in parameter of load
 * method It sends metrics to engine and tries to spread checks over check
 * period It also limits concurrent checks in order to limit system load
 */
class scheduler : public std::enable_shared_from_this<scheduler> {
 public:
  using metric_sender = std::function<void(const export_metric_request_ptr&)>;
  using check_builder = std::function<std::shared_ptr<check>(
      const std::shared_ptr<asio::io_context>&,
      const std::shared_ptr<spdlog::logger>& /*logger*/,
      time_point /* start expected*/,
      const std::string& /*host*/,
      const std::string& /*service*/,
      const std::string& /*cmd_name*/,
      const std::string& /*cmd_line*/,
      const engine_to_agent_request_ptr& /*engine to agent request*/,
      check::completion_handler&&)>;

 private:
  using check_queue = std::set<check::pointer, check::pointer_compare>;

  check_queue _check_queue;
  unsigned _active_check = 0;
  bool _alive = true;

  // request that will be sent to engine
  export_metric_request_ptr _current_request;

  struct scope_metric_request {
    ::opentelemetry::proto::metrics::v1::ScopeMetrics* scope_metric;
    absl::flat_hash_map<std::string /*metric name*/,
                        ::opentelemetry::proto::metrics::v1::Metric*>
        metrics;
  };

  absl::flat_hash_map<std::pair<std::string, std::string>, scope_metric_request>
      _host_serv_to_scope_metrics;

  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  metric_sender _metric_sender;
  asio::system_timer _send_timer;
  asio::system_timer _check_timer;
  check_builder _check_builder;
  time_point _next_send_time_point;
  // last received configuration
  engine_to_agent_request_ptr _conf;

  void _start();
  void _start_send_timer();
  void _send_timer_handler(const boost::system::error_code& err);
  void _start_check_timer();
  void _check_timer_handler(const boost::system::error_code& err);

  void _init_export_request();
  void _start_check(const check::pointer& check);
  void _check_handler(
      const check::pointer& check,
      unsigned status,
      const std::list<com::centreon::common::perfdata>& perfdata,
      const std::list<std::string>& outputs);
  void _store_result_in_metrics(
      const check::pointer& check,
      unsigned status,
      const std::list<com::centreon::common::perfdata>& perfdata,
      const std::list<std::string>& outputs);
  void _store_result_in_metrics_and_examplars(
      const check::pointer& check,
      unsigned status,
      const std::list<com::centreon::common::perfdata>& perfdata,
      const std::list<std::string>& outputs);

  scope_metric_request& _get_scope_metrics(const std::string& host,
                                           const std::string& service);

  ::opentelemetry::proto::metrics::v1::Metric* _get_metric(
      scope_metric_request& scope_metric,
      const std::string& metric_name);

  void _add_metric_to_scope(uint64_t now,
                            const com::centreon::common::perfdata& perf,
                            scope_metric_request& scope_metric);

  void _add_exemplar(
      const char* label,
      double value,
      ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point);
  void _add_exemplar(
      const char* label,
      bool value,
      ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point);

  void _start_waiting_check();

 public:
  template <typename sender, typename chck_builder>
  scheduler(const std::shared_ptr<asio::io_context>& io_context,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::shared_ptr<com::centreon::agent::EngineToAgent>& config,
            sender&& met_sender,
            chck_builder&& builder);

  void update(const engine_to_agent_request_ptr& conf);

  static std::shared_ptr<com::centreon::agent::EngineToAgent> default_config();

  template <typename sender, typename chck_builder>
  static std::shared_ptr<scheduler> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::shared_ptr<com::centreon::agent::EngineToAgent>& config,
      sender&& met_sender,
      chck_builder&& chk_builder);

  void stop();
};

/**
 * @brief Construct a new scheduler::scheduler object
 *
 * @tparam sender
 * @param met_sender void(const export_metric_request_ptr&) called each time
 * scheduler wants to send metrics to engine
 * @param io_context
 */
template <typename sender, typename chck_builder>
scheduler::scheduler(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::shared_ptr<com::centreon::agent::EngineToAgent>& config,
    sender&& met_sender,
    chck_builder&& builder)
    : _metric_sender(met_sender),
      _io_context(io_context),
      _logger(logger),
      _send_timer(*io_context),
      _check_timer(*io_context),
      _check_builder(builder),
      _conf(config) {}

/**
 * @brief create and start a new scheduler
 *
 * @tparam sender
 * @param met_sender void(const export_metric_request_ptr&) called each time
 * scheduler wants to send metrics to engine
 * @return std::shared_ptr<scheduler>
 */
template <typename sender, typename chck_builder>
std::shared_ptr<scheduler> scheduler::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::shared_ptr<com::centreon::agent::EngineToAgent>& config,
    sender&& met_sender,
    chck_builder&& chk_builder) {
  std::shared_ptr<scheduler> to_start = std::make_shared<scheduler>(
      io_context, logger, config, std::move(met_sender),
      std::move(chk_builder));
  to_start->_start();
  return to_start;
}

}  // namespace com::centreon::agent

#endif
