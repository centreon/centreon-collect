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

#include "scheduler.hh"

namespace po = boost::program_options;

using namespace com::centreon::agent;

std::shared_ptr<asio::io_context> g_io_context =
    std::make_shared<asio::io_context>();

std::shared_ptr<spdlog::logger> g_logger;

static void signal_handler(const boost::system::error_code& error,
                           int signal_number) {
  if (!error) {
    switch (signal_number) {
      case SIGTERM:
        SPDLOG_LOGGER_INFO(g_logger, "SIGTERM received");
        g_io_context->stop();
        break;
      case SIGUSR1:
        if (g_logger->level()) {
          g_logger->set_level(
              static_cast<spdlog::level::level_enum>(g_logger->level() - 1));
        }
        break;
      case SIGUSR2:
        if (g_logger->level() < spdlog::level::off) {
          g_logger->set_level(
              static_cast<spdlog::level::level_enum>(g_logger->level() + 1));
        }
        break;
    }
  }
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
      "key", po::value<std::string>(), "path of the certificate key file")(
      "hostname", po::value<std::vector<std::string>>(),
      "hosts supervised by this agent (if none given we use name of this "
      "host)")("grpc-streaming", po::value<bool>()->default_value(true),
               "this agent connect to engine in streaming mode")(
      "reversed-grpc-streaming", po::value<bool>()->default_value(true),
      "this agent accept connection from engine in streaming mode")(
      "logger-type", po::value<std::string>()->default_value("stdout"),
      "type of logger: stdout, file")(
      "logger-file", po::value<std::string>(),
      "log file used in case of logger_type = file")(
      "logger-max-file-size", po::value<unsigned>(),
      "max size of log file in Mo before rotate")(
      "logger-max-files", po::value<unsigned>(), "max log files");

  SPDLOG_INFO(
      "centreon-agent start, you can decrease log level by kill -USR1 "
      "{} or increase by kill -USR2 {}",
      getpid(), getpid());

  sigignore(SIGPIPE);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  const std::string logger_name = "centeon-agent";

  auto logger_file = [&]() {
    if (vm.count("logger-file")) {
      if (vm.count("logger-max-file-size") && vm.count("logger-max-files")) {
        g_logger = spdlog::rotating_logger_mt(
            logger_name, vm["logger-file"].as<std::string>(),
            vm["logger-max-file-size"].as<unsigned>(),
            vm["logger-max-files"].as<unsigned>());
      } else {
        g_logger = spdlog::basic_logger_mt(logger_name,
                                           vm["logger-file"].as<std::string>());
      }
    } else {
      g_logger = spdlog::stdout_color_mt(logger_name);
    }
  };

  std::string log_type = vm["logger-type"].as<std::string>();

  if (log_type == "file") {
    logger_file();
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
                     "level by kill -USR1 {} or increase by kill -USR2 {}",
                     getpid(), getpid());

  asio::signal_set signals(*g_io_context, SIGTERM, SIGUSR1, SIGUSR2);
  signals.async_wait(signal_handler);

  try {
    g_io_context->run();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_CRITICAL(g_logger, "unhandled exception: {}", e.what());
    return -1;
  }

  SPDLOG_LOGGER_INFO(g_logger, "centreon-agent end");

  return 0;
}