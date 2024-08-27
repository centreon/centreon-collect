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

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/win_eventlog_sink.h>

#include "config.hh"
#include "streaming_client.hh"
#include "streaming_server.hh"

using namespace com::centreon::agent;

std::shared_ptr<asio::io_context> g_io_context =
    std::make_shared<asio::io_context>();

std::shared_ptr<spdlog::logger> g_logger;
static std::shared_ptr<streaming_client> _streaming_client;

static std::shared_ptr<streaming_server> _streaming_server;

static asio::signal_set _signals(*g_io_context, SIGTERM, SIGINT);

static void signal_handler(const boost::system::error_code& error,
                           int signal_number) {
  if (!error) {
    switch (signal_number) {
      case SIGINT:
      case SIGTERM:
        SPDLOG_LOGGER_INFO(g_logger, "SIGTERM or SIGINT received");
        if (_streaming_client) {
          _streaming_client->shutdown();
        }
        if (_streaming_server) {
          _streaming_server->shutdown();
        }
        g_io_context->post([]() { g_io_context->stop(); });
        return;
    }
    _signals.async_wait(signal_handler);
  }
}

static std::string read_file(const std::string& file_path) {
  if (file_path.empty()) {
    return {};
  }
  try {
    std::ifstream file(file_path);
    if (file.is_open()) {
      std::stringstream ss;
      ss << file.rdbuf();
      file.close();
      return ss.str();
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(g_logger, "fail to read {}: {}", file_path, e.what());
  }
  return "";
}

int main(int argc, char* argv[]) {
  const char* registry_path = "SOFTWARE\\Centreon\\CentreonMonitoringAgent";

  std::unique_ptr<config> conf;
  try {
    conf = std::make_unique<config>(registry_path);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("fail to read conf from registry {}: {}", registry_path,
                 e.what());
    return 1;
  }

  SPDLOG_INFO("centreon-monitoring-agent start");

  const std::string logger_name = "centreon-monitoring-agent";

  auto create_event_logger = []() {
    auto sink = std::make_shared<spdlog::sinks::win_eventlog_sink_mt>(
        "CentreonMonitoringAgent");
    g_logger = std::make_shared<spdlog::logger>("", sink);
  };

  try {
    if (conf->get_log_type() == config::to_file) {
      if (!conf->get_log_file().empty()) {
        if (conf->get_log_files_max_size() > 0 &&
            conf->get_log_files_max_number() > 0) {
          g_logger = spdlog::rotating_logger_mt(
              logger_name, conf->get_log_file(),
              conf->get_log_files_max_size() * 0x100000,
              conf->get_log_files_max_number());
        } else {
          SPDLOG_INFO(
              "no log-max-file-size option or no log-max-files option provided "
              "=> logs will not be rotated by centagent");
          g_logger = spdlog::basic_logger_mt(logger_name, conf->get_log_file());
        }
      } else {
        SPDLOG_ERROR(
            "log-type=file needs the option log-file => log to event log");
        create_event_logger();
      }
    } else if (conf->get_log_type() == config::to_stdout) {
      g_logger = spdlog::stdout_color_mt(logger_name);
    } else {
      create_event_logger();
    }
  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("Can't log to {}: {}", conf->get_log_file(), e.what());
    return 2;
  }

  g_logger->set_level(conf->get_log_level());

  g_logger->flush_on(spdlog::level::warn);

  spdlog::flush_every(std::chrono::seconds(1));

  SPDLOG_LOGGER_INFO(g_logger, "centreon-monitoring-agent start");
  std::shared_ptr<com::centreon::common::grpc::grpc_config> grpc_conf;

  try {
    _signals.async_wait(signal_handler);

    grpc_conf = std::make_shared<com::centreon::common::grpc::grpc_config>(
        conf->get_endpoint(), conf->use_encryption(),
        read_file(conf->get_public_cert_file()),
        read_file(conf->get_private_key_file()),
        read_file(conf->get_ca_certificate_file()), conf->get_ca_name(), true,
        30);

  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("fail to parse input params: {}", e.what());
    return -1;
  }

  if (conf->use_reverse_connection()) {
    _streaming_server = streaming_server::load(g_io_context, g_logger,
                                               grpc_conf, conf->get_host());
  } else {
    _streaming_client = streaming_client::load(g_io_context, g_logger,
                                               grpc_conf, conf->get_host());
  }

  try {
    g_io_context->run();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_CRITICAL(g_logger, "unhandled exception: {}", e.what());
    return -1;
  }

  SPDLOG_LOGGER_INFO(g_logger, "centreon-monitoring-agent end");

  return 0;
}
