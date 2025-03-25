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

/**
 * @brief the core of the agent
 * It has to create check object with chck_builder passed in parameter of load
 * method It sends metrics to engine and tries to spread checks over check
 * period It also limits concurrent checks in order to limit system load
 */
class scheduler : public std::enable_shared_from_this<scheduler> {
 public:
  using metric_sender =
      std::function<void(const std::shared_ptr<MessageFromAgent>&)>;
  using check_builder = std::function<std::shared_ptr<check>(
      const std::shared_ptr<asio::io_context>&,
      const std::shared_ptr<spdlog::logger>& /*logger*/,
      time_point /* start expected*/,
      duration /* check interval */,
      const std::string& /*service*/,
      const std::string& /*cmd_name*/,
      const std::string& /*cmd_line*/,
      const engine_to_agent_request_ptr& /*engine to agent request*/,
      check::completion_handler&&,
      const checks_statistics::pointer& /*stat*/)>;

 private:
  using check_queue =
      absl::btree_set<check::pointer, check::pointer_start_compare>;

  check_queue _waiting_check_queue;
  // running check counter that must not exceed max_concurrent_check
  unsigned _active_check = 0;
  bool _alive = true;

  // request that will be sent to engine
  std::shared_ptr<MessageFromAgent> _current_request;

  // pointers in this struct point to _current_request
  struct scope_metric_request {
    ::opentelemetry::proto::metrics::v1::ScopeMetrics* scope_metric;
    std::unordered_map<std::string /*metric name*/,
                       ::opentelemetry::proto::metrics::v1::Metric*>
        metrics;
  };

  // one serv => one scope_metric => several metrics
  std::unordered_map<std::string, scope_metric_request> _serv_to_scope_metrics;

  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  // host declared in engine config
  std::string _supervised_host;
  metric_sender _metric_sender;
  asio::system_timer _send_timer;
  asio::system_timer _check_timer;
  time_step
      _check_time_step;  // time point used when too many checks are running
  check_builder _check_builder;
  // in order to send check_results at regular intervals, we work with absolute
  // time points that we increment
  time_point _next_send_time_point;
  // last received configuration
  engine_to_agent_request_ptr _conf;

  // As protobuf message calculation can be expensive, we measure size of first protobuf message of ten metrics for example,
  // then we devide it by the number of metrics and we store it in this variable
  // For the next frames, we multiply metrics number by this variable to estimate message length
  unsigned _average_metric_length;

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
  void _store_result_in_metrics_and_exemplars(
      const check::pointer& check,
      unsigned status,
      const std::list<com::centreon::common::perfdata>& perfdata,
      const std::list<std::string>& outputs);

  scope_metric_request& _get_scope_metrics(const std::string& service);

  ::opentelemetry::proto::metrics::v1::Metric* _get_metric(
      scope_metric_request& scope_metric,
      const std::string& metric_name);

  void _add_metric_to_scope(uint64_t check_start,
                            uint64_t now,
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
            const std::string& supervised_host,
            const std::shared_ptr<com::centreon::agent::MessageToAgent>& config,
            sender&& met_sender,
            chck_builder&& builder);

  scheduler(const scheduler&) = delete;
  scheduler operator=(const scheduler&) = delete;

  void update(const engine_to_agent_request_ptr& conf);

  static std::shared_ptr<com::centreon::agent::MessageToAgent> default_config();

  template <typename sender, typename chck_builder>
  static std::shared_ptr<scheduler> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::string& supervised_host,
      const std::shared_ptr<com::centreon::agent::MessageToAgent>& config,
      sender&& met_sender,
      chck_builder&& chk_builder);

  void stop();

  static std::shared_ptr<check> default_check_builder(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      time_point first_start_expected,
      duration check_interval,
      const std::string& service,
      const std::string& cmd_name,
      const std::string& cmd_line,
      const engine_to_agent_request_ptr& conf,
      check::completion_handler&& handler,
      const checks_statistics::pointer& stat);

  engine_to_agent_request_ptr get_last_message_to_agent() const {
    return _conf;
  }
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
    const std::string& supervised_host,
    const std::shared_ptr<com::centreon::agent::MessageToAgent>& config,
    sender&& met_sender,
    chck_builder&& builder)
    : _io_context(io_context),
      _logger(logger),
      _supervised_host(supervised_host),
      _metric_sender(met_sender),
      _send_timer(*io_context),
      _check_timer(*io_context),
      _check_builder(builder),
      _conf(config),
      _average_metric_length(0) {}

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
    const std::string& supervised_host,
    const std::shared_ptr<com::centreon::agent::MessageToAgent>& config,
    sender&& met_sender,
    chck_builder&& chk_builder) {
  std::shared_ptr<scheduler> to_start = std::make_shared<scheduler>(
      io_context, logger, supervised_host, config, std::move(met_sender),
      std::move(chk_builder));
  to_start->_start();
  return to_start;
}

}  // namespace com::centreon::agent

#endif
