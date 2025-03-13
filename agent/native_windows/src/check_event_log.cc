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
#include <format>
#include <ranges>
#include "boost/algorithm/string/replace.hpp"

#include "check.hh"
#include "check_event_log.hh"
#include "event_log/container.hh"
#include "event_log/data.hh"

#include "com/centreon/common/rapidjson_helper.hh"
#include "event_log/uniq.hh"
#include "windows_util.hh"

using namespace com::centreon::agent;

/**
 * @brief Constructor for the check_event_log class.
 *
 * This constructor initializes a check_event_log object with the provided
 * parameters.
 *
 * @param io_context Shared pointer to an asio::io_context object.
 * @param logger Shared pointer to an spdlog::logger object.
 * @param first_start_expected The expected first start time point.
 * @param check_interval The interval duration for checks.
 * @param serv The service name.
 * @param cmd_name The command name.
 * @param cmd_line The command line.
 * @param args The JSON arguments for configuration.
 * @param cnf Shared pointer to the engine to agent request configuration.
 * @param handler The completion handler for the check.
 * @param stat Shared pointer to the checks statistics.
 *
 * @throws std::exception If there is an error parsing the arguments.
 */
check_event_log::check_event_log(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration check_interval,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat) {
  com::centreon::common::rapidjson_helper arg(args);
  try {
    if (args.IsObject()) {
      duration scan_range =
          duration_from_string(arg.get_string("scan-range", ""), 's', true);

      if (scan_range.count() == 0) {  // default: 24h
        scan_range = std::chrono::days(1);
      }

      _verbose = arg.get_bool("verbose", true);

      _warning_threshold = arg.get_unsigned("warning-count", 1);
      _critical_threshold = arg.get_unsigned("critical-count", 1);
      _empty_output =
          arg.get_string("empty-state", "Empty or no match for this filter");

      _output_syntax = arg.get_string("output-syntax",
                                      "${status}: ${count} ${problem_list}");
      _ok_syntax =
          arg.get_string("ok-syntax", "${status}: Event log seems fine");

      _calc_output_format();

      _calc_event_detail_syntax(
          arg.get_string("event-detail-syntax", "'${source} ${id}'"));

      std::string_view uniq =
          arg.get_string("unique-index", "${provider}${id}");
      if (!uniq.empty()) {
        _event_compare =
            std::make_unique<event_log::event_comparator>(uniq, logger);
      }

      bool need_message_content = _event_detail_syntax.find("{3}") !=
                                  std::string::npos;  // message content
      _data = std::make_unique<event_log::event_container>(
          arg.get_string("file"),
          arg.get_string(
              "filter-event",
              "written > 60m and level in ('error', 'warning', 'critical')"),
          arg.get_string("warning-status", "level = 'warning'"),
          arg.get_string("critical-status", "level in ('error', 'critical')"),
          scan_range, need_message_content, logger);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_event_log, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

/**
 * @brief Static method to load a check_event_log instance.
 *
 * This method creates and initializes a shared pointer to a check_event_log
 * object with the provided parameters, and starts the event log data
 * processing.
 *
 * @param io_context Shared pointer to an asio::io_context object.
 * @param logger Shared pointer to an spdlog::logger object.
 * @param first_start_expected The expected first start time point.
 * @param check_interval The interval duration for checks.
 * @param serv The service name.
 * @param cmd_name The command name.
 * @param cmd_line The command line.
 * @param args The JSON arguments for configuration.
 * @param cnf Shared pointer to the engine to agent request configuration.
 * @param handler The completion handler for the check.
 * @param stat Shared pointer to the checks statistics.
 *
 * @return A shared pointer to the initialized check_event_log object.
 */
std::shared_ptr<check_event_log> check_event_log::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration check_interval,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat) {
  std::shared_ptr<check_event_log> ret = std::make_shared<check_event_log>(
      io_context, logger, first_start_expected, check_interval, serv, cmd_name,
      cmd_line, args, cnf, std::move(handler), stat);

  ret->_data->start();

  return ret;
}

static const boost::container::flat_map<std::string_view, std::string_view>
    _label_to_event_index = {{"${file}", "{0}"},
                             {"${id}", "{1}"},
                             {"${source}", "{2}"},
                             {"${log}", "{2}"},
                             {"${provider}", "{2}"},
                             {"${message}", "{3}"},
                             {"${status}", "{4}"},
                             {"${written}", "{5}"},
                             {"${record_id}", "{6}"},
                             {"${record-id}", "{6}"},
                             {"${computer}", "{7}"},
                             {"${channel}", "{8}"},
                             {"${keywords}", "{9}"},
                             {"${level}", "{10}"},
                             {"${written_str}", "{11}"},
                             {"{file}", "{0}"},
                             {"{id}", "{1}"},
                             {"{source}", "{2}"},
                             {"{log}", "{2}"},
                             {"{provider}", "{2}"},
                             {"{message}", "{3}"},
                             {"{status}", "{4}"},
                             {"{written}", "{5}"},
                             {"{record_id}", "{6}"},
                             {"{record-id}", "{6}"},
                             {"{computer}", "{7}"},
                             {"{channel}", "{8}"},
                             {"{keywords}", "{9}"},
                             {"{level}", "{10}"},
                             {"{written_str}", "{11}"}};

void check_event_log::_calc_event_detail_syntax(const std::string_view& param) {
  _event_detail_syntax = param;
  for (const auto& translate : _label_to_event_index) {
    boost::replace_all(_event_detail_syntax, translate.first, translate.second);
  }
}

static const boost::container::flat_map<std::string_view, std::string_view>
    _label_to_output_index = {
        {"${status}", "{0}"},       {"${count}", "{1}"},
        {"${problem_list}", "{2}"}, {"${problem-list}", "{2}"},
        {"{status}", "{0}"},        {"{count}", "{1}"},
        {"{problem_list}", "{2}"},  {"{problem-list}", "{2}"}};

/**
 * @brief Calculate the output format by replacing labels with their
 * corresponding indices.
 *
 * This method updates the output format strings (_empty_output, _output_syntax,
 * _ok_syntax) by replacing the labels (e.g., ${status}, ${count},
 * ${problem_list}) with their corresponding indices defined in
 * _label_to_output_index.
 */
void check_event_log::_calc_output_format() {
  auto replace_label = [](std::string* to_maj) {
    for (const auto& translate : _label_to_output_index) {
      boost::replace_all(*to_maj, translate.first, translate.second);
    }
  };

  replace_label(&_empty_output);
  replace_label(&_output_syntax);
  replace_label(&_ok_syntax);
}

/**
 * @brief get uptime with GetTickCount64
 *
 * @param timeout unused
 */
void check_event_log::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  std::string output;
  std::list<common::perfdata> perf;
  e_status status = compute(*_data, &output, &perf);

  asio::post(
      *_io_context, [me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perf)]() {
        on_completion(_get_running_check_index(), status, performance, {out});
      });
}

/**
 * @brief Print event details.
 *
 * This template function prints the details of an event based on the provided
 * parameters. It formats the event details according to the specified syntax.
 *
 * @tparam need_written_str Boolean indicating whether the formatted date time
 * string is needed.
 * @param file The file name as System.
 * @param status The status of the check.
 * @param evt The event object containing event details.
 * @return A formatted string containing the event details.
 */
template <bool need_written_str>
std::string check_event_log::print_event_detail(
    const std::string& file,
    e_status status,
    const event_log::event& evt) const {
  unsigned event_id = evt.get_event_id();
  uint64_t record_id = evt.get_record_id();
  unsigned level = evt.get_level();
  uint64_t time_s = std::chrono::duration_cast<std::chrono::seconds>(
                        evt.get_time().time_since_epoch())
                        .count();
  if constexpr (need_written_str) {
    std::string written_str = std::format("{:%FT%T}", evt.get_time());
    return std::vformat(
        _event_detail_syntax,
        std::make_format_args(file, event_id, evt.get_provider(),
                              evt.get_message(), status_label[status], time_s,
                              record_id, evt.get_computer(), evt.get_channel(),
                              evt.get_str_keywords(), level, written_str));
  } else {
    return std::vformat(
        _event_detail_syntax,
        std::make_format_args(file, event_id, evt.get_provider(),
                              evt.get_message(), status_label[status], time_s,
                              record_id, evt.get_computer(), evt.get_channel(),
                              evt.get_str_keywords(), level));
  }
}

/**
 * @brief Compute the status and output for the event log check.
 *
 * This method processes the event log data, determines the status based on the
 * number of critical and warning events, and formats the output string
 * accordingly. It also generates performance data.
 *
 * @param data Reference to the event log container.
 * @param output Pointer to the output string.
 * @param perf Pointer to the list of performance data.
 * @return The computed status of the check.
 */
e_status check_event_log::compute(
    event_log::event_container& data,
    std::string* output,
    std::list<com::centreon::common::perfdata>* perf) {
  std::lock_guard l(data);

  const std::string* out_format = &_output_syntax;

  // Clean perempted events
  data.clean_perempted_events(true);
  e_status status = e_status::ok;

  // Determine status based on thresholds
  if (data.get_nb_critical() >= _critical_threshold) {
    status = e_status::critical;
  } else if (data.get_nb_warning() >= _warning_threshold) {
    status = e_status::warning;
  } else {
    if (data.get_critical().empty() && data.get_warning().empty() &&
        !data.get_nb_ok()) {
      out_format = &_empty_output;
    } else {
      out_format = &_ok_syntax;
    }
  }

  // Determine the event format function based on the presence of written_str
  std::string (check_event_log::*event_format)(const std::string&, e_status,
                                               const event_log::event&) const;
  if (out_format->find("{11}") == std::string::npos) {  // written_str?
    event_format = &check_event_log::print_event_detail<true>;
  } else {
    event_format = &check_event_log::print_event_detail<false>;
  }

  std::string file = lpwcstr_to_acp(data.get_file().c_str());

  std::unique_ptr<
      absl::flat_hash_set<const event_log::event*, event_log::event_comparator,
                          event_log::event_comparator>>
      critical_uniq;
  std::unique_ptr<
      absl::flat_hash_set<const event_log::event*, event_log::event_comparator,
                          event_log::event_comparator>>
      warning_uniq;

  // Group events by _event_compare if needed
  if (_event_compare && _verbose ||
      out_format->find("{2}") != std::string::npos) {
    critical_uniq =
        std::make_unique<absl::flat_hash_set<const event_log::event*,
                                             event_log::event_comparator,
                                             event_log::event_comparator>>(
            0, *_event_compare, *_event_compare);
    warning_uniq =
        std::make_unique<absl::flat_hash_set<const event_log::event*,
                                             event_log::event_comparator,
                                             event_log::event_comparator>>(
            0, *_event_compare, *_event_compare);

    // Insert events into unique sets
    for (const auto& evt : data.get_critical() | std::views::reverse) {
      critical_uniq->insert(&evt.second);
    }
    for (const auto& evt : data.get_warning() | std::views::reverse) {
      warning_uniq->insert(&evt.second);
    }
  }

  struct reverse_event_compare {
    bool operator()(const event_log::event* a,
                    const event_log::event* b) const {
      return a->get_time() > b->get_time();
    }
  };

  // Reorder events by time (newest first)
  std::multiset<const event_log::event*, reverse_event_compare>
      critical_ordered, warning_ordered;
  if (critical_uniq) {
    for (const event_log::event* evt : *critical_uniq) {
      critical_ordered.insert(evt);
    }
  }
  if (warning_uniq) {
    for (const event_log::event* evt : *warning_uniq) {
      warning_ordered.insert(evt);
    }
  }

  // Calculate problem_list if needed
  try {
    std::string problem_list;
    if (!_verbose && out_format->find("{2}") != std::string::npos) {
      for (const event_log::event* evt : critical_ordered) {
        problem_list.push_back(' ');
        problem_list += (this->*event_format)(file, e_status::critical, *evt);
      }
      for (const event_log::event* evt : warning_ordered) {
        problem_list.push_back(' ');
        problem_list += (this->*event_format)(file, e_status::warning, *evt);
      }
    }

    unsigned problem_count = data.get_nb_critical() + data.get_nb_warning();
    *output = std::vformat(
        *out_format, std::make_format_args(status_label[status], problem_count,
                                           problem_list));
    if (_verbose) {
      for (const event_log::event* evt : critical_ordered) {
        output->push_back('\n');
        *output += (this->*event_format)(file, e_status::critical, *evt);
      }
      for (const event_log::event* evt : warning_ordered) {
        output->push_back('\n');
        *output += (this->*event_format)(file, e_status::warning, *evt);
      }
    }
  } catch (const std::exception& e) {
    *output =
        std::format("fail to format output string with this pattern '{}' : {}",
                    *out_format, e.what());
    return e_status::critical;
  }

  // Generate performance data
  common::perfdata critical;
  critical.name("critical-count");
  critical.value(data.get_nb_critical());
  common::perfdata warning;
  warning.name("warning-count");
  warning.value(data.get_nb_warning());
  perf->emplace_back(std::move(critical));
  perf->emplace_back(std::move(warning));

  return status;
}

void check_event_log::help(std::ostream& help_stream) {
  help_stream << R"(
- eventlog params:
    scan-range : validity of events, can be s, second, m, minute, h, hour, d, day, w, week, default: 24h
    verbose : display all events in long plugins output format (one line per event), default: true
    warning-count : number of warning events to trigger a warning, default: 1
    critical-count : number of critical events to trigger a critical, default: 1
    empty-state : message to display when no event is found, default: "Empty or no match for this filter"
    output-syntax : output format when status is not ok, default: "{status}: {count} {problem_list}"
    ok-syntax : output format when status is ok, default: "{status}: Event log seems fine"
    event-detail-syntax : output format for each event, default: "'{source} {id}'"
    unique-index : unique index for events, events are grouped by this index. 
                   For example is two events have the same provider and the same id, only latest is printed to output , default: "{provider}{id}"
    file : event log file to monitor
    filter-event : filter to apply on event log, default: "written > 60m and level in ('error', 'warning', 'critical')
    warning-status : filter to apply on event log to get warning events, default: "level = 'warning'
    critical-status : filter to apply on event log to get critical events, default: "level in ('error', 'critical')
  event filter keywords:
    - written : event written time in seconds
    - level : event level or numerical values
    - id : event id
    - source : event source
    - provider : source alias
    - keywords: auditsuccess auditfailure
    - channel: event channel
  level values:
    - critical: 1
    - error: 2
    - warning: 3
    - info: 4
    - debug: 5
  output keywords:
    - status : status of the check
    - count : number of events
    - problem_list : list of events seperated by a space
  unique-index keywords:
    - source : event source
    - provider : source alias
    - id : event id
    - message : event message (nor regex nor wildcard)
    - channel: event cahnnel
  event print keywords:
    - file : event log file
    - id : event id
    - source : event source
    - log : source alias
    - provider : source alias
    - message : event message
    - status : event status
    - written : event written time in seconds
    - record_id : event record id
    - computer : event computer
    - channel : event channel
    - keywords : event keywords (audit_success, audit_failure)
    - level : event level
    - written_str : event written time in string in ISO format
  Examples of output:
    with these params: { "file" : "System", 
      "critical-status": "level == 'error' and written > -2s", 
      "verbose": false,
      "output-syntax": "${status}: ${count} ${problem_list}",
      "event-detail-syntax": "'${file} ${source} ${log} ${provider} ${id} ${message} ${status} ${written} ${computer} ${channel} ${keywords} ${level} ${record_id} ${written_str}'"
      }
    output will be:  CRITICAL: 1  'System my_provider my_provider my_provider 12 my message CRITICAL 13386175600 my_computer my_channel audit_success|audit_failure 2 456 2025-03-11T14:06:40.7680000'
  
  Metrics:
    - critical-count : number of critical events
    - warning-count : number of warning events
)";
}