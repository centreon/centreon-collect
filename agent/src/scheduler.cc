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
#include "check.hh"
#include "check_cpu.hh"
#include "check_health.hh"
#include "config.hh"
#ifdef _WIN32
#include "check_counter.hh"
#include "check_event_log.hh"
#include "check_memory.hh"
#include "check_process.hh"
#include "check_service.hh"
#include "check_uptime.hh"
#endif
#include "check_exec.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/common/utf8.hh"
#include "drive_size.hh"

using namespace com::centreon::agent;

/**
 * @brief to call after creation
 * it create a default configuration with no check and start send timer
 */
void scheduler::_start() {
  _init_export_request();
  _next_send_time_point = std::chrono::system_clock::now();
  _check_time_step =
      time_step(_next_send_time_point, std::chrono::milliseconds(100));
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
  _check_time_step.increment_to_after_now();
  _check_timer.expires_at(_check_time_step.value());
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
  if (!_waiting_check_queue.empty()) {
    for (check_queue::iterator to_check = _waiting_check_queue.begin();
         !_waiting_check_queue.empty() &&
         to_check != _waiting_check_queue.end() &&
         to_check->second->get_start_expected() <= now &&
         _active_check < _conf->config().max_concurrent_checks();) {
      _start_check(to_check->second);
      to_check = _waiting_check_queue.erase(to_check);
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
  _waiting_check_queue.clear();
  _active_check = 0;
  size_t nb_check = conf->config().services().size();

  if (nb_check > 0) {
    // raz stats in order to not keep statistics of deleted checks
    checks_statistics::pointer statistics =
        std::make_shared<checks_statistics>();

    // first we group checks by check_interval
    std::map<uint32_t, std::vector<const Service*>> group_serv;
    for (const auto& serv : conf->config().services()) {
      uint32_t check_interval = serv.check_interval();
      if (check_interval == 0) {
        check_interval = 60;  // one minute by default
      }
      group_serv[check_interval].push_back(&serv);
    }

    srand(time(nullptr));
    std::chrono::milliseconds first_inter_check_delay(
        (group_serv.begin()->first * 1000) / nb_check);
    // in order to avoid collision when we will use a time_step equal to
    // first_inter_check_delay / 2 with a little random
    duration time_unit = first_inter_check_delay / 2 +
                         std::chrono::milliseconds(
                             rand() % (first_inter_check_delay.count() / 10));

    std::chrono::seconds accuracy(conf->config().max_check_interval_error());
    if (accuracy.count() == 0) {
      accuracy = std::chrono::seconds(5);
    }
    // we need to respect check_interval accuracy
    while (1) {
      bool need_to_continue = false;
      for (const auto& [interval, _] : group_serv) {
        if (std::chrono::seconds(interval) % time_unit > accuracy) {
          time_unit -= time_unit / 10;
          need_to_continue = true;
          break;
        }
      }
      if (!need_to_continue) {
        break;
      }
    }

    SPDLOG_LOGGER_DEBUG(_logger, "all checks will use a time step of {}",
                        time_unit);

    auto group_iter = group_serv.begin();

    time_point next = std::chrono::system_clock::now();

    _check_time_step = time_step(next, time_unit);

    auto last_inserted_iter = _waiting_check_queue.end();
    unsigned step_index = 0;
    while (true) {
      const auto& serv = **group_iter->second.rbegin();

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
        std::chrono::seconds check_interval(serv.check_interval());
        if (!check_interval.count()) {
          check_interval = std::chrono::seconds(60);
        }
        auto check_to_schedule = _check_builder(
            _io_context, _logger, next, check_interval,
            serv.service_description(), serv.command_name(),
            serv.command_line(), conf,
            [me = shared_from_this()](
                const std::shared_ptr<check>& check, unsigned status,
                const std::list<com::centreon::common::perfdata>& perfdata,
                const std::list<std::string>& outputs) {
              me->_check_handler(check, status, perfdata, outputs);
            },
            statistics);
        last_inserted_iter = _waiting_check_queue.emplace_hint(
            last_inserted_iter, step_index, check_to_schedule);
        next += first_inter_check_delay;
        ++step_index;
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(
            _logger, "service: {}  command:{} won't be scheduled cause: {}",
            serv.service_description(), serv.command_name(), e.what());
      }
      group_iter->second.pop_back();
      if (group_iter->second.empty()) {
        group_iter = group_serv.erase(group_iter);
      } else {
        ++group_iter;
      }
      if (group_serv.empty()) {
        break;
      }
      if (group_iter == group_serv.end()) {
        group_iter = group_serv.begin();
      }
    }
  }

  _conf = conf;

  _start_waiting_check();
  _start_check_timer();
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
  SPDLOG_LOGGER_DEBUG(_logger, "end check for service {} command {}",
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
    time_point now = std::chrono::system_clock::now();

    // repush for next check and search a free start slot in queue
    check->increment_start_expected_to_after_min_timepoint(now);

    time_step slot_search(_check_time_step);
    slot_search.increment_to_after_min(check->get_start_expected());
    uint64_t steps = slot_search.get_step_index();
    while (!_waiting_check_queue.emplace(steps, check).second) {
      // slot yet reserved => try next
      ++steps;
    }
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
    [[maybe_unused]] const check::pointer& check,
    [[maybe_unused]] unsigned status,
    [[maybe_unused]] const std::list<com::centreon::common::perfdata>& perfdata,
    [[maybe_unused]] const std::list<std::string>& outputs) {
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
  // we don't want to erase existing previous metrics, so we send right now
  auto exist = _serv_to_scope_metrics.find(check->get_service());
  if (exist != _serv_to_scope_metrics.end()) {
    _metric_sender(_current_request);
    _init_export_request();
  }

  auto& scope_metrics = _get_scope_metrics(check->get_service());
  uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  uint64_t check_start = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             check->get_last_start().time_since_epoch())
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
  data_point->set_start_time_unix_nano(check_start);
  data_point->set_as_int(status);

  for (const com::centreon::common::perfdata& perf : perfdata) {
    _add_metric_to_scope(check_start, now, perf, scope_metrics);
  }
  if (!_average_metric_length &&
      _current_request->otel_request().resource_metrics_size() > 10) {
    _average_metric_length =
        _current_request->ByteSizeLong() /
        _current_request->otel_request().resource_metrics_size();
  }
  if (_current_request->otel_request().resource_metrics_size() *
          _average_metric_length >
      2 * 1024 * 1024) {
    _metric_sender(_current_request);
    _init_export_request();
  }
}

/**
 * @brief metrics are grouped by host service
 * (one resource_metrics by host serv pair)
 * no resource_metrics for this service must exist before calling this function
 * @param service
 * @return a new scheduler::scope_metric_request&
 */
scheduler::scope_metric_request& scheduler::_get_scope_metrics(
    const std::string& service) {
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
    uint64_t check_start,
    uint64_t now,
    const com::centreon::common::perfdata& perf,
    scope_metric_request& scope_metric) {
  auto metric = _get_metric(scope_metric, perf.name());
  metric->set_unit(perf.unit());
  auto data_point = metric->mutable_gauge()->add_data_points();
  data_point->set_as_double(perf.value());
  data_point->set_time_unix_nano(now);
  data_point->set_start_time_unix_nano(check_start);
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
    case com::centreon::common::perfdata::gauge: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("gauge");
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

/**
 * @brief build a check object from command lline
 *
 * @param io_context
 * @param logger logger that will be used by the check
 * @param start_expected timepoint of first check
 * @param service
 * @param cmd_name
 * @param cmd_line
 * @param conf conf given by engine
 * @param handler handler that will be called on check completion
 * @return std::shared_ptr<check>
 */
std::shared_ptr<check> scheduler::default_check_builder(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration check_interval,
    const std::string& service,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const engine_to_agent_request_ptr& conf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat) {
  using namespace std::literals;
  // test native checks where cmd_lin is a json
  try {
    rapidjson::Document native_check_info =
        common::rapidjson_helper::read_from_string(cmd_line);
    common::rapidjson_helper native_params(native_check_info);
    try {
      std::string_view check_type = native_params.get_string("check");
      const rapidjson::Value* args;
      if (native_params.has_member("args")) {
        args = &native_params.get_member("args");
      } else {
        static const rapidjson::Value no_arg;
        args = &no_arg;
      }

      if (check_type == "cpu_percentage"sv) {
        return std::make_shared<check_cpu>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "health"sv) {
        return std::make_shared<check_health>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
#ifdef _WIN32
      } else if (check_type == "uptime"sv) {
        return std::make_shared<check_uptime>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "storage"sv) {
        return std::make_shared<check_drive_size>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "memory"sv) {
        return std::make_shared<check_memory>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "service"sv) {
        return std::make_shared<check_service>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "counter"sv) {
        return std::make_shared<check_counter>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "eventlog_nscp"sv) {
        return check_event_log::load(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
      } else if (check_type == "process_nscp"sv) {
        return std::make_shared<check_process>(
            io_context, logger, first_start_expected, check_interval, service,
            cmd_name, cmd_line, *args, conf, std::move(handler), stat);
#endif
      } else {
        throw exceptions::msg_fmt("command {}, unknown native check:{}",
                                  cmd_name, cmd_line);
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "unexpected error: {}", e.what());
      return check_dummy::load(io_context, logger, first_start_expected,
                               check_interval, service, cmd_name, cmd_line,
                               std::string(e.what()), conf, std::move(handler),
                               stat);
    }
  } catch (const std::exception&) {
    return check_exec::load(io_context, logger, first_start_expected,
                            check_interval, service, cmd_name, cmd_line, conf,
                            std::move(handler), stat);
  }
}
