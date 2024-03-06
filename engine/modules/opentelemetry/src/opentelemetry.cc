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

#include "com/centreon/exceptions/msg_fmt.hh"

#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/modules/opentelemetry/opentelemetry.hh"

#include "otl_fmt.hh"
#include "otl_server.hh"

using namespace com::centreon::engine::modules::opentelemetry;

/**
 * @brief Construct a new open telemetry::open telemetry object
 *
 * @param config_file_path
 * @param io_context
 */
open_telemetry::open_telemetry(
    const std::string_view config_file_path,
    const std::shared_ptr<asio::io_context>& io_context)
    : _config_file_path(config_file_path),
      _second_timer(*io_context),
      _io_context(io_context) {
  SPDLOG_LOGGER_INFO(log_v2::otl(), "load of open telemetry module");
}

/**
 * @brief to call when configuration changes
 *
 */
void open_telemetry::_reload() {
  std::unique_ptr<otl_config> new_conf =
      std::make_unique<otl_config>(_config_file_path);
  if (!_conf || !(*new_conf->get_grpc_config() == *_conf->get_grpc_config())) {
    this->_create_otl_server(new_conf->get_grpc_config());
  }
  if (!_conf || !(*_conf == *new_conf)) {
    fmt::formatter<::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>::max_length_log =
        new_conf->get_max_length_grpc_log();
    fmt::formatter<::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>::json_grpc_format =
        new_conf->get_json_grpc_log();
    data_point_fifo::update_fifo_limit(new_conf->get_second_fifo_expiry(),
                                       new_conf->get_max_fifo_size());

    _conf = std::move(new_conf);
  }
}

/**
 * @brief static creator of singleton
 *
 * @param config_path
 * @param io_context
 * @return std::shared_ptr<open_telemetry>
 */
std::shared_ptr<open_telemetry> open_telemetry::load(
    const std::string_view& config_path,
    const std::shared_ptr<asio::io_context>& io_context) {
  if (!_instance) {
    _instance = std::make_shared<open_telemetry>(config_path, io_context);
    instance()->_reload();
    instance()->_start_second_timer();
  }
  return instance();
}

/**
 * @brief create grpc server witch accept otel collector connections
 *
 * @param server_conf json server config
 */
void open_telemetry::_create_otl_server(
    const grpc_config::pointer& server_conf) {
  std::shared_ptr<otl_server> to_shutdown = _otl_server;
  if (to_shutdown) {
    to_shutdown->shutdown(std::chrono::seconds(10));
  }
  _otl_server = otl_server::load(
      server_conf,
      [me = shared_from_this()](const metric_request_ptr& request) {
        me->_on_metric(request);
      });
}

/**
 * @brief static method used to make singleton reload is configuration
 *
 */
void open_telemetry::reload() {
  if (_instance) {
    try {
      SPDLOG_LOGGER_INFO(log_v2::otl(), "reload of open telemetry module");
      instance()->_reload();
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(
          log_v2::otl(),
          "bad configuration, new configuration  not taken into account: {}",
          e.what());
    }
  }
}

/**
 * @brief shutdown singleton and dereference it. It may continue to live until
 * no object has a reference on it
 *
 */
void open_telemetry::unload() {
  if (_instance) {
    SPDLOG_LOGGER_INFO(log_v2::otl(), "unload of open telemetry module");
    instance()->_shutdown();
    _instance.reset();
  }
}

/**
 * @brief shutdown grpc server and stop second timer
 *
 */
void open_telemetry::_shutdown() {
  std::shared_ptr<otl_server> to_shutdown = _otl_server;
  _otl_server.reset();
  if (to_shutdown) {
    to_shutdown->shutdown(std::chrono::seconds(10));
  }
  std::lock_guard l(_protect);
  _second_timer.cancel();
}

/**
 * @brief create an host serv extractor from connector command line
 *
 * @param cmdline witch begins with name of extractor, following parameters are
 * used by extractor
 * @return
 * std::shared_ptr<com::centreon::engine::commands::otel::host_serv_extractor>
 * @throw if extractor type is unknown
 */
std::shared_ptr<com::centreon::engine::commands::otel::host_serv_extractor>
open_telemetry::create_extractor(
    const std::string& cmdline,
    const commands::otel::host_serv_list::pointer& host_serv_list) {
  // erase host serv extractors that are only owned by this object
  auto clean = [this]() {
    for (cmd_line_to_extractor_map::const_iterator to_test =
             _extractors.begin();
         !_extractors.empty() && to_test != _extractors.end();) {
      if (to_test->second.use_count() <= 1) {
        SPDLOG_LOGGER_DEBUG(log_v2::otl(), "create extractor:{}",
                            *to_test->second);
        to_test = _extractors.erase(to_test);
      } else {
        ++to_test;
      }
    }
  };
  std::lock_guard l(_protect);
  auto exist = _extractors.find(cmdline);
  if (exist != _extractors.end()) {
    std::shared_ptr<com::centreon::engine::commands::otel::host_serv_extractor>
        to_ret = exist->second;
    clean();
    return to_ret;
  }
  clean();
  try {
    std::shared_ptr<host_serv_extractor> new_extractor =
        host_serv_extractor::create(cmdline, host_serv_list);
    _extractors.emplace(cmdline, new_extractor);
    SPDLOG_LOGGER_DEBUG(log_v2::otl(), "create extractor:{}", *new_extractor);
    return new_extractor;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::otl(), "fail to create extractor \"{}\" : {}",
                        cmdline, e.what());
    throw;
  }
}

/**
 * @brief simulate a check by reading in metrics fifos
 * It creates an otel_converter, the first word of processed_cmd is the name of
 * converter such as nagios_telegraf. Following parameters are used by converter
 *
 * @param processed_cmd converter type with arguments
 * @param command_id command id
 * @param macros
 * @param timeout
 * @param res filled if it returns true
 * @param handler called later if it returns false
 * @return true res is filled with a result
 * @return false result will be passed to handler as soon as available or
 * timeout
 * @throw  if converter type is unknown
 */
bool open_telemetry::check(const std::string& processed_cmd,
                           uint64_t command_id,
                           nagios_macros& macros,
                           uint32_t timeout,
                           commands::result& res,
                           commands::otel::result_callback&& handler) {
  std::shared_ptr<otl_converter> to_use;
  try {
    to_use = otl_converter::create(
        processed_cmd, command_id, *macros.host_ptr, macros.service_ptr,
        std::chrono::system_clock::now() + std::chrono::seconds(timeout),
        std::move(handler));
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::otl(), "fail to create converter for {} : {}",
                        processed_cmd, e.what());
    throw;
  };

  bool res_available = to_use->sync_build_result_from_metrics(_fifo, res);

  if (res_available) {
    SPDLOG_LOGGER_TRACE(log_v2::otl(),
                        "data available for command {} converter:{}",
                        command_id, *to_use);
    return true;
  }

  SPDLOG_LOGGER_TRACE(
      log_v2::otl(), "data unavailable for command {} timeout: {} converter:{}",
      command_id, timeout, *to_use);

  // metrics not yet available = wait for data or until timeout
  std::lock_guard l(_protect);
  _waiting.insert(to_use);

  return false;
}

/**
 * @brief called on metric reception
 * we first fill data_point fifos and then if a converter of a service is
 * waiting for data_point, we call him. If it achieve to generate a check
 * result, the handler of otl_converter is called
 * unknown metrics are passed to _forward_to_broker
 *
 * @param metrics collector request
 */
void open_telemetry::_on_metric(const metric_request_ptr& metrics) {
  std::vector<data_point> unknown;
  {
    std::lock_guard l(_protect);
    if (_extractors.empty()) {
      data_point::extract_data_points(metrics,
                                      [&unknown](const data_point& data_pt) {
                                        unknown.push_back(data_pt);
                                      });
    } else {
      waiting_converter::nth_index<0>::type& host_serv_index =
          _waiting.get<0>();
      std::vector<std::shared_ptr<otl_converter>> to_notify;
      auto last_success = _extractors.begin();
      data_point::extract_data_points(metrics, [this, &unknown, &last_success,
                                                &host_serv_index, &to_notify](
                                                   const data_point& data_pt) {
        bool data_point_known = false;
        // we try all extractors and we begin with the last witch has achieved
        // to extract host
        for (unsigned tries = 0; tries < _extractors.size(); ++tries) {
          host_serv_metric hostservmetric =
              last_success->second->extract_host_serv_metric(data_pt);

          if (!hostservmetric.host.empty()) {
            _fifo.add_data_point(hostservmetric.host, hostservmetric.service,
                                 hostservmetric.metric, data_pt);

            auto waiting = host_serv_index.equal_range(
                host_serv{hostservmetric.host, hostservmetric.service});
            while (waiting.first != waiting.second) {
              to_notify.push_back(*waiting.first);
              waiting.first = host_serv_index.erase(waiting.first);
            }
            data_point_known = true;
            break;
          }
          ++last_success;
          if (last_success == _extractors.end()) {
            last_success = _extractors.begin();
          }
        }
        if (!data_point_known) {
          unknown.push_back(data_pt);  // unknown metric => forward to broker
        }
      });
      SPDLOG_LOGGER_TRACE(log_v2::otl(), "fifos:{}", _fifo);
      // we wait that all request datas have been computed to give us more
      // chance of converter success
      for (auto to_callback : to_notify) {
        if (!to_callback->async_build_result_from_metrics(
                _fifo)) {  // not enough data => repush in _waiting
          _waiting.insert(to_callback);
        }
      }
      SPDLOG_LOGGER_TRACE(log_v2::otl(), "fifos:{}", _fifo);
    }
  }
  if (!unknown.empty()) {
    SPDLOG_LOGGER_TRACE(log_v2::otl(), "{} unknown data_points",
                        unknown.size());
    _forward_to_broker(unknown);
  }
}

/**
 * @brief the second timer is used to handle converter timeouts
 *
 */
void open_telemetry::_start_second_timer() {
  std::lock_guard l(_protect);
  _second_timer.expires_from_now(std::chrono::seconds(1));
  _second_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          me->_second_timer_handler();
        }
      });
}

/**
 * @brief notify all timeouts
 *
 */
void open_telemetry::_second_timer_handler() {
  std::vector<std::shared_ptr<otl_converter>> to_notify;
  {
    std::lock_guard l(_protect);
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    waiting_converter::nth_index<1>::type& expiry_index = _waiting.get<1>();
    while (!_waiting.empty()) {
      auto oldest = expiry_index.begin();
      if ((*oldest)->get_time_out() > now) {
        break;
      }
      to_notify.push_back(*oldest);
      expiry_index.erase(oldest);
    }
  }

  // notify all timeout
  for (std::shared_ptr<otl_converter> to_not : to_notify) {
    SPDLOG_LOGGER_DEBUG(log_v2::otl(), "time out: {}", *to_not);
    to_not->async_time_out();
  }

  _start_second_timer();
}

void open_telemetry::_forward_to_broker(
    const std::vector<data_point>& unknown) {}
