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

#include "config.hh"

using namespace com::centreon::agent;

std::shared_ptr<asio::io_context> g_io_context =
    std::make_shared<asio::io_context>();

std::shared_ptr<spdlog::logger> g_logger;

static asio::signal_set _signals(*g_io_context, SIGTERM, SIGUSR1, SIGUSR2);

static void signal_handler(const boost::system::error_code& error,
                           int signal_number) {
  if (!error) {
    switch (signal_number) {
      case SIGTERM:
        SPDLOG_LOGGER_INFO(g_logger, "SIGTERM received");
        g_io_context->stop();
        break;
      case SIGUSR2:
        SPDLOG_LOGGER_INFO(g_logger, "SIGUSR2 received");
        if (g_logger->level()) {
          g_logger->set_level(
              static_cast<spdlog::level::level_enum>(g_logger->level() - 1));
        }
        break;
      case SIGUSR1:
        SPDLOG_LOGGER_INFO(g_logger, "SIGUSR1 received");
        if (g_logger->level() < spdlog::level::off) {
          g_logger->set_level(
              static_cast<spdlog::level::level_enum>(g_logger->level() + 1));
        }
        break;
    }
    _signals.async_wait(signal_handler);
  }
}

static std::string read_crypto_file(const std::string& file_path) {
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
    SPDLOG_LOGGER_ERROR(g_logger, "{} fail to read {}: {}", file_path,
                        e.what());
  }
  return "";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    SPDLOG_ERROR("usage: {} <path to json config file", argv[0]);
    return 1;
  }

  std::unique_ptr<config> conf;
  try {
    conf = std::make_unique<config>(argv[1]);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("fail to parse config file {}: {}", argv[1], e.what());
    return 1;
  }

  SPDLOG_INFO(
      "centreon-agent start, you can decrease log verbosity by kill -USR1 "
      "{} or increase by kill -USR2 {}",
      getpid(), getpid());

  const std::string logger_name = "centreon-agent";

  if (conf->get_log_type() == config::file) {
    try {
      if (!conf->get_log_file().empty()) {
        if (conf->get_log_max_file_size() > 0 &&
            conf->get_log_max_files() > 0) {
          g_logger = spdlog::rotating_logger_mt(
              logger_name, conf->get_log_file(),
              conf->get_log_max_file_size() * 0x100000,
              conf->get_log_max_files());
        } else {
          SPDLOG_INFO(
              "no log-max-file-size option or no log-max-files option provided "
              "=> logs will not be rotated by centagent");
          g_logger = spdlog::basic_logger_mt(logger_name, conf->get_log_file());
        }
      } else {
        SPDLOG_ERROR(
            "log-type=file needs the option log-file => log to stdout");
        g_logger = spdlog::stdout_color_mt(logger_name);
      }
    } catch (const std::exception& e) {
      SPDLOG_CRITICAL("Can't log to {}: {}", conf->get_log_file(), e.what());
      return 2;
    }
  } else {
    g_logger = spdlog::stdout_color_mt(logger_name);
  }

  g_logger->set_level(conf->get_log_level());

  SPDLOG_LOGGER_INFO(g_logger,
                     "centreon-agent start, you can decrease log "
                     "verbosity by kill -USR1 {} or increase by kill -USR2 {}",
                     getpid(), getpid());
  std::shared_ptr<com::centreon::common::grpc::grpc_config> grpc_conf;

  try {
    // ignored but mandatory because of forks
    _signals.add(SIGPIPE);

    _signals.async_wait(signal_handler);

    grpc_conf = std::make_shared<com::centreon::common::grpc::grpc_config>(
        conf->get_endpoint(), conf->use_encryption(),
        read_crypto_file(conf->get_certificate_file()),
        read_crypto_file(conf->get_private_key_file()),
        read_crypto_file(conf->get_ca_certificate_file()), conf->get_ca_name(),
        true, 30);

  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("fail to parse input params: {}", e.what());
    return -1;
  }

  try {
    g_io_context->run();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_CRITICAL(g_logger, "unhandled exception: {}", e.what());
    return -1;
  }

  SPDLOG_LOGGER_INFO(g_logger, "centreon-agent end");

  return 0;
}