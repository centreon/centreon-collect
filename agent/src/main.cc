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

#include "com/centreon/common/grpc/grpc_config.hh"

namespace po = boost::program_options;

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

static std::string read_crypto_file(const char* field,
                                    const po::variables_map& vm) {
  if (!vm.count(field)) {
    return {};
  }
  std::string path = vm[field].as<std::string>();
  try {
    std::ifstream file(path);
    if (file.is_open()) {
      std::stringstream ss;
      ss << file.rdbuf();
      file.close();
      return ss.str();
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(g_logger, "{} fail to read {}: {}", field, path,
                        e.what());
  }
  return "";
}

int main(int argc, char* argv[]) {
  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "produce help message")(
      "log-level", po::value<std::string>()->default_value("info"),
      "log level, may be critical, error, info, debug, trace")(
      "endpoint", po::value<std::string>(),
      "connect or listen endpoint <host:port> ex: 192.168.1.1:4443")(
      "config-file", po::value<std::string>(),
      "command line options in a file with format key=value")(
      "encryption", po::value<bool>()->default_value(false),
      "true if encryption")("certificate", po::value<std::string>(),
                            "path of the certificate file")(
      "private_key", po::value<std::string>(),
      "path of the certificate key file")(
      "ca_certificate", po::value<std::string>(),
      "path of the certificate authority file")(
      "ca_name", po::value<std::string>(), "hostname of the certificate")(
      "host", po::value<std::string>(),
      "host supervised by this agent (if none given we use name of this "
      "host)")("grpc-streaming", po::value<bool>()->default_value(true),
               "this agent connect to engine in streaming mode")(
      "reversed-grpc-streaming", po::value<bool>()->default_value(false),
      "this agent accept connection from engine in streaming mode")(
      "log-type", po::value<std::string>()->default_value("stdout"),
      "type of logger: stdout, file")(
      "log-file", po::value<std::string>(),
      "log file used in case of log_type = file")(
      "log-max-file-size", po::value<unsigned>(),
      "max size of log file in Mo before rotate")(
      "log-max-files", po::value<unsigned>(), "max log files");

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << "\n";
      return 1;
    }

    if (vm.count("config-file")) {
      po::store(po::parse_config_file(
                    vm["config-file"].as<std::string>().c_str(), desc),
                vm);
    }

  } catch (const std::exception& e) {
    SPDLOG_ERROR("fail to parse arguments {}", e.what());
    return 2;
  }

  SPDLOG_INFO(
      "centreon-agent start, you can decrease log verbosity by kill -USR1 "
      "{} or increase by kill -USR2 {}",
      getpid(), getpid());

  const std::string logger_name = "centreon-agent";

  std::string log_type = vm["log-type"].as<std::string>();

  if (log_type == "file") {
    try {
      if (vm.count("log-file")) {
        if (vm.count("log-max-file-size") && vm.count("log-max-files")) {
          g_logger = spdlog::rotating_logger_mt(
              logger_name, vm["log-file"].as<std::string>(),
              vm["log-max-file-size"].as<unsigned>(),
              vm["log-max-files"].as<unsigned>());
        } else {
          SPDLOG_INFO(
              "no log-max-file-size option or no log-max-files option provided "
              "=> logs will not be rotated by centagent");
          g_logger = spdlog::basic_logger_mt(logger_name,
                                             vm["log-file"].as<std::string>());
        }
      } else {
        SPDLOG_ERROR(
            "log-type=file needs the option log-file => log to stdout");
        g_logger = spdlog::stdout_color_mt(logger_name);
      }
    } catch (const std::exception& e) {
      SPDLOG_CRITICAL("Can't log to {}: {}", vm["log-file"].as<std::string>(),
                      e.what());
      return 2;
    }
  } else {
    g_logger = spdlog::stdout_color_mt(logger_name);
  }

  spdlog::set_level(spdlog::level::info);
  if (vm.count("log-level")) {
    std::string log_level = vm["log-level"].as<std::string>();
    if (log_level == "critical") {
      g_logger->set_level(spdlog::level::critical);
    } else if (log_level == "error") {
      g_logger->set_level(spdlog::level::err);
    } else if (log_level == "debug") {
      g_logger->set_level(spdlog::level::debug);
    } else if (log_level == "trace") {
      g_logger->set_level(spdlog::level::trace);
    }
  }

  SPDLOG_LOGGER_INFO(g_logger,
                     "centreon-agent start, you can decrease log "
                     "verbosity by kill -USR1 {} or increase by kill -USR2 {}",
                     getpid(), getpid());
  std::shared_ptr<com::centreon::common::grpc::grpc_config> conf;
  std::string supervised_host;

  try {
    // ignored but mandatory because of forks
    _signals.add(SIGPIPE);

    _signals.async_wait(signal_handler);
    if (!vm.count("endpoint")) {
      SPDLOG_CRITICAL(
          "endpoint param is mandatory (represents where to connect or where "
          "to listen example: 127.0.0.1:4317)");
      return -1;
    }
    std::string host_port = vm["endpoint"].as<std::string>();
    std::string ca_name;
    if (vm.count("ca_name")) {
      ca_name = vm["ca_name"].as<std::string>();
    }

    if (vm.count("host")) {
      supervised_host = vm["host"].as<std::string>();
    }
    if (supervised_host.empty()) {
      supervised_host = boost::asio::ip::host_name();
    }

    conf = std::make_shared<com::centreon::common::grpc::grpc_config>(
        host_port, vm["encryption"].as<bool>(),
        read_crypto_file("certificate", vm),
        read_crypto_file("private_key", vm),
        read_crypto_file("ca_certificate", vm), ca_name, true, 30);

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