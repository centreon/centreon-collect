/*
** Copyright 2020-2021 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/anomalydetection.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/logging.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/grab_host.hh"
#include "com/centreon/engine/macros/grab_service.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/exceptions/interruption.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

CCE_BEGIN()
namespace commands {
class cancellable_command : public command {
  command::pointer _original_command;

  /**
   * @brief if _fake_result is set, check isn't executed
   *
   */
  check_result::pointer _fake_result;

  static const std::string _empty;

 public:
  cancellable_command(const command::pointer& original_command)
      : command(original_command ? original_command->get_name()
                                 : "cancellable_command",
                original_command ? original_command->get_command_line() : ""),
        _original_command(original_command) {}

  void set_fake_result(const check_result::pointer& res) { _fake_result = res; }
  void reset_fake_result() { _fake_result.reset(); }

  void set_original_command(const command::pointer& original_command) {
    _original_command = original_command;
  }

  const std::string& get_command_line() const noexcept override;
  void set_command_line(const std::string& command_line) noexcept override;

  inline const command::pointer& get_original_command() const {
    return _original_command;
  }

  uint64_t run(const std::string& processed_cmd,
               nagios_macros& macors,
               uint32_t timeout,
               const check_result::pointer& to_push_to_checker,
               const void* caller = nullptr) override;
  void run(const std::string& process_cmd,
           nagios_macros& macros,
           uint32_t timeout,
           result& res) override;
};

const std::string cancellable_command::_empty;

/**
 *  Run a command.
 *
 *  @param[in] args    The command arguments.
 *  @param[in] macros  The macros data struct.
 *  @param[in] timeout The command timeout.
 *  @param[in] to_push_to_checker This check_result will be pushed to checher.
 *  @param[in] caller  pointer to the caller
 *
 *  @return The command id or 0 if it uses the perf_data of dependent_service
 */
uint64_t cancellable_command::run(
    const std::string& processed_cmd,
    nagios_macros& macors,
    uint32_t timeout,
    const check_result::pointer& to_push_to_checker,
    const void* caller) {
  if (_fake_result) {
    checks::checker::instance().add_check_result_to_reap(_fake_result);
    return 0;  // no command => no async result
  } else {
    if (_original_command) {
      uint64_t id = _original_command->run(processed_cmd, macors, timeout,
                                           to_push_to_checker, caller);
      log_v2::checks()->debug(
          "cancellable_command::run command launched id={} cmd {}", id,
          _original_command);
      return id;
    } else {
      log_v2::checks()->debug("cancellable_command::run no original command");
      return 0;
    }
  }
}

void cancellable_command::run(const std::string& process_cmd,
                              nagios_macros& macros,
                              uint32_t timeout,
                              result& res) {
  if (_fake_result) {
    res = result(*_fake_result);
    _fake_result.reset();
  } else {
    if (_original_command) {
      _original_command->run(process_cmd, macros, timeout, res);
    }
  }
}

const std::string& cancellable_command::get_command_line() const noexcept {
  if (_original_command) {
    return _original_command->get_command_line();
  } else {
    log_v2::commands()->error(
        "cancellable_command::get_command_line: original command no set");
    return _empty;
  }
}

void cancellable_command::set_command_line(
    const std::string& command_line) noexcept {
  if (_original_command) {
    _original_command->set_command_line(command_line);
  } else {
    log_v2::commands()->error(
        "cancellable_command::set_command_line: original command no set");
  }
}
}  // namespace commands

CCE_END()

/**
 *  Anomaly detection constructor
 *
 *  @param[in] host_name                    Name of the host this
 *                                          service is running on.
 *  @param[in] description                  Service description.
 *  @param[in] display_name                 Display name.
 *  @param[in] internal_id                  Configuration AD unique id
 *  @param[in] dependent_service            Dependent service
 *  @param[in] metric_name                  Metric to consider.
 *  @param[in] thresholds_file              Full path of the file containing
 *                                          metric thresholds.
 *  @param[in] status_change                Should we follow the thresholds file
 *                                          to determine status.
 *  @param[in] initial_state                Initial service state.
 *  @param[in] max_attempts                 Max check attempts.
 *  @param[in] accept_passive_checks        Does this service accept
 *                                          check result submission ?
 *  @param[in] check_interval               Normal check interval.
 *  @param[in] retry_interval               Retry check interval.
 *  @param[in] notification_interval        Notification interval.
 *  @param[in] first_notification_delay     First notification delay.
 *  @param[in] recovery_notification_delay  Recovery notification delay.
 *  @param[in] notification_period          Notification timeperiod
 *                                          name.
 *  @param[in] notify_recovery              Does this service notify
 *                                          when recovering ?
 *  @param[in] notify_unknown               Does this service notify in
 *                                          unknown state ?
 *  @param[in] notify_warning               Does this service notify in
 *                                          warning state ?
 *  @param[in] notify_critical              Does this service notify in
 *                                          critical state ?
 *  @param[in] notify_flapping              Does this service notify
 *                                          when flapping ?
 *  @param[in] notify_downtime              Does this service notify on
 *                                          downtime ?
 *  @param[in] notifications_enabled        Are notifications enabled
 *                                          for this service ?
 *  @param[in] is_volatile                  Is this service volatile ?
 *  @param[in] event_handler                Event handler command name.
 *  @param[in] event_handler_enabled        Whether or not event handler
 *                                          is enabled.
 *  @param[in] checks_enabled               Are active checks enabled ?
 *  @param[in] flap_detection_enabled       Whether or not flap
 *                                          detection is enabled.
 *  @param[in] low_flap_threshold           Low flap threshold.
 *  @param[in] high_flap_threshold          High flap threshold.
 *  @param[in] flap_detection_on_ok         Is flap detection enabled
 *                                          for ok state ?
 *  @param[in] flap_detection_on_warning    Is flap detection enabled
 *                                          for warning state ?
 *  @param[in] flap_detection_on_unknown    Is flap detection enabled
 *                                          for unknown state ?
 *  @param[in] flap_detection_on_critical   Is flap detection enabled
 *                                          for critical state ?
 *  @param[in] stalk_on_ok                  Stalk on ok state ?
 *  @param[in] stalk_on_warning             Stalk on warning state ?
 *  @param[in] stalk_on_unknown             Stalk on unknown state ?
 *  @param[in] stalk_on_critical            Stalk on critical state ?
 *  @param[in] process_perfdata             Whether or not service
 *                                          performance data should be
 *                                          processed.
 *  @param[in] check_freshness              Enable freshness check ?
 *  @param[in] freshness_threshold          Freshness threshold.
 *  @param[in] notes                        Notes.
 *  @param[in] notes_url                    URL.
 *  @param[in] action_url                   Action URL.
 *  @param[in] icon_image                   Icon image.
 *  @param[in] icon_image_alt               Alternative icon image.
 *  @param[in] retain_status_information    Should Engine retain service
 *                                          status information ?
 *  @param[in] retain_nonstatus_information Should Engine retain service
 *                                          non-status information ?
 *  @param[in] obsess_over_service          Should we obsess over
 *                                          service ?
 *
 *  @return New service.
 */
anomalydetection::anomalydetection(uint64_t host_id,
                                   uint64_t service_id,
                                   std::string const& hostname,
                                   std::string const& description,
                                   std::string const& display_name,
                                   uint64_t internal_id,
                                   service* dependent_service,
                                   std::string const& metric_name,
                                   std::string const& thresholds_file,
                                   bool status_change,
                                   bool checks_enabled,
                                   bool accept_passive_checks,
                                   enum service::service_state initial_state,
                                   uint32_t check_interval,
                                   uint32_t retry_interval,
                                   uint32_t notification_interval,
                                   int max_attempts,
                                   uint32_t first_notification_delay,
                                   uint32_t recovery_notification_delay,
                                   std::string const& notification_period,
                                   bool notifications_enabled,
                                   bool is_volatile,
                                   std::string const& event_handler,
                                   bool event_handler_enabled,
                                   std::string const& notes,
                                   std::string const& notes_url,
                                   std::string const& action_url,
                                   std::string const& icon_image,
                                   std::string const& icon_image_alt,
                                   bool flap_detection_enabled,
                                   double low_flap_threshold,
                                   double high_flap_threshold,
                                   bool check_freshness,
                                   int freshness_threshold,
                                   bool obsess_over,
                                   std::string const& timezone,
                                   uint64_t icon_id)
    : service{hostname,
              description,
              display_name,
              "",
              checks_enabled,
              accept_passive_checks,
              initial_state,
              check_interval,
              retry_interval,
              notification_interval,
              max_attempts,
              first_notification_delay,
              recovery_notification_delay,
              notification_period,
              notifications_enabled,
              is_volatile,
              dependent_service->check_period(),
              event_handler,
              event_handler_enabled,
              notes,
              notes_url,
              action_url,
              icon_image,
              icon_image_alt,
              flap_detection_enabled,
              low_flap_threshold,
              high_flap_threshold,
              check_freshness,
              freshness_threshold,
              obsess_over,
              timezone,
              icon_id,
              ANOMALY_DETECTION},
      _internal_id{internal_id},
      _metric_name{metric_name},
      _thresholds_file{thresholds_file},
      _status_change{status_change},
      _thresholds_file_viable{false} {
  set_host_id(host_id);
  set_service_id(service_id);
  init_thresholds();
  set_dependent_service(dependent_service);
}

/**
 *  Add a new anomalydetection to the list in memory.
 *
 *  @param[in] host_name                    Name of the host this
 *                                          service is running on.
 *  @param[in] description                  Service description.
 *  @param[in] display_name                 Display name.
 *  @param[in] internal_id                  Configuration id of this AD
 *  @param[in] dependent_service_id         Dependent service id.
 *  @param[in] metric_name                  Metric to consider.
 *  @param[in] thresholds_file,             fullname to the thresholds file.
 *  @param[in] status_change,               should we follow the thresholds file
 *                                          to determine status.
 *  @param[in] initial_state                Initial service state.
 *  @param[in] max_attempts                 Max check attempts.
 *  @param[in] accept_passive_checks        Does this service accept
 *                                          check result submission ?
 *  @param[in] check_interval               Normal check interval.
 *  @param[in] retry_interval               Retry check interval.
 *  @param[in] notification_interval        Notification interval.
 *  @param[in] first_notification_delay     First notification delay.
 *  @param[in] recovery_notification_delay  Recovery notification delay.
 *  @param[in] notification_period          Notification timeperiod
 *                                          name.
 *  @param[in] notify_recovery              Does this service notify
 *                                          when recovering ?
 *  @param[in] notify_unknown               Does this service notify in
 *                                          unknown state ?
 *  @param[in] notify_warning               Does this service notify in
 *                                          warning state ?
 *  @param[in] notify_critical              Does this service notify in
 *                                          critical state ?
 *  @param[in] notify_flapping              Does this service notify
 *                                          when flapping ?
 *  @param[in] notify_downtime              Does this service notify on
 *                                          downtime ?
 *  @param[in] notifications_enabled        Are notifications enabled
 *                                          for this service ?
 *  @param[in] is_volatile                  Is this service volatile ?
 *  @param[in] event_handler                Event handler command name.
 *  @param[in] event_handler_enabled        Whether or not event handler
 *                                          is enabled.
 *  @param[in] checks_enabled               Are active checks enabled ?
 *  @param[in] flap_detection_enabled       Whether or not flap
 *                                          detection is enabled.
 *  @param[in] low_flap_threshold           Low flap threshold.
 *  @param[in] high_flap_threshold          High flap threshold.
 *  @param[in] flap_detection_on_ok         Is flap detection enabled
 *                                          for ok state ?
 *  @param[in] flap_detection_on_warning    Is flap detection enabled
 *                                          for warning state ?
 *  @param[in] flap_detection_on_unknown    Is flap detection enabled
 *                                          for unknown state ?
 *  @param[in] flap_detection_on_critical   Is flap detection enabled
 *                                          for critical state ?
 *  @param[in] stalk_on_ok                  Stalk on ok state ?
 *  @param[in] stalk_on_warning             Stalk on warning state ?
 *  @param[in] stalk_on_unknown             Stalk on unknown state ?
 *  @param[in] stalk_on_critical            Stalk on critical state ?
 *  @param[in] process_perfdata             Whether or not service
 *                                          performance data should be
 *                                          processed.
 *  @param[in] check_freshness              Enable freshness check ?
 *  @param[in] freshness_threshold          Freshness threshold.
 *  @param[in] notes                        Notes.
 *  @param[in] notes_url                    URL.
 *  @param[in] action_url                   Action URL.
 *  @param[in] icon_image                   Icon image.
 *  @param[in] icon_image_alt               Alternative icon image.
 *  @param[in] retain_status_information    Should Engine retain service
 *                                          status information ?
 *  @param[in] retain_nonstatus_information Should Engine retain service
 *                                          non-status information ?
 *  @param[in] obsess_over_service          Should we obsess over
 *                                          service ?
 *
 *  @return New service.
 */
com::centreon::engine::anomalydetection* add_anomalydetection(
    uint64_t host_id,
    uint64_t service_id,
    std::string const& host_name,
    std::string const& description,
    std::string const& display_name,
    uint64_t internal_id,
    uint64_t dependent_service_id,
    std::string const& metric_name,
    std::string const& thresholds_file,
    bool status_change,
    com::centreon::engine::service::service_state initial_state,
    int max_attempts,
    double check_interval,
    double retry_interval,
    double notification_interval,
    uint32_t first_notification_delay,
    uint32_t recovery_notification_delay,
    std::string const& notification_period,
    bool notify_recovery,
    bool notify_unknown,
    bool notify_warning,
    bool notify_critical,
    bool notify_flapping,
    bool notify_downtime,
    bool notifications_enabled,
    bool is_volatile,
    std::string const& event_handler,
    bool event_handler_enabled,
    bool checks_enabled,
    bool accept_passive_checks,
    bool flap_detection_enabled,
    double low_flap_threshold,
    double high_flap_threshold,
    bool flap_detection_on_ok,
    bool flap_detection_on_warning,
    bool flap_detection_on_unknown,
    bool flap_detection_on_critical,
    bool stalk_on_ok,
    bool stalk_on_warning,
    bool stalk_on_unknown,
    bool stalk_on_critical,
    int process_perfdata,
    bool check_freshness,
    int freshness_threshold,
    std::string const& notes,
    std::string const& notes_url,
    std::string const& action_url,
    std::string const& icon_image,
    std::string const& icon_image_alt,
    int retain_status_information,
    int retain_nonstatus_information,
    bool obsess_over_service,
    std::string const& timezone,
    uint64_t icon_id) {
  // Make sure we have everything we need.
  if (!service_id) {
    engine_logger(log_config_error, basic)
        << "Error: Service comes from a database, therefore its service id "
        << "must not be null";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: Service comes from a database, therefore its service id must "
        "not be null");
    return nullptr;
  } else if (description.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Service description is not set";
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Error: Service description is not set");
    return nullptr;
  } else if (!host_name.empty()) {
    uint64_t hid = get_host_id(host_name);
    if (hid != host_id) {
      engine_logger(log_config_error, basic)
          << "Error: host id (" << host_id << ") of host ('" << host_name
          << "') of anomaly detection service '" << description
          << "' has a conflict between config does not match with the config "
             "id ("
          << hid << ")";
      SPDLOG_LOGGER_ERROR(
          log_v2::config(),
          "Error: host id ({}) of host ('{}') of anomaly detection service "
          "'{}' has a conflict between config does not match with the config "
          "id ({})",
          host_id, host_name, description, hid);
      return nullptr;
    }
  }

  if (internal_id == 0) {
    engine_logger(log_config_error, basic)
        << "Error: The internal_id in the anomaly detection configuration is "
           "mandatory";
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Error: The internal_id in the anomaly detection "
                        "configuration is mandatory");
    return nullptr;
  }
  auto it = service::services_by_id.find({host_id, dependent_service_id});
  if (it == service::services_by_id.end()) {
    engine_logger(log_config_error, basic)
        << "Error: Dependent service " << dependent_service_id
        << " does not exist (anomaly detection " << service_id << ")";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: Dependent service {} does not exist (anomaly detection {})",
        dependent_service_id, service_id);
    return nullptr;
  }
  service* dependent_service = it->second.get();

  if (metric_name.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: metric name must be provided for an anomaly detection "
           "service (host_id:"
        << host_id << ", service_id:" << service_id << ")";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: metric name must be provided for an anomaly detection "
        "service (host_id:{}, service_id:{})",
        host_id, service_id);
    return nullptr;
  }

  if (thresholds_file.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: thresholds file must be provided for an anomaly detection "
           "service (host_id:"
        << host_id << ", service_id:" << service_id << ")";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: thresholds file must be provided for an anomaly detection "
        "service (host_id:{}, service_id:{})",
        host_id, service_id);
    return nullptr;
  }

  // Check values.
  if (max_attempts <= 0 || check_interval < 0 || retry_interval <= 0 ||
      notification_interval < 0) {
    engine_logger(log_config_error, basic)
        << "Error: Invalid max_attempts, check_interval, retry_interval"
           ", or notification_interval value for service '"
        << description << "' on host '" << host_name << "'";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: Invalid max_attempts, check_interval, retry_interval"
        ", or notification_interval value for service '{}' on host '{}'",
        description, host_name);
    return nullptr;
  }
  // Check if the service is already exist.
  std::pair<uint64_t, uint64_t> id(std::make_pair(host_id, service_id));
  if (is_service_exist(id)) {
    engine_logger(log_config_error, basic)
        << "Error: Service '" << description << "' on host '" << host_name
        << "' has already been defined";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: Service '{}' on host '{}' has already been defined",
        description, host_name);
    return nullptr;
  }

  // Allocate memory.
  auto obj{std::make_shared<anomalydetection>(
      host_id, service_id, host_name, description,
      display_name.empty() ? description : display_name, internal_id,
      dependent_service, metric_name, thresholds_file, status_change,
      checks_enabled, accept_passive_checks, initial_state, check_interval,
      retry_interval, notification_interval, max_attempts,
      first_notification_delay, recovery_notification_delay,
      notification_period, notifications_enabled, is_volatile, event_handler,
      event_handler_enabled, notes, notes_url, action_url, icon_image,
      icon_image_alt, flap_detection_enabled, low_flap_threshold,
      high_flap_threshold, check_freshness, freshness_threshold,
      obsess_over_service, timezone, icon_id)};
  try {
    obj->set_acknowledgement_type(ACKNOWLEDGEMENT_NONE);
    obj->set_check_options(CHECK_OPTION_NONE);
    uint32_t flap_detection_on;
    flap_detection_on = none;
    flap_detection_on |=
        (flap_detection_on_critical > 0 ? notifier::critical : 0);
    flap_detection_on |= (flap_detection_on_ok > 0 ? notifier::ok : 0);
    flap_detection_on |=
        (flap_detection_on_unknown > 0 ? notifier::unknown : 0);
    flap_detection_on |=
        (flap_detection_on_warning > 0 ? notifier::warning : 0);
    obj->set_flap_detection_on(flap_detection_on);
    obj->set_modified_attributes(MODATTR_NONE);
    uint32_t notify_on;
    notify_on = none;
    notify_on |= (notify_critical > 0 ? notifier::critical : 0);
    notify_on |= (notify_downtime > 0 ? notifier::downtime : 0);
    notify_on |= (notify_flapping > 0
                      ? (notifier::flappingstart | notifier::flappingstop |
                         notifier::flappingdisabled)
                      : 0);
    notify_on |= (notify_recovery > 0 ? notifier::ok : 0);
    notify_on |= (notify_unknown > 0 ? notifier::unknown : 0);
    notify_on |= (notify_warning > 0 ? notifier::warning : 0);
    obj->set_notify_on(notify_on);
    obj->set_process_performance_data(process_perfdata > 0);
    obj->set_retain_nonstatus_information(retain_nonstatus_information > 0);
    obj->set_retain_status_information(retain_status_information > 0);
    obj->set_should_be_scheduled(true);
    uint32_t stalk_on = (stalk_on_critical ? notifier::critical : 0) |
                        (stalk_on_ok ? notifier::ok : 0) |
                        (stalk_on_unknown ? notifier::unknown : 0) |
                        (stalk_on_warning ? notifier::warning : 0);
    obj->set_stalk_on(stalk_on);
    obj->set_state_type(notifier::hard);

    // state_ok = 0, so we don't need to set state_history (memset
    // is used before).
    // for (unsigned int x(0); x < MAX_STATE_HISTORY_ENTRIES; ++x)
    //   obj->state_history[x] = state_ok;

    // Add new items to the list.
    service::services[{obj->get_hostname(), obj->get_description()}] = obj;
    service::services_by_id[{host_id, service_id}] = obj;
  } catch (...) {
    obj.reset();
  }

  return obj.get();
}

uint64_t anomalydetection::get_internal_id() const {
  return _internal_id;
}

service* anomalydetection::get_dependent_service() const {
  return _dependent_service;
}

void anomalydetection::set_internal_id(uint64_t id) {
  _internal_id = id;
}

void anomalydetection::set_dependent_service(service* svc) {
  _dependent_service = svc;
}

void anomalydetection::set_metric_name(std::string const& name) {
  _metric_name = name;
}

void anomalydetection::set_thresholds_file(std::string const& file) {
  std::lock_guard<std::mutex> lock(_thresholds_m);
  _thresholds_file = file;
}

/*
 * forks a child process to run a service check, but does not wait for the
 * service check result
 */
int anomalydetection::run_async_check(int check_options,
                                      double latency,
                                      bool scheduled_check,
                                      bool reschedule_check,
                                      bool* time_is_valid,
                                      time_t* preferred_time) noexcept {
  engine_logger(dbg_functions, basic)
      << "anomalydetection::run_async_check, check_options=" << check_options
      << ", latency=" << latency << ", scheduled_check=" << scheduled_check
      << ", reschedule_check=" << reschedule_check;

  SPDLOG_LOGGER_TRACE(
      log_v2::functions(),
      "anomalydetection::run_async_check, check_options={}, latency={}, "
      "scheduled_check={}, reschedule_check={}",
      check_options, latency, scheduled_check, reschedule_check);

  engine_logger(dbg_checks, basic)
      << "** Running async check of anomalydetection '" << get_description()
      << "' on host '" << get_hostname() << "'...";

  SPDLOG_LOGGER_TRACE(
      log_v2::checks(),
      "** Running async check of anomalydetection '{} ' on host '{}'...",
      get_description(), get_hostname());

  // Check if the service is viable now.
  if (!verify_check_viability(check_options, time_is_valid, preferred_time))
    return ERROR;

  // need to update original command?
  if (!get_check_command_ptr()) {
    set_check_command_ptr(std::make_shared<commands::cancellable_command>(
        _dependent_service->get_check_command_ptr()));
    service* group[2] = {this, _dependent_service};
    _dependent_service->get_check_command_ptr()->add_caller_group(group,
                                                                  group + 2);
  }
  if (std::static_pointer_cast<commands::cancellable_command>(
          get_check_command_ptr())
          ->get_original_command() !=
      _dependent_service->get_check_command_ptr()) {
    std::static_pointer_cast<commands::cancellable_command>(
        get_check_command_ptr())
        ->set_original_command(_dependent_service->get_check_command_ptr());
    service* group[2] = {this, _dependent_service};
    _dependent_service->get_check_command_ptr()->add_caller_group(group,
                                                                  group + 2);
  }

  if (get_current_state() == service::service_state::state_ok) {
    // if state is ok we don't execute command
    std::string dependent_perf_data = _dependent_service->get_perf_data();
    struct timeval now;
    gettimeofday(&now, nullptr);
    check_result::pointer fake_res = std::make_shared<check_result>(
        check_source::service_check, this, checkable::check_active,
        check_options, reschedule_check, latency, now, now, true, false,
        service_state::state_unknown,
        "failed to calc check_result from perf_data");
    if (!parse_perfdata(dependent_perf_data, time(nullptr), *fake_res)) {
      SPDLOG_LOGGER_ERROR(log_v2::checks(),
                          "parse_perfdata failed => unknown state");
    } else {
      SPDLOG_LOGGER_TRACE(
          log_v2::checks(),
          "** Running async check of anomalydetection '{} ' on host '{}'... "
          "without check",
          get_description(), get_hostname());
    }
    std::static_pointer_cast<commands::cancellable_command>(
        get_check_command_ptr())
        ->set_fake_result(fake_res);
  } else {
    if (!std::static_pointer_cast<commands::cancellable_command>(
             get_check_command_ptr())
             ->get_original_command()) {
      SPDLOG_LOGGER_ERROR(
          log_v2::checks(),
          "anomaly: no original commands for host {} => do nothing",
          get_hostname());
      return ERROR;
    }
    SPDLOG_LOGGER_TRACE(
        log_v2::checks(),
        "** Running async check of anomalydetection '{} ' on host '{}'... with "
        "check",
        get_description(), get_hostname());
    std::static_pointer_cast<commands::cancellable_command>(
        get_check_command_ptr())
        ->reset_fake_result();  // execute original commands
  }

  return service::run_async_check(check_options, latency, scheduled_check,
                                  reschedule_check, time_is_valid,
                                  preferred_time);
}

int anomalydetection::handle_async_check_result(
    const check_result& queued_check_result) {
  std::string output{queued_check_result.get_output()};
  std::string plugin_output;
  std::string long_plugin_output;
  std::string perf_data;
  parse_check_output(output, plugin_output, long_plugin_output, perf_data, true,
                     false);

  perf_data = string::extract_perfdata(perf_data, _metric_name);

  check_result anomaly_check_result(queued_check_result);
  // mandatory to avoid service::handle_async_check_result to erase
  // parse_perfdata output
  anomaly_check_result.set_exited_ok(true);
  parse_perfdata(perf_data, queued_check_result.get_start_time().tv_sec,
                 anomaly_check_result);

  return service::handle_async_check_result(anomaly_check_result);
}

/**
 * @brief Parse the given perfdata. The only metric parsed is the one whose name
 * is _metric_name.
 *
 * @param perfdata A string containing perfdata.
 *
 * @return A tuple containing the status, the value, its unit, the lower bound
 * and the upper bound
 */
// std::tuple<service::service_state, double, std::string, double, double>
bool anomalydetection::parse_perfdata(std::string const& perfdata,
                                      time_t check_time,
                                      check_result& calculated_result) {
  std::ostringstream oss;

  if (!_thresholds_file_viable) {
    engine_logger(log_info_message, basic)
        << "The thresholds file is not viable "
           "(not available or not readable).";
    SPDLOG_LOGGER_ERROR(log_v2::checks(),
                        "The thresholds file is not viable "
                        "(not available or not readable).");
    oss << "The thresholds file is not viable for metric " << _metric_name
        << " | " << perfdata;
    calculated_result.set_output(oss.str());
    calculated_result.set_return_code(service_state::state_unknown);
    return false;
  }

  std::string without_thresholds(perfdata);
  string::remove_thresholds(without_thresholds);
  std::lock_guard<std::mutex> lock(_thresholds_m);
  size_t pos = without_thresholds.find_last_of("=");
  /* If the perfdata is wrong. */
  if (pos == std::string::npos) {
    engine_logger(log_runtime_error, basic)
        << "Error: Unable to parse perfdata '" << without_thresholds << "'";
    SPDLOG_LOGGER_ERROR(log_v2::runtime(),
                        "Error: Unable to parse perfdata '{}'",
                        without_thresholds);
    oss << "UNKNOWN: Unknown activity, " << _metric_name
        << " did not return any values"
        << " | " << perfdata;
    calculated_result.set_output(oss.str());
    calculated_result.set_return_code(service_state::state_unknown);
    return false;
  }

  /* If the perfdata is good. */
  pos++;
  char* unit;

  double value = std::strtod(without_thresholds.c_str() + pos, &unit);
  char const* end = without_thresholds.c_str() + without_thresholds.size() - 1;
  size_t l = 0;
  /* If there is a unit, it starts at unit char* */
  while (unit + l <= end && unit[l] != ' ' && unit[l] != ';')
    ++l;
  std::string uom = std::string(unit, l);

  service::service_state status;

  /* The check time is probably between two timestamps stored in _thresholds.
   *
   *   |                    d2 +
   *   |            dc+
   *   |   d1 +
   *   |
   *   +------+-------+--------+-->
   *         t1       tc       t2
   *
   * For both lower bound and upper bound, we get values d1 and d2
   * respectively corresponding to timestamp t1 and t2.
   * We have a check_time tc between them, and we would like a value dc
   * corresponding to this timestamp.
   *
   * The linear approximation gives the formula:
   *                       dc = (d2-d1) * (tc-t1) / (t2-t1) + d1
   */
  auto it2 = _thresholds.upper_bound(check_time);
  auto it1 = it2;
  if (it2 == _thresholds.end()) {
    engine_logger(log_runtime_error, basic)
        << "Error: the thresholds file is too old "
           "compared to the check timestamp "
        << check_time;
    SPDLOG_LOGGER_ERROR(log_v2::runtime(),
                        "Error: the thresholds file is too old "
                        "compared to the check timestamp {}",
                        check_time);
    oss << "The thresholds file is too old "
           "compared to the check timestamp "
        << check_time << " for metric " << _metric_name << " | " << perfdata;
    calculated_result.set_output(oss.str());
    calculated_result.set_return_code(service_state::state_unknown);
    return false;
  }
  if (it1 != _thresholds.begin())
    --it1;
  else {
    engine_logger(log_runtime_error, basic)
        << "Error: timestamp " << check_time
        << " too old compared with the thresholds file";
    SPDLOG_LOGGER_ERROR(
        log_v2::runtime(),
        "Error: timestamp {} too old compared with the thresholds file",
        check_time);
    oss << "timestamp " << check_time
        << " is too old compared with the thresholds file for metric "
        << _metric_name << " | " << perfdata << without_thresholds;
    ;
    calculated_result.set_output(oss.str());
    calculated_result.set_return_code(service_state::state_unknown);
    return false;
  }

  /* Now it1.first <= check_time < it2.first */
  double upper = (it2->second.second - it1->second.second) *
                     (check_time - it1->first) / (it2->first - it1->first) +
                 it1->second.second;
  double lower = (it2->second.first - it1->second.first) *
                     (check_time - it1->first) / (it2->first - it1->first) +
                 it1->second.first;

  if (!_status_change)
    status = service::state_ok;
  else {
    if (std::isnan(value))
      status = service::state_unknown;
    else if (value >= lower && value <= upper)
      status = service::state_ok;
    else
      status = service::state_critical;
  }

  oss.setf(std::ios_base::fixed, std::ios_base::floatfield);
  oss.precision(2);
  calculated_result.set_early_timeout(false);
  calculated_result.set_exited_ok(true);
  if (status == service::state_ok)
    oss << "OK: Regular activity, " << _metric_name << '=' << value << uom
        << " |";
  else if (status == service::state_unknown && std::isnan(value))
    oss << "UNKNOWN: Unknown activity, " << _metric_name
        << " did not return any values| ";
  else {
    oss << "NON-OK: Unusual activity, the actual value of " << _metric_name
        << " is " << value << uom;
    if (!std::isnan(lower) && !std::isnan(upper))
      oss << " which is outside the forecasting range [" << lower << uom
          << " ; " << upper << uom << "] |";
    else
      oss << " and the forecasting range is unknown |";
  }

  calculated_result.set_return_code(status);

  oss << without_thresholds;

  std::string without_thresholds_nor_value;
  pos = without_thresholds.find(';');
  if (pos != std::string::npos)
    without_thresholds_nor_value = without_thresholds.substr(pos);

  if (!std::isnan(lower)) {
    oss << ' ' << _metric_name << "_lower_thresholds=" << lower << uom
        << without_thresholds_nor_value;
  }
  if (!std::isnan(upper)) {
    oss << ' ' << _metric_name << "_upper_thresholds=" << upper << uom
        << without_thresholds_nor_value;
  }
  /* We should master this string, so no need to check if it is utf-8 */
  calculated_result.set_output(oss.str());

  timestamp now(timestamp::now());

  // Update check result.
  timeval tv;
  gettimeofday(&tv, nullptr);
  calculated_result.set_finish_time(tv);

  return true;
}

void anomalydetection::init_thresholds() {
  std::lock_guard<std::mutex> lock(_thresholds_m);

  engine_logger(dbg_config, most)
      << "Trying to read thresholds file '" << _thresholds_file << "'";
  SPDLOG_LOGGER_DEBUG(log_v2::config(), "Trying to read thresholds file '{}'",
                      _thresholds_file);
  std::ifstream t;
  t.exceptions(t.exceptions() | std::ios::failbit);
  try {
    t.open(_thresholds_file);
  } catch (const std::system_error& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Fail to read thresholds file '{}' : {}",
                        _thresholds_file, e.code().message());
    return;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Fail to read thresholds file '{}' : {}",
                        _thresholds_file, e.what());
    return;
  }

  std::stringstream buffer;
  buffer << t.rdbuf();
  std::string err;
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(buffer.str());
  } catch (const nlohmann::json::parse_error& e) {
    engine_logger(log_config_error, basic)
        << "Error: the file '" << _thresholds_file
        << "' contains errors: " << e.what();
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Error: the file '{}' contains errors: {}",
                        _thresholds_file, e.what());
    return;
  }
  if (!json.is_array()) {
    engine_logger(log_config_error, basic)
        << "Error: the file '" << _thresholds_file
        << "' is not a thresholds file. Its global structure is not an array.";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: the file '{}' is not a thresholds file. Its global structure "
        "is not an array.",
        _thresholds_file);
    return;
  }

  bool found = false;
  for (auto it = json.begin(); it != json.end(); ++it) {
    uint64_t host_id, service_id;
    auto item = it.value();
    try {
      host_id = stoull(item["host_id"].get<std::string>());
      service_id = stoull(item["service_id"].get<std::string>());
    } catch (std::exception const& e) {
      engine_logger(log_config_error, basic)
          << "Error: host_id and service_id must "
             "be strings containing integers: "
          << e.what();
      SPDLOG_LOGGER_ERROR(log_v2::config(),
                          "Error: host_id and service_id must "
                          "be strings containing integers: {}",
                          e.what());
      return;
    }
    if (host_id == get_host_id() && service_id == get_service_id() &&
        item["metric_name"].get<std::string>() == _metric_name) {
      set_thresholds_no_lock(_thresholds_file, item["predict"]);
      if (!_thresholds_file_viable) {
        SPDLOG_LOGGER_ERROR(log_v2::config(),
                            "{} don't contain at least 2 thresholds datas for "
                            "host_id {} and service_id {}",
                            _thresholds_file, get_host_id(), get_service_id());
      }
      found = true;
      break;
    }
  }
  if (!found) {
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "{} don't contain datas for host_id {} and service_id {}",
        _thresholds_file, get_host_id(), get_service_id());
  }
}

/**
 * @brief Update all the anomaly detection services concerned by one thresholds
 *        file. The file has already been parsed and is translated into json.
 *
 * @param filename The fullname of the file to parse.
 */
int anomalydetection::update_thresholds(const std::string& filename) {
  engine_logger(log_info_message, most)
      << "Reading thresholds file '" << filename << "'.";
  SPDLOG_LOGGER_INFO(log_v2::checks(), "Reading thresholds file '{}'.",
                     filename);

  std::ifstream t;
  t.exceptions(t.exceptions() | std::ios::failbit);
  try {
    t.open(filename);
  } catch (const std::system_error& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Fail to read thresholds file '{}' : {}", filename,
                        e.code().message());
    return -1;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Fail to read thresholds file '{}' : {}", filename,
                        e.what());
    return -1;
  }

  std::stringstream buffer;
  buffer << t.rdbuf();
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(buffer.str());
  } catch (const nlohmann::json::parse_error& e) {
    engine_logger(log_config_error, basic)
        << "Error: The thresholds file '" << filename
        << "' should be a json file: " << e.what();
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: The thresholds file '{}' should be a json file: {}", filename,
        e.what());
    return -2;
  }

  if (!json.is_array()) {
    engine_logger(log_config_error, basic)
        << "Error: the file '" << filename
        << "' is not a thresholds file. Its global structure is not an array.";
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "Error: the file '{}' is not a thresholds file. Its global structure "
        "is not an array.",
        filename);
    return -3;
  }

  for (auto it = json.begin(); it != json.end(); ++it) {
    uint64_t host_id, svc_id;
    auto item = it.value();
    try {
      host_id = stoull(item["host_id"].get<std::string>());
      svc_id = stoull(item["service_id"].get<std::string>());
    } catch (std::exception const& e) {
      engine_logger(log_config_error, basic)
          << "Error: host_id and service_id must "
             "be strings containing integers: "
          << e.what();
      SPDLOG_LOGGER_ERROR(log_v2::config(),
                          "Error: host_id and service_id must "
                          "be strings containing integers: {}",
                          e.what());
      continue;
    }
    auto found = service::services_by_id.find({host_id, svc_id});
    if (found == service::services_by_id.end()) {
      engine_logger(log_config_error, basic)
          << "Error: The thresholds file contains thresholds for the anomaly "
             "detection service (host_id: "
          << host_id << ", service_id: " << svc_id << ") that does not exist";
      SPDLOG_LOGGER_ERROR(
          log_v2::config(),
          "Error: The thresholds file contains thresholds for the anomaly "
          "detection service (host_id: {}, service_id: {}) that does not exist",
          host_id, svc_id);
      continue;
    }
    std::shared_ptr<anomalydetection> ad =
        std::dynamic_pointer_cast<anomalydetection>(found->second);
    if (!ad) {
      SPDLOG_LOGGER_ERROR(
          log_v2::config(),
          "host_id: {}, service_id: {} is not an anomaly detection service",
          host_id, svc_id);
      continue;
    }
    const std::string& metric_name(item["metric_name"].get<std::string>());
    if (ad->get_metric_name() != metric_name) {
      engine_logger(log_config_error, basic)
          << "Error: The thresholds file contains thresholds for the anomaly "
             "detection service (host_id: "
          << ad->get_host_id() << ", service_id: " << ad->get_service_id()
          << ") with metric_name='" << metric_name
          << "' whereas the configured metric name is '"
          << ad->get_metric_name() << "'";
      SPDLOG_LOGGER_ERROR(
          log_v2::config(),
          "Error: The thresholds file contains thresholds for the anomaly "
          "detection service (host_id: {}, service_id: {}) with "
          "metric_name='{}' whereas the configured metric name is '{}'",
          ad->get_host_id(), ad->get_service_id(), metric_name,
          ad->get_metric_name());
      continue;
    }
    engine_logger(log_info_message, basic)
        << "Filling thresholds in anomaly detection (host_id: "
        << ad->get_host_id() << ", service_id: " << ad->get_service_id()
        << ", metric: " << ad->get_metric_name() << ")";
    SPDLOG_LOGGER_INFO(
        log_v2::checks(),
        "Filling thresholds in anomaly detection (host_id: {}, service_id: {}, "
        "metric: {})",
        ad->get_host_id(), ad->get_service_id(), ad->get_metric_name());

    ad->set_thresholds_lock(filename, item["predict"]);
  }
  return 0;
}

void anomalydetection::set_thresholds_lock(const std::string& filename,
                                           const nlohmann::json& thresholds) {
  std::lock_guard<std::mutex> _lock(_thresholds_m);
  set_thresholds_no_lock(filename, thresholds);
}

void anomalydetection::set_thresholds_no_lock(
    const std::string& filename,
    const nlohmann::json& thresholds) {
  if (_thresholds_file != filename) {
    _thresholds_file = filename;
  }
  _thresholds.clear();
  for (auto& threshold_obj : thresholds) {
    time_t timestamp =
        static_cast<time_t>(threshold_obj["timestamp"].get<uint64_t>());
    double upper = threshold_obj["upper"].get<double>();
    double lower = threshold_obj["lower"].get<double>();
    _thresholds.emplace_hint(
        _thresholds.end(),
        std::make_pair(timestamp, std::make_pair(lower, upper)));
  }
  if (_thresholds.size() > 1) {
    engine_logger(dbg_config, most)
        << "host_id=" << get_host_id() << " serv_id=" << get_service_id()
        << " Number of rows in memory: " << _thresholds.size();
    SPDLOG_LOGGER_DEBUG(log_v2::config(),
                        "host_id={} serv_id={} Number of rows in memory: {}",
                        get_host_id(), get_service_id(), _thresholds.size());
    _thresholds_file_viable = true;
  } else {
    engine_logger(dbg_config, most)
        << "Nothing in memory " << _thresholds.size()
        << " for host_id=" << get_host_id() << " serv_id=" << get_service_id();
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "Nothing in memory {} for host_id={} servid={}",
                        _thresholds.size(), get_host_id(), get_service_id());
    _thresholds_file_viable = false;
  }
}

void anomalydetection::set_status_change(bool status_change) {
  _status_change = status_change;
}

const std::string& anomalydetection::get_metric_name() const {
  return _metric_name;
}

const std::string& anomalydetection::get_thresholds_file() const {
  return _thresholds_file;
}

void anomalydetection::resolve(int& w, int& e) {
  set_check_period(_dependent_service->check_period());
  service::resolve(w, e);
}
