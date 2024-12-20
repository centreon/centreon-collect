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

#ifndef CENTREON_AGENT_NATIVE_CHECK_SERVICE_HH
#define CENTREON_AGENT_NATIVE_CHECK_SERVICE_HH

#include "native_check_base.hh"

namespace com::centreon::agent {
namespace native_check_detail {

enum e_service_metric : unsigned {
  stopped,
  start_pending,
  stop_pending,
  running,
  continue_pending,
  pause_pending,
  paused,
  total,
  nb_service_metric
};

/**
 * @brief service filter
 * it can filter services by their name and also by their start_auto
 */
class service_filter {
  using string_set = absl::flat_hash_set<std::string>;

  string_set _name_cache_allowed;
  string_set _name_cache_excluded;
  string_set _display_cache_allowed;
  string_set _display_cache_excluded;

  std::unique_ptr<re2::RE2> _name_filter, _name_filter_exclude;
  std::unique_ptr<re2::RE2> _display_filter, _display_filter_exclude;

  std::optional<bool> _start_auto;

 public:
  service_filter(const rapidjson::Value& args);

  bool is_allowed(bool start_auto,
                  const std::string_view& service_name,
                  const std::string_view& service_display);

  bool use_start_auto_filter() const { return _start_auto.has_value(); }
};

/**
 * @brief service enumerator
 * enumerate services and call a callback on each service allowed by filter
 */
class service_enumerator {
 public:
  using listener = std::function<void(const ENUM_SERVICE_STATUSA&)>;
  using constructor = std::function<std::unique_ptr<service_enumerator>()>;

 private:
  template <bool start_auto>
  void _enumerate_services(service_filter& filter,
                           listener&& callback,
                           const std::shared_ptr<spdlog::logger>& logger);

 protected:
  static constexpr size_t service_array_size = 512;

  SC_HANDLE _sc_manager_handler = nullptr;
  DWORD _resume_handle = 0;

  using serv_array = ENUM_SERVICE_STATUSA[service_array_size];

  virtual bool _enumerate_services(serv_array& services,
                                   DWORD* services_returned);

  virtual bool _query_service_config(
      LPCSTR service_name,
      QUERY_SERVICE_CONFIGA& serv_conf,
      const std::shared_ptr<spdlog::logger>& logger);

 public:
  service_enumerator();

  void reset_resume_handle() { _resume_handle = 0; }

  virtual ~service_enumerator();

  void enumerate_services(service_filter& filter,
                          listener&& callback,
                          const std::shared_ptr<spdlog::logger>& logger);
};

/**
 * snapshot of services informations, used to create output and perfdatas
 */
class w_service_info : public snapshot<e_service_metric::nb_service_metric> {
  std::string _output;
  unsigned _state_to_warning;
  unsigned _state_to_critical;
  e_status _status = e_status::ok;

 public:
  w_service_info(service_enumerator& service_enumerator,
                 service_filter& filter,
                 unsigned state_to_warning,
                 unsigned state_to_critical,
                 const std::shared_ptr<spdlog::logger>& logger);

  void on_service(const ENUM_SERVICE_STATUSA& service_status);

  e_status get_status() const { return _status; }

  void dump_to_output(std::string* output) const override;
};

}  // namespace native_check_detail

/**
 * @brief native final check object
 *
 */
class check_service
    : public native_check_base<
          native_check_detail::e_service_metric::nb_service_metric> {
  /**
   * @brief these enums are indexed by service states values
   * https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_status
   */
  enum state_mask : unsigned {
    stopped = 1,
    start_pending = 2,
    stop_pending = 4,
    running = 8,
    continue_pending = 16,
    pause_pending = 32,
    paused = 64
  };

  static const std::array<std::pair<std::string_view, state_mask>, 7>
      _label_state;

  unsigned _state_to_warning = 0;
  unsigned _state_to_critical = 0;
  native_check_detail::service_filter _filter;
  std::unique_ptr<native_check_detail::service_enumerator> _enumerator;

 public:
  /**
   * in order to mock services API, this static constructor is used to replace
   * service_enumarator by a mock
   */
  static native_check_detail::service_enumerator::constructor
      _enumerator_constructor;

  check_service(const std::shared_ptr<asio::io_context>& io_context,
                const std::shared_ptr<spdlog::logger>& logger,
                time_point first_start_expected,
                duration check_interval,
                const std::string& serv,
                const std::string& cmd_name,
                const std::string& cmd_line,
                const rapidjson::Value& args,
                const engine_to_agent_request_ptr& cnf,
                check::completion_handler&& handler,
                const checks_statistics::pointer& stat);

  std::shared_ptr<native_check_detail::snapshot<
      native_check_detail::e_service_metric::nb_service_metric>>
  measure() override;

  static void help(std::ostream& help_stream);

  const std::vector<native_check_detail::metric_definition>&
  get_metric_definitions() const override;
};

}  // namespace com::centreon::agent
#endif
