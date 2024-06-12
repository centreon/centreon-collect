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

#include "scheduler.hh"
#include "com/centreon/common/utf8.hh"

using namespace com::centreon::agent;

/**
 * @brief to call after creation
 * it create a default configuration with no check and start send timer
 */
void scheduler::_start() {
  _init_export_request();
  _next_send_time_point = std::chrono::system_clock::now();
  update(_conf);
  _start_send_timer();
  _start_check_timer();
}

/**
 * @brief start periodic metric sent to engine
 *
 */
void scheduler::_start_send_timer() {
  _next_send_time_point +=
      std::chrono::seconds(_conf->config().export_period());
  _send_timer.expires_at(_next_send_time_point);
  _send_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_send_timer_handler(err);
      });
}

/**
 * @brief send all check results to engine
 *
 * @param err
 */
void scheduler::_send_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }
  if (_current_request->mutable_otel_request()->resource_metrics_size() > 0) {
    _metric_sender(_current_request);
    _init_export_request();
  }
  _start_send_timer();
}

/**
 * @brief create export request and fill some attributes
 *
 */
void scheduler::_init_export_request() {
  _current_request = std::make_shared<MessageFromAgent>();
  _serv_to_scope_metrics.clear();
}

/**
 * @brief create a default empty configuration to scheduler
 *
 */
std::shared_ptr<com::centreon::agent::MessageToAgent>
scheduler::default_config() {
  std::shared_ptr<com::centreon::agent::MessageToAgent> ret =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  ret->mutable_config()->set_check_interval(1);
  ret->mutable_config()->set_export_period(1);
  ret->mutable_config()->set_max_concurrent_checks(10);
  return ret;
}

/**
 * @brief start check timer.
 * When it will expire, we will call every check whose start_expected is lower
 * than the actual time point
 * if no check available, we start timer for 100ms
 *
 */
void scheduler::_start_check_timer() {
  if (_check_queue.empty() ||
      _active_check >= _conf->config().max_concurrent_checks()) {
    _check_timer.expires_from_now(std::chrono::milliseconds(100));
  } else {
    _check_timer.expires_at((*_check_queue.begin())->get_start_expected());
  }
  _check_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_check_timer_handler(err);
      });
}

/**
 * @brief check timer handler
 *
 * @param err
 */
void scheduler::_check_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }
  _start_waiting_check();
  _start_check_timer();
}

/**
 * @brief start all waiting checks, no more concurrent checks than
 * max_concurrent_checks
 * check started are removed from queue and will be inserted once completed
 */
void scheduler::_start_waiting_check() {
  time_point now = std::chrono::system_clock::now();
  if (!_check_queue.empty()) {
    for (check_queue::iterator to_check = _check_queue.begin();
         !_check_queue.empty() && to_check != _check_queue.end() &&
         (*to_check)->get_start_expected() <= now &&
         _active_check < _conf->config().max_concurrent_checks();) {
      _start_check(*to_check);
      to_check = _check_queue.erase(to_check);
    }
  }
}

/**
 * @brief called when we receive a new configuration
 * It initialize check queue and restart all checks schedule
 * running checks stay alive but their completion will not be handled
 * We compute start_expected of checks in order to spread checks over
 * check_interval
 * @param conf
 */
void scheduler::update(const engine_to_agent_request_ptr& conf) {
  _check_queue.clear();
  _active_check = 0;
  size_t nb_check = conf->config().services().size();

  if (conf->config().check_interval() <= 0) {
    SPDLOG_LOGGER_ERROR(
        _logger, "check_interval cannot be null => no configuration update");
    return;
  }

  SPDLOG_LOGGER_INFO(_logger, "schedule {} checks to execute in {}s", nb_check,
                     conf->config().check_interval());

  if (nb_check > 0) {
    duration check_interval =
        std::chrono::microseconds(conf->config().check_interval() * 1000000) /
        nb_check;

    time_point next = std::chrono::system_clock::now();
    for (const auto& serv : conf->config().services()) {
      if (_logger->level() == spdlog::level::trace) {
        SPDLOG_LOGGER_TRACE(
            _logger, "check expected to start at {} for service {} command {}",
            next, serv.service_description(), serv.command_line());
      } else {
        SPDLOG_LOGGER_TRACE(_logger,
                            "check expected to start at {} for service {}",
                            next, serv.service_description());
      }
      try {
        auto check_to_schedule = _check_builder(
            _io_context, _logger, next, serv.service_description(),
            serv.command_name(), serv.command_line(), conf,
            [me = shared_from_this()](
                const std::shared_ptr<check>& check, unsigned status,
                const std::list<com::centreon::common::perfdata>& perfdata,
                const std::list<std::string>& outputs) {
              me->_check_handler(check, status, perfdata, outputs);
            });
        _check_queue.emplace(check_to_schedule);
        next += check_interval;
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(_logger,
                            "service: {}  command:{} won't be scheduled",
                            serv.service_description(), serv.command_name());
      }
    }
  }

  _conf = conf;
}

/**
 * @brief start a check
 *
 * @param check
 */
void scheduler::_start_check(const check::pointer& check) {
  ++_active_check;
  if (_logger->level() <= spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(_logger, "start check for service {} command {}",
                        check->get_service(), check->get_command_line());
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "start check for service {}",
                        check->get_service());
  }
  check->start_check(std::chrono::seconds(_conf->config().check_timeout()));
}

/**
 * @brief completion check handler
 * if conf has been updated during check, it does nothing
 *
 * @param check
 * @param status
 * @param perfdata
 * @param outputs
 */
void scheduler::_check_handler(
    const check::pointer& check,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  SPDLOG_LOGGER_TRACE(_logger, "end check for service {} command {}",
                      check->get_service(), check->get_command_line());

  // conf has changed => no repush for next check
  if (check->get_conf() != _conf) {
    return;
  }

  if (_conf->config().use_exemplar()) {
    _store_result_in_metrics_and_exemplars(check, status, perfdata, outputs);
  } else {
    _store_result_in_metrics(check, status, perfdata, outputs);
  }

  --_active_check;

  if (_alive) {
    // repush for next check
    check->add_duration_to_start_expected(
        std::chrono::seconds(_conf->config().check_interval()));

    _check_queue.insert(check);
    // we have decreased _active_check, so we can launch another check
    _start_waiting_check();
  }
}

/**
 * @brief to call on process termination or accepted connection error
 *
 */
void scheduler::stop() {
  if (_alive) {
    _alive = false;
    _send_timer.cancel();
    _check_timer.cancel();
  }
}

/**
 * @brief stores results in telegraf manner
 *
 * @param check
 * @param status
 * @param perfdata
 * @param outputs
 */
void scheduler::_store_result_in_metrics(
    const check::pointer& check,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  // auto scope_metrics =
  //     get_scope_metrics(check->get_host(), check->get_service());
  // unsigned now = std::chrono::duration_cast<std::chrono::nanoseconds>(
  //                    std::chrono::system_clock::now().time_since_epoch())
  //                    .count();

  // auto state_metrics = scope_metrics->add_metrics();
  // state_metrics->set_name(check->get_command_name() + "_state");
  // if (!outputs.empty()) {
  //   const std::string& first_line = *outputs.begin();
  //   size_t pipe_pos = first_line.find('|');
  //   state_metrics->set_description(pipe_pos != std::string::npos
  //                                      ? first_line.substr(0, pipe_pos)
  //                                      : first_line);
  // }
  // auto data_point = state_metrics->mutable_gauge()->add_data_points();
  // data_point->set_time_unix_nano(now);
  // data_point->set_as_int(status);

  // we aggregate perfdata results by type (min, max, )
}

/**
 * @brief store results with centreon sauce
 *
 * @param check
 * @param status
 * @param perfdata
 * @param outputs
 */
void scheduler::_store_result_in_metrics_and_exemplars(
    const check::pointer& check,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  auto& scope_metrics = _get_scope_metrics(check->get_service());
  uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

  auto state_metrics = _get_metric(scope_metrics, "status");
  if (!outputs.empty()) {
    const std::string& first_line = *outputs.begin();
    size_t pipe_pos = first_line.find('|');
    state_metrics->set_description(common::check_string_utf8(
        pipe_pos != std::string::npos ? first_line.substr(0, pipe_pos)
                                      : first_line));
  }
  auto data_point = state_metrics->mutable_gauge()->add_data_points();
  data_point->set_time_unix_nano(now);
  data_point->set_as_int(status);

  for (const com::centreon::common::perfdata& perf : perfdata) {
    _add_metric_to_scope(now, perf, scope_metrics);
  }
}

/**
 * @brief metrics are grouped by host service
 * (one resource_metrics by host serv pair)
 *
 * @param service
 * @return scheduler::scope_metric_request&
 */
scheduler::scope_metric_request& scheduler::_get_scope_metrics(
    const std::string& service) {
  auto exist = _serv_to_scope_metrics.find(service);
  if (exist != _serv_to_scope_metrics.end()) {
    return exist->second;
  }
  ::opentelemetry::proto::metrics::v1::ResourceMetrics* new_res =
      _current_request->mutable_otel_request()->add_resource_metrics();

  auto* host_attrib = new_res->mutable_resource()->add_attributes();
  host_attrib->set_key("host.name");
  host_attrib->mutable_value()->set_string_value(_supervised_host);
  auto* serv_attrib = new_res->mutable_resource()->add_attributes();
  serv_attrib->set_key("service.name");
  serv_attrib->mutable_value()->set_string_value(service);

  ::opentelemetry::proto::metrics::v1::ScopeMetrics* new_scope =
      new_res->add_scope_metrics();

  scope_metric_request to_insert;
  to_insert.scope_metric = new_scope;

  return _serv_to_scope_metrics.emplace(service, to_insert).first->second;
}

/**
 * @brief one metric by metric name (can contains several datapoints in case of
 * multiple checks during send period )
 *
 * @param scope_metric
 * @param metric_name
 * @return ::opentelemetry::proto::metrics::v1::Metric*
 */
::opentelemetry::proto::metrics::v1::Metric* scheduler::_get_metric(
    scope_metric_request& scope_metric,
    const std::string& metric_name) {
  auto exist = scope_metric.metrics.find(metric_name);
  if (exist != scope_metric.metrics.end()) {
    return exist->second;
  }

  ::opentelemetry::proto::metrics::v1::Metric* new_metric =
      scope_metric.scope_metric->add_metrics();
  new_metric->set_name(metric_name);

  scope_metric.metrics.emplace(metric_name, new_metric);

  return new_metric;
}

/**
 * @brief add a perfdata to metric
 *
 * @param now
 * @param perf
 * @param scope_metric
 */
void scheduler::_add_metric_to_scope(
    uint64_t now,
    const com::centreon::common::perfdata& perf,
    scope_metric_request& scope_metric) {
  auto metric = _get_metric(scope_metric, perf.name());
  metric->set_unit(perf.unit());
  auto data_point = metric->mutable_gauge()->add_data_points();
  data_point->set_as_double(perf.value());
  data_point->set_time_unix_nano(now);
  switch (perf.value_type()) {
    case com::centreon::common::perfdata::counter: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("counter");
      break;
    }
    case com::centreon::common::perfdata::derive: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("derive");
      break;
    }
    case com::centreon::common::perfdata::absolute: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("absolute");
      break;
    }
    case com::centreon::common::perfdata::automatic: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("auto");
      break;
    }
  }
  if (perf.critical() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.critical_mode() ? "crit_ge" : "crit_gt", perf.critical(),
                  *data_point);
  }
  if (perf.critical_low() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.critical_mode() ? "crit_le" : "crit_lt",
                  perf.critical_low(), *data_point);
  }
  if (perf.warning() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.warning_mode() ? "warn_ge" : "warn_gt", perf.warning(),
                  *data_point);
  }
  if (perf.warning_low() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.critical_mode() ? "warn_le" : "warn_lt",
                  perf.warning_low(), *data_point);
  }
  if (perf.min() <= std::numeric_limits<double>::max()) {
    _add_exemplar("min", perf.min(), *data_point);
  }
  if (perf.max() <= std::numeric_limits<double>::max()) {
    _add_exemplar("max", perf.max(), *data_point);
  }
}

/**
 * @brief add an exemplar to metric such as crit_le, min, max..
 *
 * @param label
 * @param value
 * @param data_point
 */
void scheduler::_add_exemplar(
    const char* label,
    double value,
    ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point) {
  auto exemplar = data_point.add_exemplars();
  auto attrib = exemplar->add_filtered_attributes();
  attrib->set_key(label);
  exemplar->set_as_double(value);
}

/**
 * @brief add an exemplar to metric such as crit_le, min, max..
 *
 * @param label
 * @param value
 * @param data_point
 */
void scheduler::_add_exemplar(
    const char* label,
    bool value,
    ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point) {
  auto exemplar = data_point.add_exemplars();
  auto attrib = exemplar->add_filtered_attributes();
  attrib->set_key(label);
  exemplar->set_as_int(value);
}
