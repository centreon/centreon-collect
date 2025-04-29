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

#include "check_service.hh"
#include "native_check_base.cc"
#include "windows_util.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::native_check_detail;

namespace com::centreon::agent::native_check_detail {

/***********************************************************************************************
 *     service_enumerator
 ***********************************************************************************************/

/**
 * @brief Constructor
 */
service_enumerator::service_enumerator() {
  _sc_manager_handler = OpenSCManager(nullptr, nullptr, GENERIC_READ);
  if (!_sc_manager_handler) {
    throw exceptions::msg_fmt("OpenSCManager failed");
  }
}

/**
 * @brief Destructor
 */
service_enumerator::~service_enumerator() {
  CloseServiceHandle(_sc_manager_handler);
}

/**
 * @brief Enumerate services (just call a version of _enumerate_services)
 */
void service_enumerator::enumerate_services(
    service_filter& filter,
    service_enumerator::listener&& callback,
    const std::shared_ptr<spdlog::logger>& logger) {
  if (filter.use_start_auto_filter()) {
    _enumerate_services<true>(filter, std::move(callback), logger);
  } else {
    _enumerate_services<false>(filter, std::move(callback), logger);
  }
}

/**
 * @brief Abstract layer used to enumerate services (overloaded in tests to do a
 * mock)
 */
bool service_enumerator::_enumerate_services(serv_array& services,
                                             DWORD* services_returned) {
  DWORD buff_size = sizeof(services);
  return EnumServicesStatusA(_sc_manager_handler, SERVICE_TYPE_ALL,
                             SERVICE_STATE_ALL, services, sizeof(services),
                             &buff_size, services_returned, &_resume_handle);
}

/**
 * @brief Query the service configuration  (overloaded in tests to do a mock)
 */
bool service_enumerator::_query_service_config(
    LPCSTR service_name,
    std::unique_ptr<unsigned char[]>& buffer,
    size_t* buffer_size,
    const std::shared_ptr<spdlog::logger>& logger) {
  SC_HANDLE serv_handle =
      OpenService(_sc_manager_handler, service_name, GENERIC_READ);
  if (!serv_handle) {
    SPDLOG_LOGGER_ERROR(logger, " fail to open service {}: {}", service_name,
                        get_last_error_as_string());
    return false;
  }
  DWORD bytes_needed = 0;
  if (!QueryServiceConfigA(
          serv_handle, reinterpret_cast<QUERY_SERVICE_CONFIGA*>(buffer.get()),
          *buffer_size, &bytes_needed)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      buffer = std::make_unique<unsigned char[]>(bytes_needed);
      *buffer_size = bytes_needed;

      if (!QueryServiceConfigA(
              serv_handle,
              reinterpret_cast<QUERY_SERVICE_CONFIGA*>(buffer.get()),
              *buffer_size, &bytes_needed)) {
        SPDLOG_LOGGER_ERROR(logger, " fail to query service config {}: {}",
                            service_name, GetLastError());
      }
    } else {
      SPDLOG_LOGGER_ERROR(logger, " fail to query service config {}: {}",
                          service_name, GetLastError());
    }
    CloseServiceHandle(serv_handle);
    return false;
  }
  CloseServiceHandle(serv_handle);
  return true;
}

/**
 * @brief Enumerate services
 * @tparam start_auto if true, start_auto config parameter will be checked
 * @param filter service filter
 * @param callback callback to call on each service
 * @param logger logger
 */
template <bool start_auto>
void service_enumerator::_enumerate_services(
    service_filter& filter,
    service_enumerator::listener&& callback,
    const std::shared_ptr<spdlog::logger>& logger) {
  ENUM_SERVICE_STATUSA services[512];

  _resume_handle = 0;

  DWORD bytes_needed = 0;
  DWORD services_count = 0;

  while (true) {
    BOOL success = _enumerate_services(services, &services_count);
    if (success || GetLastError() == ERROR_MORE_DATA) {
      LPENUM_SERVICE_STATUSA services_end = services + services_count;

      if constexpr (start_auto) {
        std::unique_ptr<unsigned char[]> query_serv_conf_buff =
            std::make_unique<unsigned char[]>(sizeof(QUERY_SERVICE_CONFIGA));
        size_t query_serv_conf_buff_size = sizeof(QUERY_SERVICE_CONFIGA);
        for (LPENUM_SERVICE_STATUS serv = services; serv < services_end;
             ++serv) {
          if (!_query_service_config(serv->lpServiceName, query_serv_conf_buff,
                                     &query_serv_conf_buff_size, logger)) {
            continue;
          }

          const QUERY_SERVICE_CONFIGA* serv_conf =
              reinterpret_cast<const QUERY_SERVICE_CONFIGA*>(
                  query_serv_conf_buff.get());

          bool this_serv_auto_start =
              (serv_conf->dwStartType &
               (SERVICE_AUTO_START | SERVICE_BOOT_START |
                SERVICE_SYSTEM_START)) != 0;
          if (!filter.is_allowed(this_serv_auto_start, serv->lpServiceName,
                                 serv->lpDisplayName)) {
            continue;
          }
          callback(*serv);
        }
      } else {
        for (LPENUM_SERVICE_STATUS serv = services; serv < services_end;
             ++serv) {
          if (!filter.is_allowed(false, serv->lpServiceName,
                                 serv->lpDisplayName)) {
            continue;
          }
          callback(*serv);
        }
      }
    }
    if (success) {
      break;
    }
  }
}

/***********************************************************************************************
 *    w_service_info
 **********************************************************************************************/

/**
 * service status printed in plugin output
 */
static constexpr std::array<std::string_view, 8> _labels = {
    "",        "stopped",    "starting", "stopping",
    "running", "continuing", "pausing",  "paused"};

/**
 * @brief Constructor
 * @param service_enumerator service enumerator
 * @param filter service filter
 * @param state_to_warning state to warning, if a service state is in this mask,
 * output status will be at less warning
 * @param state_to_critical state to critical
 * @param logger logger
 */
w_service_info::w_service_info(service_enumerator& service_enumerator,
                               service_filter& filter,
                               unsigned state_to_warning,
                               unsigned state_to_critical,
                               const std::shared_ptr<spdlog::logger>& logger)
    : _state_to_warning(state_to_warning),
      _state_to_critical(state_to_critical) {
  memset(&_metrics, 0, sizeof(_metrics));
  service_enumerator.enumerate_services(
      filter,
      [this](const ENUM_SERVICE_STATUSA& service_status) {
        on_service(service_status);
      },
      logger);
  if (_metrics[e_service_metric::total] == 0) {
    _status = e_status::critical;
  }
}

/**
 * @brief callback called by enumerator
 */
void w_service_info::on_service(const ENUM_SERVICE_STATUSA& service_status) {
  unsigned state = service_status.ServiceStatus.dwCurrentState & 7;
  unsigned state_flag = 1 << (state - 1);
  if (state & _state_to_critical) {
    _status = e_status::critical;
    if (!_output.empty()) {
      _output.push_back(' ');
    }
    _output += fmt::format("CRITICAL: {} is {}", service_status.lpServiceName,
                           _labels[state]);
  } else if (state & _state_to_warning) {
    if (_status == e_status::ok) {
      _status = e_status::warning;
    }
    if (!_output.empty()) {
      _output.push_back(' ');
    }
    _output += fmt::format("WARNING: {} is {}", service_status.lpServiceName,
                           _labels[state]);
  }
  ++_metrics[state - 1];
  ++_metrics[e_service_metric::total];
}

/**
 * plugin output
 */
void w_service_info::dump_to_output(std::string* output) const {
  uint64_t total = _metrics[e_service_metric::total];
  if (total == 0) {
    output->append("no service found");
  } else if (total == _metrics[e_service_metric::running]) {
    output->append("all services are running");
  } else {
    output->append("services: ");
    bool first = true;
    for (unsigned i = 0; i < e_service_metric::total; ++i) {
      if (_metrics[i] > 0) {
        if (first) {
          first = false;
        } else {
          output->append(", ");
        }
        output->append(fmt::format("{} {}", _metrics[i], _labels[i + 1]));
      }
    }
  }
  if (!_output.empty()) {
    output->push_back(' ');
    output->append(_output);
  }
}

/***********************************************************************************************
 *    service_filter
 **********************************************************************************************/

/**
 * @brief Constructor that initializes the service filter based on the provided
 * arguments.
 * @param args JSON value containing the plugin config.
 * @throws exceptions::msg_fmt if any of the filter parameters are invalid.
 */
service_filter::service_filter(const rapidjson::Value& args) {
  if (args.IsObject()) {
    for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
         ++member_iter) {
      std::string key = absl::AsciiStrToLower(member_iter->name.GetString());
      if (key == "start-auto") {
        const rapidjson::Value& val = member_iter->value;
        if (val.IsBool()) {
          _start_auto = val.GetBool();
        } else {
          throw exceptions::msg_fmt("start-auto must be a boolean");
        }
      } else if (key == "filter-name") {
        const rapidjson::Value& val = member_iter->value;
        if (val.IsString()) {
          std::string value = val.GetString();
          absl::AsciiStrToLower(&value);
          _name_filter = std::make_unique<re2::RE2>(value);
          if (!_name_filter->ok()) {
            throw exceptions::msg_fmt("filter-name: {} is not a valid regex",
                                      val.GetString());
          }
        } else {
          throw exceptions::msg_fmt("filter-name must be a string");
        }
      } else if (key == "exclude-name") {
        const rapidjson::Value& val = member_iter->value;
        if (val.IsString()) {
          std::string value = val.GetString();
          absl::AsciiStrToLower(&value);
          _name_filter_exclude = std::make_unique<re2::RE2>(value);
          if (!_name_filter_exclude->ok()) {
            throw exceptions::msg_fmt("exclude-name: {} is not a valid regex",
                                      val.GetString());
          }
        } else {
          throw exceptions::msg_fmt("exclude-name must be a string");
        }
      } else if (key == "filter-display") {
        const rapidjson::Value& val = member_iter->value;
        if (val.IsString()) {
          std::string value = val.GetString();
          absl::AsciiStrToLower(&value);
          _display_filter = std::make_unique<re2::RE2>(value);
          if (!_display_filter->ok()) {
            throw exceptions::msg_fmt("filter-display: {} is not a valid regex",
                                      val.GetString());
          }
        } else {
          throw exceptions::msg_fmt("filter-display must be a string");
        }
      } else if (key == "exclude-display") {
        const rapidjson::Value& val = member_iter->value;
        if (val.IsString()) {
          std::string value = val.GetString();
          absl::AsciiStrToLower(&value);
          _display_filter_exclude = std::make_unique<re2::RE2>(value);
          if (!_display_filter_exclude->ok()) {
            throw exceptions::msg_fmt(
                "exclude-display: {} is not a valid regex", val.GetString());
          }
        } else {
          throw exceptions::msg_fmt("exclude-display must be a string");
        }
      }
    }
  }
}

/**
 * @brief remove all negative chars
 * @param sz string to clean
 */
static void remove_accents(std::string* sz) {
  for (char& chr : *sz) {
    if (chr < 0) {
      chr = '_';
    }
  }
}

/**
 * @brief Check if a service is allowed by the filter.
 * @param start_auto Whether the service is set to start automatically.
 * @param service_name The name of the service.
 */
bool service_filter::is_allowed(bool start_auto,
                                const std::string_view& service_name,
                                const std::string_view& service_display) {
  std::string lower_service_name(service_name.data(), service_name.length());
  absl::AsciiStrToLower(&lower_service_name);

  std::string lower_display(service_display.data(), service_display.length());
  absl::AsciiStrToLower(&lower_display);

  // accented characters are not supported by RE2 so we remove them
  remove_accents(&lower_display);

  if (_start_auto && _start_auto.value() != start_auto) {
    return false;
  }
  if (_name_cache_excluded.find(lower_service_name) !=
      _name_cache_excluded.end()) {
    return false;
  }
  if (_display_cache_excluded.find(lower_display) !=
      _display_cache_excluded.end()) {
    return false;
  }

  auto check_display = [&]() {
    if (_display_filter_exclude &&
        RE2::FullMatch(lower_display, *_display_filter_exclude)) {
      _display_cache_excluded.emplace(lower_display);
      return false;
    }
    if (_display_filter && !RE2::FullMatch(lower_display, *_display_filter)) {
      _display_cache_excluded.emplace(lower_display);
      return false;
    }
    _display_cache_allowed.emplace(lower_display);
    return true;
  };

  auto check_name = [&]() {
    if (_name_filter_exclude &&
        RE2::FullMatch(lower_service_name, *_name_filter_exclude)) {
      _name_cache_excluded.emplace(lower_service_name);
      return false;
    }
    if (_name_filter && !RE2::FullMatch(lower_service_name, *_name_filter)) {
      _name_cache_excluded.emplace(lower_service_name);
      return false;
    }
    _name_cache_allowed.emplace(lower_service_name);
    return true;
  };

  if (_name_cache_allowed.find(lower_service_name) !=
      _name_cache_allowed.end()) {
    if (_display_cache_allowed.find(lower_display) !=
        _display_cache_allowed.end()) {
      return true;
    }
    return check_display();
  }

  if (_display_cache_allowed.find(lower_display) !=
      _display_cache_allowed.end()) {
    return check_name();
  }

  return check_name() && check_display();
}

/***********************************************************************************************
 *    w_service_info_to_status
 **********************************************************************************************/

/**
 * The goal of this class is to convert the status field of w_service_info into
 * check_status. It compares nothing
 *  */
class w_service_info_to_status
    : public measure_to_status<e_service_metric::nb_service_metric> {
 public:
  w_service_info_to_status()
      : measure_to_status<e_service_metric::nb_service_metric>(e_status::ok,
                                                               0,
                                                               0,
                                                               0,
                                                               false,
                                                               false) {}

  void compute_status(
      const snapshot<e_service_metric::nb_service_metric>& to_test,
      e_status* status) const override {
    e_status serv_status =
        static_cast<const w_service_info&>(to_test).get_status();
    if (serv_status > *status) {
      *status = serv_status;
    }
  }
};

}  // namespace com::centreon::agent::native_check_detail

/***********************************************************************************************
 *    check_service
 **********************************************************************************************/

/**
 * we can allow different service states, so we use check_service::state_mask to
 * filter or set status to critical
 */
const std::array<std::pair<std::string_view, check_service::state_mask>, 7>
    check_service::_label_state = {
        std::make_pair("stopped", check_service::state_mask::stopped),
        std::make_pair("starting", check_service::state_mask::start_pending),
        std::make_pair("stopping", check_service::state_mask::stop_pending),
        std::make_pair("running", check_service::state_mask::running),
        std::make_pair("continuing",
                       check_service::state_mask::continue_pending),
        std::make_pair("pausing", check_service::state_mask::pause_pending),
        std::make_pair("paused", check_service::state_mask::paused)};

using w_service_to_status =
    measure_to_status<e_service_metric::nb_service_metric>;

using service_to_status_constructor =
    std::function<std::unique_ptr<w_service_to_status>(double /*threshold*/)>;

static const absl::flat_hash_map<std::string_view,
                                 service_to_status_constructor>
    _label_to_service_status = {
        {"warning-total-running",
         [](double threshold) {
           return std::make_unique<w_service_to_status>(
               e_status::warning, e_service_metric::running, threshold,
               e_service_metric::nb_service_metric, false, true);
         }},
        {"critical-total-running",
         [](double threshold) {
           return std::make_unique<w_service_to_status>(
               e_status::critical, e_service_metric::running, threshold,
               e_service_metric::nb_service_metric, false, true);
         }},
        {"warning-total-paused",
         [](double threshold) {
           return std::make_unique<w_service_to_status>(
               e_status::warning, e_service_metric::paused, threshold,
               e_service_metric::nb_service_metric, false, false);
         }},
        {"critical-total-paused",
         [](double threshold) {
           return std::make_unique<w_service_to_status>(
               e_status::critical, e_service_metric::paused, threshold,
               e_service_metric::nb_service_metric, false, false);
         }},
        {"warning-total-stopped",
         [](double threshold) {
           return std::make_unique<w_service_to_status>(
               e_status::warning, e_service_metric::stopped, threshold,
               e_service_metric::nb_service_metric, false, false);
         }},
        {"critical-total-stopped",
         [](double threshold) {
           return std::make_unique<w_service_to_status>(
               e_status::critical, e_service_metric::stopped, threshold,
               e_service_metric::nb_service_metric, false, false);
         }}

};

/**
 * default service enumerator constructor
 */
service_enumerator::constructor check_service::_enumerator_constructor = []() {
  return std::make_unique<service_enumerator>();
};

/**
 * @brief constructor
 */
check_service::check_service(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration time_step,
    duration check_interval,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat)
    : native_check_base(io_context,
                        logger,
                        first_start_expected,
                        check_interval,
                        serv,
                        cmd_name,
                        cmd_line,
                        args,
                        cnf,
                        std::move(handler),
                        stat),
      _filter(args),
      _enumerator(_enumerator_constructor()) {
  _measure_to_status.emplace(
      std::make_tuple(e_service_metric::nb_service_metric,
                      e_service_metric::nb_service_metric, e_status::ok),
      std::make_unique<w_service_info_to_status>());

  if (!args.IsObject()) {
    return;
  }

  for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
       ++member_iter) {
    std::string key = absl::AsciiStrToLower(member_iter->name.GetString());

    if (key == "warning-state") {
      const rapidjson::Value& val = member_iter->value;
      if (val.IsString()) {
        re2::RE2 filter_typ_re(val.GetString());
        if (!filter_typ_re.ok()) {
          throw exceptions::msg_fmt(
              "command: {} warning-state: {} is not a valid regex", cmd_name,
              val.GetString());
        } else {
          for (const auto& [label, flag] : _label_state) {
            if (RE2::FullMatch(label, filter_typ_re)) {
              _state_to_warning |= flag;
            }
          }
        }
      } else {
        throw exceptions::msg_fmt("command: {} warning-state must be a string",
                                  cmd_name);
      }
    } else if (key == "critical-state") {
      const rapidjson::Value& val = member_iter->value;
      if (val.IsString()) {
        re2::RE2 filter_typ_re(val.GetString());
        if (!filter_typ_re.ok()) {
          throw exceptions::msg_fmt(
              "command: {} critical-state: {} is not a valid regex", cmd_name,
              val.GetString());
        } else {
          for (const auto& [label, flag] : _label_state) {
            if (RE2::FullMatch(label, filter_typ_re)) {
              _state_to_critical |= flag;
            }
          }
        }
      } else {
        throw exceptions::msg_fmt("command: {} critical-state must be a string",
                                  cmd_name);
      }
    } else {
      auto threshold = _label_to_service_status.find(key);
      if (threshold != _label_to_service_status.end()) {
        std::optional<double> val = get_double(
            cmd_name, member_iter->name.GetString(), member_iter->value, true);
        if (val) {
          std::unique_ptr<w_service_to_status> to_ins = threshold->second(*val);
          _measure_to_status.emplace(
              std::make_tuple(to_ins->get_data_index(),
                              e_service_metric::nb_service_metric,
                              to_ins->get_status()),
              std::move(to_ins));
        }
      } else if (key != "filter-name" && key != "exclude-name" &&
                 key != "filter-display" && key != "exclude-display" &&
                 key != "start-auto") {
        SPDLOG_LOGGER_ERROR(logger, "command: {}, unknown parameter: {}",
                            cmd_name, member_iter->name);
      }
    }
  }
}

/**
 * @brief create a snapshot of services state
 */
std::shared_ptr<native_check_detail::snapshot<
    native_check_detail::e_service_metric::nb_service_metric>>
check_service::measure() {
  // used to reset service list walking
  _enumerator->reset_resume_handle();
  return std::make_shared<native_check_detail::w_service_info>(
      *_enumerator, _filter, _state_to_warning, _state_to_critical, _logger);
}

static const std::vector<native_check_detail::metric_definition>
    metric_definitions = {
        {"services.stopped.count", e_service_metric::stopped,
         e_service_metric::total, false},
        {"services.starting.count", e_service_metric::start_pending,
         e_service_metric::total, false},
        {"services.stopping.count", e_service_metric::stop_pending,
         e_service_metric::total, false},
        {"services.running.count", e_service_metric::running,
         e_service_metric::total, false},
        {"services.continuing.count", e_service_metric::continue_pending,
         e_service_metric::total, false},
        {"services.pausing.count", e_service_metric::pause_pending,
         e_service_metric::total, false},
        {"services.paused.count", e_service_metric::paused,
         e_service_metric::total, false}};

const std::vector<native_check_detail::metric_definition>&
check_service::get_metric_definitions() const {
  return metric_definitions;
}

/**
 * @brief some help
 */
void check_service::help(std::ostream& help_stream) {
  help_stream << R"(
- service params:
    warning-state: regex to match service state that will trigger a warning
      states are:
        - stopped
        - starting
        - stopping
        - running
        - continuing
        - pausing
        - paused
    critical-state: regex to match service state that will trigger a critical
    warning-total-running: running service number threshold below which the service will pass in the warning state
    critical-total-running: running service number threshold below which the service will pass in the critical state
    warning-total-paused: number of services in the pause state above which the service goes into the warning state
    critical-total-paused: number of services in the pause state above which the service goes into the critical state
    warning-total-stopped: number of services in the stop state above which the service goes into the warning state
    critical-total-stopped: number of services in the stop state above which the service goes into the critical state
    start-auto: true: only services that start automatically will be counted
    filter-name: regex to filter service names
    exclude-name: regex to exclude service names
    filter-display: regex to filter service display names as they appear in service manager
    exclude-display: regex to exclude service display names
  An example of a configuration file:
  {
    "check": "service",
    "args": {
      "warning-state": "stopped",
      "critical-state": "running",
      "warning-total-running": 20,
      "critical-total-running": 150,
      "start-auto": true,
      "filter-name": ".*",
      "exclude-name": ".*"
    }
  }
  Examples of output:
    OK: all services are running
    In case of a too restricted filter:
      CRITICAL: no service found
    In case on some services not in running state:
      OK: services: 1 stopped, 1 starting, 1 stopping
    In case of a service in a critical state:
      CRITICAL: services: 1 stopped, 1 starting, 1 stopping CRITICAL: logon is stopped CRITICAL: httpd is stopping
  Metrics:
    services.stopped.count
    services.starting.count
    services.stopping.count
    services.running.count
    services.continuing.count
    services.pausing.count
    services.paused.count
)";
}

namespace com::centreon::agent {
template class native_check_base<
    native_check_detail::e_service_metric::nb_service_metric>;
}
