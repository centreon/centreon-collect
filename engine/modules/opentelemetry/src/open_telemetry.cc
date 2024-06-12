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

#include "centreon_agent/agent_impl.hh"
#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/engine/modules/opentelemetry/open_telemetry.hh"

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
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger)
    : _second_timer(*io_context),
      _config_file_path(config_file_path),
      _logger(logger),
      _io_context(io_context) {
  SPDLOG_LOGGER_INFO(_logger, "load of open telemetry module");
}

/**
 * @brief to call when configuration changes
 *
 */
void open_telemetry::_reload() {
  std::unique_ptr<otl_config> new_conf =
      std::make_unique<otl_config>(_config_file_path, *_io_context);

  if (new_conf->get_grpc_config()) {
    if (!_conf || !_conf->get_grpc_config() ||
        *new_conf->get_grpc_config() != *_conf->get_grpc_config()) {
      this->_create_otl_server(new_conf->get_grpc_config(),
                               new_conf->get_centreon_agent_config());
    }
    if (_conf && _conf->get_centreon_agent_config() &&
        *_conf->get_centreon_agent_config() !=
            *new_conf->get_centreon_agent_config()) {
      _otl_server->update_agent_config(new_conf->get_centreon_agent_config());
    }
  } else {  // only reverse connection
    std::shared_ptr<otl_server> to_shutdown = std::move(_otl_server);
    if (to_shutdown) {
      to_shutdown->shutdown(std::chrono::seconds(10));
    }
  }

  if (!new_conf->get_telegraf_conf_server_config()) {
    if (_telegraf_conf_server) {
      _telegraf_conf_server->shutdown();
      _telegraf_conf_server.reset();
    }
  } else if (!_conf || !_conf->get_telegraf_conf_server_config() ||
             !(*new_conf->get_telegraf_conf_server_config() ==
               *_conf->get_telegraf_conf_server_config())) {
    _create_telegraf_conf_server(new_conf->get_telegraf_conf_server_config());
  }

  if (!_conf || *_conf != *new_conf) {
    fmt::formatter<::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>::max_length_log =
        new_conf->get_max_length_grpc_log();
    fmt::formatter<::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>::json_grpc_format =
        new_conf->get_json_grpc_log();
    data_point_fifo::update_fifo_limit(new_conf->get_second_fifo_expiry(),
                                       new_conf->get_max_fifo_size());

    _conf = std::move(new_conf);

    if (!_agent_reverse_client) {
      _agent_reverse_client =
          std::make_unique<centreon_agent::agent_reverse_client>(
              _io_context,
              [me = shared_from_this()](const metric_request_ptr& request) {
                me->_on_metric(request);
              },
              _logger);
    }
    _agent_reverse_client->update(_conf->get_centreon_agent_config());
  }
  // push new configuration to connected agents
  centreon_agent::agent_impl<::grpc::ServerBidiReactor<agent::MessageFromAgent,
                                                       agent::MessageToAgent>>::
      all_agent_calc_and_send_config_if_needed(
          _conf->get_centreon_agent_config());

  centreon_agent::agent_impl<::grpc::ClientBidiReactor<
      agent::MessageToAgent, agent::MessageFromAgent>>::
      all_agent_calc_and_send_config_if_needed(
          _conf->get_centreon_agent_config());
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
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger) {
  if (!_instance) {
    _instance =
        std::make_shared<open_telemetry>(config_path, io_context, logger);
    instance()->_reload();
    instance()->_start_second_timer();
  }
  return instance();
}

/**
 * @brief create grpc server which accept otel collector connections
 *
 * @param server_conf json server config
 */
void open_telemetry::_create_otl_server(
    const grpc_config::pointer& server_conf,
    const centreon_agent::agent_config::pointer& agent_conf) {
  try {
    std::shared_ptr<otl_server> to_shutdown = std::move(_otl_server);
    if (to_shutdown) {
      to_shutdown->shutdown(std::chrono::seconds(10));
    }
    _otl_server = otl_server::load(
        _io_context, server_conf, agent_conf,
        [me = shared_from_this()](const metric_request_ptr& request) {
          me->_on_metric(request);
        },
        _logger);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to create opentelemetry grpc server: {}",
                        e.what());
    throw;
  }
}

/**
 * @brief create an http(s) server to give configuration to telegraf
 *
 * @param telegraf_conf
 */
void open_telemetry::_create_telegraf_conf_server(
    const telegraf::conf_server_config::pointer& telegraf_conf) {
  try {
    http::server::pointer to_shutdown = _telegraf_conf_server;
    if (to_shutdown) {
      to_shutdown->shutdown();
    }
    http::http_config::pointer conf = std::make_shared<http::http_config>(
        telegraf_conf->get_listen_endpoint(), "", telegraf_conf->is_crypted(),
        std::chrono::seconds(10), std::chrono::seconds(30),
        std::chrono::seconds(300), 30, std::chrono::seconds(10), 0,
        std::chrono::hours(1), 1, asio::ssl::context::tlsv12,
        telegraf_conf->get_certificate_path(), telegraf_conf->get_key_path());

    if (telegraf_conf->is_crypted()) {
      _telegraf_conf_server = http::server::load(
          _io_context, _logger, conf,
          [conf, io_ctx = _io_context, telegraf_conf, logger = _logger]() {
            return std::make_shared<
                telegraf::conf_session<http::https_connection>>(
                io_ctx, logger, conf,
                http::https_connection::load_server_certificate, telegraf_conf);
          });
    } else {
      _telegraf_conf_server = http::server::load(
          _io_context, _logger, conf,
          [conf, io_ctx = _io_context, telegraf_conf, logger = _logger]() {
            return std::make_shared<
                telegraf::conf_session<http::http_connection>>(
                io_ctx, logger, conf, nullptr, telegraf_conf);
          });
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        _logger, "fail to create telegraf http(s) conf server: {}", e.what());
    _telegraf_conf_server.reset();
  }
}

/**
 * @brief static method used to make singleton reload is configuration
 *
 */
void open_telemetry::reload(const std::shared_ptr<spdlog::logger>& logger) {
  if (_instance) {
    try {
      SPDLOG_LOGGER_INFO(logger, "reload of open telemetry module");
      instance()->_reload();
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(
          logger,
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
void open_telemetry::unload(const std::shared_ptr<spdlog::logger>& logger) {
  if (_instance) {
    SPDLOG_LOGGER_INFO(logger, "unload of open telemetry module");
    instance()->_shutdown();
    _instance.reset();
  }
}

/**
 * @brief shutdown grpc server and stop second timer
 *
 */
void open_telemetry::_shutdown() {
  std::shared_ptr<otl_server> to_shutdown = std::move(_otl_server);
  if (to_shutdown) {
    to_shutdown->shutdown(std::chrono::seconds(10));
  }
  std::lock_guard l(_protect);
  _second_timer.cancel();
}

/**
 * @brief create an host serv extractor from connector command line
 *
 * @param cmdline which begins with name of extractor, following parameters
 * are used by extractor
 * @param host_serv_list list that will be shared bu host_serv_extractor and
 * otel_connector
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
        SPDLOG_LOGGER_DEBUG(_logger, "remove extractor:{}", *to_test->second);
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
    SPDLOG_LOGGER_DEBUG(_logger, "create extractor:{}", *new_extractor);
    return new_extractor;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to create extractor \"{}\" : {}",
                        cmdline, e.what());
    throw;
  }
}

/**
 * @brief converter is created for each check, so in order to not parse otel
 * connector command line on each check , we create a
 * check_result_builder_config object that is used to create converter it search
 * the flag extractor
 *
 * @param cmd_line
 * @return
 * std::shared_ptr<com::centreon::engine::commands::otel::check_result_builder_config>
 */
std::shared_ptr<
    com::centreon::engine::commands::otel::check_result_builder_config>
open_telemetry::create_check_result_builder_config(
    const std::string& cmd_line) {
  return otl_check_result_builder::create_check_result_builder_config(cmd_line);
}

/**
 * @brief simulate a check by reading in metrics fifos
 * It creates an otel_converter, the first word of processed_cmd is the name
 * of converter such as nagios_telegraf. Following parameters are used by
 * converter
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
bool open_telemetry::check(
    const std::string& processed_cmd,
    const std::shared_ptr<commands::otel::check_result_builder_config>&
        conv_config,
    uint64_t command_id,
    nagios_macros& macros,
    uint32_t timeout,
    commands::result& res,
    commands::otel::result_callback&& handler) {
  std::shared_ptr<otl_check_result_builder> to_use;
  try {
    to_use = otl_check_result_builder::create(
        processed_cmd,
        std::static_pointer_cast<check_result_builder_config>(conv_config),
        command_id, *macros.host_ptr, macros.service_ptr,
        std::chrono::system_clock::now() + std::chrono::seconds(timeout),
        std::move(handler), _logger);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to create converter for {} : {}",
                        processed_cmd, e.what());
    throw;
  };

  bool res_available = to_use->sync_build_result_from_metrics(_fifo, res);

  if (res_available) {
    SPDLOG_LOGGER_TRACE(_logger, "data available for command {} converter:{}",
                        command_id, *to_use);
    return true;
  }

  SPDLOG_LOGGER_TRACE(
      _logger, "data unavailable for command {} timeout: {} converter:{}",
      command_id, timeout, *to_use);

  // metrics not yet available = wait for data or until timeout
  std::lock_guard l(_protect);
  _waiting.insert(to_use);

  return false;
}

/**
 * @brief called on metric reception
 * we first fill otl_data_point fifos and then if a converter of a service is
 * waiting for otl_data_point, we call him. If it achieve to generate a check
 * result, the handler of otl_check_result_builder is called
 * unknown metrics are passed to _forward_to_broker
 *
 * @param metrics collector request
 */
void open_telemetry::_on_metric(const metric_request_ptr& metrics) {
  std::vector<otl_data_point> unknown;
  {
    std::lock_guard l(_protect);
    if (_extractors.empty()) {  // no extractor configured => all unknown
      otl_data_point::extract_data_points(
          metrics, [&unknown](const otl_data_point& data_pt) {
            unknown.push_back(data_pt);
          });
    } else {
      waiting_converter::nth_index<0>::type& host_serv_index =
          _waiting.get<0>();
      std::vector<std::shared_ptr<otl_check_result_builder>> to_notify;
      auto last_success = _extractors.begin();
      otl_data_point::extract_data_points(
          metrics, [this, &unknown, &last_success, &host_serv_index,
                    &to_notify](const otl_data_point& data_pt) {
            bool data_point_known = false;
            // we try all extractors and we begin with the last which has
            // achieved to extract host
            for (unsigned tries = 0; tries < _extractors.size(); ++tries) {
              host_serv_metric hostservmetric =
                  last_success->second->extract_host_serv_metric(data_pt);

              if (!hostservmetric.host.empty()) {  // match
                _fifo.add_data_point(hostservmetric.host,
                                     hostservmetric.service,
                                     hostservmetric.metric, data_pt);

                // converters waiting this metric?
                auto waiting = host_serv_index.equal_range(
                    host_serv{hostservmetric.host, hostservmetric.service});
                while (waiting.first != waiting.second) {
                  to_notify.push_back(*waiting.first);
                  waiting.first = host_serv_index.erase(waiting.first);
                }
                data_point_known = true;
                break;
              }
              // no match => we try next extractor
              ++last_success;
              if (last_success == _extractors.end()) {
                last_success = _extractors.begin();
              }
            }
            if (!data_point_known) {
              unknown.push_back(
                  data_pt);  // unknown metric => forward to broker
            }
          });
      SPDLOG_LOGGER_TRACE(_logger, "fifos:{}", _fifo);
      // we wait that all request datas have been computed to give us more
      // chance of converter success
      for (auto to_callback : to_notify) {
        if (!to_callback->async_build_result_from_metrics(
                _fifo)) {  // not enough data => repush in _waiting
          _waiting.insert(to_callback);
        }
      }
      SPDLOG_LOGGER_TRACE(_logger, "fifos:{}", _fifo);
    }
  }
  if (!unknown.empty()) {
    SPDLOG_LOGGER_TRACE(_logger, "{} unknown data_points", unknown.size());
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
  std::vector<std::shared_ptr<otl_check_result_builder>> to_notify;
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
  for (std::shared_ptr<otl_check_result_builder> to_not : to_notify) {
    SPDLOG_LOGGER_DEBUG(_logger, "time out: {}", *to_not);
    to_not->async_time_out();
  }

  _start_second_timer();
}

/**
 * @brief unknown metrics are directly forwarded to broker
 *
 * @param unknown
 */
void open_telemetry::_forward_to_broker(
    const std::vector<otl_data_point>& unknown) {}
