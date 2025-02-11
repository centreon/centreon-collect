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

#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/command_manager.hh"

#include "open_telemetry.hh"

#include "com/centreon/engine/commands/otel_connector.hh"
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
    : _config_file_path(config_file_path),
      _logger(logger),
      _io_context(io_context),
      _agent_stats(centreon_agent::agent_stat::load(io_context)) {
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

    _conf = std::move(new_conf);

    if (!_agent_reverse_client) {
      _agent_reverse_client =
          std::make_unique<centreon_agent::agent_reverse_client>(
              _io_context,
              [me = shared_from_this()](const metric_request_ptr& request) {
                me->on_metric(request);
              },
              _logger, _agent_stats);
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
          me->on_metric(request);
        },
        _logger, _agent_stats);
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
        telegraf_conf->get_public_cert(), telegraf_conf->get_private_key());

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
  _agent_stats->stop_send_timer();
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

std::shared_ptr<
    com::centreon::engine::commands::otel::otl_check_result_builder_base>
open_telemetry::create_check_result_builder(const std::string& cmdline) {
  return otl_check_result_builder::create(cmdline, _logger);
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
void open_telemetry::on_metric(const metric_request_ptr& metrics) {
  std::vector<otl_data_point> unknown;
  {
    std::lock_guard l(_protect);
    if (_extractors.empty()) {  // no extractor configured => all unknown
      otl_data_point::extract_data_points(
          metrics, [&unknown](const otl_data_point& data_pt) {
            unknown.push_back(data_pt);
          });
    } else {
      std::shared_ptr<absl::flat_hash_map<
          std::pair<std::string_view, std::string_view>, metric_to_datapoints>>
          known_data_pt = std::make_shared<
              absl::flat_hash_map<std::pair<std::string_view, std::string_view>,
                                  metric_to_datapoints>>();
      auto last_success = _extractors.begin();
      otl_data_point::extract_data_points(
          metrics, [this, &unknown, &last_success,
                    known_data_pt](const otl_data_point& data_pt) {
            bool data_point_known = false;
            // we try all extractors and we begin with the last which has
            // achieved to extract host
            for (unsigned tries = 0; tries < _extractors.size(); ++tries) {
              host_serv_metric hostservmetric =
                  last_success->second->extract_host_serv_metric(data_pt);

              if (!hostservmetric.host.empty()) {  // match
                (*known_data_pt)[std::make_pair(hostservmetric.host,
                                                hostservmetric.service)]
                                [data_pt.get_metric().name()]
                                    .insert(data_pt);
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

      // we post all check results in the main thread
      auto fn = std::packaged_task<int(void)>(
          [known_data_pt, metrics, logger = _logger]() {
            // for each host or service, we generate a result
            for (const auto& host_serv_data : *known_data_pt) {
              // get connector for this service
              std::shared_ptr<commands::otel_connector> conn =
                  commands::otel_connector::get_otel_connector_from_host_serv(
                      host_serv_data.first.first, host_serv_data.first.second);
              if (!conn) {
                SPDLOG_LOGGER_ERROR(
                    logger, "no opentelemetry connector found for {}:{}",
                    host_serv_data.first.first, host_serv_data.first.second);
              } else {
                conn->process_data_pts(host_serv_data.first.first,
                                       host_serv_data.first.second,
                                       host_serv_data.second);
              }
            }
            return OK;
          });
      command_manager::instance().enqueue(std::move(fn));
    }
  }
  if (!unknown.empty()) {
    SPDLOG_LOGGER_TRACE(_logger, "{} unknown data_points", unknown.size());
    _forward_to_broker(unknown);
  }
}

/**
 * @brief unknown metrics are directly forwarded to broker
 *
 * @param unknown
 */
void open_telemetry::_forward_to_broker(
    const std::vector<otl_data_point>& unknown) {}
