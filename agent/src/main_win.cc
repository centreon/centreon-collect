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
#include <windows.h>

#include "log.hh"

#include "agent_info.hh"
#include "check_cpu.hh"
#include "check_event_log.hh"
#include "check_health.hh"
#include "check_memory.hh"
#include "check_process.hh"
#include "check_service.hh"
#include "check_uptime.hh"
#include "drive_size.hh"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/win_eventlog_sink.h>

#include "config.hh"
#include "drive_size.hh"
#include "ntdll.hh"
#include "streaming_client.hh"
#include "streaming_server.hh"

namespace com::centreon::agent::check_drive_size_detail {

std::list<fs_stat> os_fs_stats(filter& filter,
                               const std::shared_ptr<spdlog::logger>& logger);

}

using namespace com::centreon::agent;

#define SERVICE_NAME "CentreonMonitoringAgent"

std::shared_ptr<asio::io_context> g_io_context =
    std::make_shared<asio::io_context>();

static std::shared_ptr<streaming_client> _streaming_client;

static std::shared_ptr<streaming_server> _streaming_server;

static asio::signal_set _signals(*g_io_context, SIGTERM, SIGINT);

/**
 * @brief shutdown network connections and stop asio service
 *
 */
static void stop_process() {
  if (_streaming_client) {
    _streaming_client->shutdown();
  }
  if (_streaming_server) {
    _streaming_server->shutdown();
  }
  asio::post(*g_io_context, []() { g_io_context->stop(); });
}

/**
 * @brief called on Ctrl+C
 *
 * @param error
 * @param signal_number
 */
static void signal_handler(const boost::system::error_code& error,
                           int signal_number) {
  if (!error) {
    switch (signal_number) {
      case SIGINT:
      case SIGTERM:
        SPDLOG_LOGGER_INFO(g_logger, "SIGTERM or SIGINT received");
        stop_process();
        return;
    }
    _signals.async_wait(signal_handler);
  }
}

/**
 * @brief load file in a std::string
 *
 * @param file_path file path
 * @return std::string file content
 */
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

void show_help() {
  std::cout << "usage: centagent.exe [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --standalone: run the agent in standalone mode not from "
               "service manager (mandatory for start it from command line)"
            << std::endl;
  std::cout << "  --help: show this help" << std::endl;
  std::cout << std::endl << "native checks options:" << std::endl;
  check_cpu::help(std::cout);
  check_memory::help(std::cout);
  check_uptime::help(std::cout);
  check_drive_size::help(std::cout);
  check_service::help(std::cout);
  check_health::help(std::cout);
  check_event_log::help(std::cout);
  check_process::help(std::cout);
}

/**
 * @brief this program can be started in two ways
 * from command line: main function
 * from service manager: SvcMain function
 *
 * @return int exit status returned to command line (0 success)
 */
int _main(bool service_start) {
  std::string registry_path = "SOFTWARE\\Centreon\\" SERVICE_NAME;

  try {
    config::load(registry_path);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("fail to read conf from registry {}: {}", registry_path,
                 e.what());
    return 1;
  }

  // init os specific drive_size getter
  check_drive_size_detail::drive_size_thread::os_fs_stats =
      check_drive_size_detail::os_fs_stats;

  if (service_start)
    SPDLOG_INFO("centreon-monitoring-agent service start");
  else
    SPDLOG_INFO("centreon-monitoring-agent start");

  const std::string logger_name = "centreon-monitoring-agent";

  auto create_event_logger = []() {
    auto sink =
        std::make_shared<spdlog::sinks::win_eventlog_sink_mt>(SERVICE_NAME);
    g_logger = std::make_shared<spdlog::logger>("", sink);
  };

  const config& conf = config::instance();

  try {
    if (conf.get_log_type() == config::to_file) {
      if (!conf.get_log_file().empty()) {
        if (conf.get_log_max_file_size() > 0 && conf.get_log_max_files() > 0) {
          g_logger = spdlog::rotating_logger_mt(
              logger_name, conf.get_log_file(),
              conf.get_log_max_file_size() * 0x100000,
              conf.get_log_max_files());
        } else {
          SPDLOG_INFO(
              "no log-max-file-size option or no log-max-files option provided "
              "=> logs will not be rotated by centagent");
          g_logger = spdlog::basic_logger_mt(logger_name, conf.get_log_file());
        }
      } else {
        SPDLOG_ERROR(
            "log-type=file needs the option log-file => log to event log");
        create_event_logger();
      }
    } else if (conf.get_log_type() == config::to_stdout) {
      g_logger = spdlog::stdout_color_mt(logger_name);
    } else {
      create_event_logger();
    }
  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("Can't log to {}: {}", conf.get_log_file(), e.what());
    return 2;
  }

  g_logger->set_level(conf.get_log_level());

  g_logger->flush_on(spdlog::level::warn);

  spdlog::flush_every(std::chrono::seconds(1));

  set_grpc_logger();

  SPDLOG_LOGGER_INFO(g_logger, "centreon-monitoring-agent start");
  std::shared_ptr<com::centreon::common::grpc::grpc_config> grpc_conf;

  try {
    _signals.async_wait(signal_handler);

    grpc_conf = std::make_shared<com::centreon::common::grpc::grpc_config>(
        conf.get_endpoint(), conf.use_encryption(),
        read_file(conf.get_public_cert_file()),
        read_file(conf.get_private_key_file()),
        read_file(conf.get_ca_certificate_file()), conf.get_ca_name(), true, 30,
        conf.get_second_max_reconnect_backoff(), conf.get_max_message_length(),
        conf.get_token());

  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("fail to parse input params: {}", e.what());
    return -1;
  }

  try {
    load_nt_dll();
    read_os_version();

    if (conf.use_reverse_connection()) {
      _streaming_server = streaming_server::load(g_io_context, g_logger,
                                                 grpc_conf, conf.get_host());
    } else {
      _streaming_client = streaming_client::load(g_io_context, g_logger,
                                                 grpc_conf, conf.get_host());
    }

    if (!conf.use_encryption()) {
      SPDLOG_LOGGER_WARN(
          g_logger,
          "NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION");

      auto timer = std::make_shared<asio::steady_timer>(*g_io_context,
                                                        std::chrono::hours(1));
      timer->async_wait([timer](const boost::system::error_code& ec) {
        if (!ec) {
          SPDLOG_LOGGER_WARN(g_logger,
                             "NON TLS CONNECTION TIME EXPIRED // THIS IS NOT "
                             "ALLOWED IN PRODUCTION");
          SPDLOG_LOGGER_WARN(g_logger,
                             "CONNECTION KILLED, AGENT NEED TO BE RESTART");
          stop_process();
        }
      });
    }

    g_io_context->run();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_CRITICAL(g_logger, "unhandled exception: {}", e.what());
    return -1;
  }

  // kill check_drive_size thread if used
  check_drive_size::thread_kill();

  SPDLOG_LOGGER_INFO(g_logger, "centreon-monitoring-agent end");

  return 0;
}

/**************************************************************************************
                    service part
**************************************************************************************/

static SERVICE_STATUS gSvcStatus;
static SERVICE_STATUS_HANDLE gSvcStatusHandle;
static HANDLE ghSvcStopEvent = NULL;
void WINAPI SvcMain(DWORD, LPTSTR*);

/**
 * @brief main used when program is launched on command line
 * if program is sued with --standalone flag, it does not register in service
 * manager
 *
 * @param argc not used
 * @param argv not used
 * @return int program status
 */
int main(int argc, char* argv[]) {
  if (argc > 1 && !lstrcmpi(argv[1], "--standalone")) {
    return _main(false);
  }

  if (argc > 1 && !lstrcmpi(argv[1], "--help")) {
    show_help();
    return 0;
  }

  SPDLOG_INFO(
      "centagent.exe will start in service mode, if you launch it from command "
      "line, use --standalone flag");

  const SERVICE_TABLE_ENTRY DispatchTable[] = {
      {(LPSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)SvcMain}, {NULL, NULL}};

  // This call returns when the service has stopped.
  // The process should simply terminate when the call returns.
  StartServiceCtrlDispatcher(DispatchTable);

  return 0;
}

/**
 * @brief Sets the current service status and reports it to the service manager.
 *
 * @param dwCurrentState The current state (see SERVICE_STATUS)
 * @param dwWin32ExitCode The system error code
 * @param dwWaitHint Estimated time for pending operation in milliseconds
 */
void report_svc_status(DWORD dwCurrentState,
                       DWORD dwWin32ExitCode,
                       DWORD dwWaitHint) {
  static DWORD dwCheckPoint = 1;

  // Fill in the SERVICE_STATUS structure.

  gSvcStatus.dwCurrentState = dwCurrentState;
  gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
  gSvcStatus.dwWaitHint = dwWaitHint;

  if (dwCurrentState == SERVICE_START_PENDING)
    gSvcStatus.dwControlsAccepted = 0;
  else
    gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

  if ((dwCurrentState == SERVICE_RUNNING) ||
      (dwCurrentState == SERVICE_STOPPED))
    gSvcStatus.dwCheckPoint = 0;
  else
    gSvcStatus.dwCheckPoint = dwCheckPoint++;

  // Report the status of the service to the SCM.
  SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

/**
 * @brief function called by service manager
 *
 * @param dwCtrl -control code sent by service manager
 */
void WINAPI SvcCtrlHandler(DWORD dwCtrl) {
  // Handle the requested control code.

  SPDLOG_LOGGER_INFO(g_logger, "SvcCtrlHandler {}", dwCtrl);

  switch (dwCtrl) {
    case SERVICE_CONTROL_STOP:
      report_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);

      SPDLOG_LOGGER_INFO(g_logger, "SERVICE_CONTROL_STOP received");
      stop_process();

      report_svc_status(gSvcStatus.dwCurrentState, NO_ERROR, 0);

      return;

    case SERVICE_CONTROL_INTERROGATE:
      break;

    default:
      break;
  }
}

/**
 * @brief main called by service manager
 *
 */
void WINAPI SvcMain(DWORD, LPTSTR*) {
  gSvcStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, SvcCtrlHandler);

  if (!gSvcStatusHandle) {
    SPDLOG_LOGGER_CRITICAL(g_logger, "fail to RegisterServiceCtrlHandler");
    return;
  }

  // These SERVICE_STATUS members remain as set here

  gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  gSvcStatus.dwServiceSpecificExitCode = 0;

  // Report initial status to the SCM

  report_svc_status(SERVICE_START_PENDING, NO_ERROR, 3000);

  report_svc_status(SERVICE_RUNNING, NO_ERROR, 0);

  _main(true);

  report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
}
