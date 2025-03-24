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

#include "check_process.hh"
#include <format>
#include <iterator>
#include <tuple>
#include "com/centreon/common/rapidjson_helper.hh"
#include "process/process_container.hh"
#include "process/process_data.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::process;

/**
 * @brief Construct a new check uptime::check uptime object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected
 * @param check_interval
 * @param serv
 * @param cmd_name
 * @param cmd_line
 * @param args
 * @param cnf
 * @param handler
 */
check_process::check_process(
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
      _verbose = arg.get_bool("verbose", true);

      _empty_output =
          arg.get_string("empty-state", "Empty or no match for this filter");

      _ok_syntax =
          arg.get_string("ok-syntax", "{status}: All processes are ok");

      _output_syntax =
          arg.get_string("output-syntax", "{status}: {problem_list}");

      _process_detail_syntax =
          arg.get_string("process-detail-syntax", "{exe}={state}");

      _calc_output_format();

      unsigned field_mask = _calc_process_detail_syntax(
          arg.get_string("process-detail-syntax", "'${exe}=${state}'"));

      std::string_view filter, exclude_filter, warning_filter, critical_filter;
      filter = arg.get_string("filter-process", "state = 'started'");
      exclude_filter = arg.get_string("exclude-filter", "");
      warning_filter = arg.get_string("warning-status", "state != 'started'");
      critical_filter = arg.get_string("critical-status", "count = 0");
      _processes = std::make_unique<process::container>(
          filter, exclude_filter, warning_filter, critical_filter, field_mask,
          logger);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_process, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

constexpr std::array<std::tuple<std::string_view, std::string_view, unsigned>,
                     30>
    _label_to_process_detail{
        {{"${exe}", "{0}", process_field::exe_filename},
         {"${exe}", "{0}", process_field::exe_filename},
         {"${filename}", "{1}", process_field::exe_filename},
         {"{filename}", "{1}", process_field::exe_filename},
         {"${status}", "{2}", 0},
         {"{status}", "{2}", 0},
         {"${state}", "{2}", 0},
         {"{state}", "{2}", 0},
         {"${creation}", "{3}", process_field::times},
         {"{creation}", "{3}", process_field::times},
         {"${kernel}", "{4}", process_field::times},
         {"{kernel}", "{4}", process_field::times},
         {"${kernel_percent}", "{5}", process_field::times},
         {"{kernel_percent}", "{5}", process_field::times},
         {"${user}", "{6}", process_field::times},
         {"{user}", "{6}", process_field::times},
         {"${user_percent}", "{7}", process_field::times},
         {"{user_percent}", "{7}", process_field::times},
         {"${time}", "{8}", process_field::times},
         {"{time}", "{8}", process_field::times},
         {"${time_percent}", "{9}", process_field::times},
         {"{time_percent}", "{9}", process_field::times},
         {"${virtual}", "{10}", process_field::memory},
         {"{virtual}", "{10}", process_field::memory},
         {"${gdi_handle}", "{11}", process_field::handle},
         {"{gdi_handle}", "{11}", process_field::handle},
         {"${user_handle}", "{12}", process_field::handle},
         {"{user_handle}", "{12}", process_field::handle},
         {"${pid}", "{13}", 0},
         {"{pid}", "{13}", 0}}};

namespace com::centreon::agent::process::detail {
struct const_formatter {
  std::string_view new_string;
  unsigned field_mask;
  unsigned* parent_field_mask;

  template <typename FindResultT>
  std::string_view operator()(const FindResultT&) const {
    *parent_field_mask |= field_mask;
    return new_string;
  }
};
}  // namespace com::centreon::agent::process::detail

unsigned check_process::_calc_process_detail_syntax(
    const std::string_view& param) {
  unsigned mask = 0;

  _process_detail_syntax = param;
  for (const auto& translate : _label_to_process_detail) {
    detail::const_formatter formatter{std::get<1>(translate),
                                      std::get<2>(translate), &mask};
    boost::algorithm::find_format_all(
        _process_detail_syntax,
        boost::algorithm::first_finder(std::get<0>(translate)), formatter);
  }

  _process_detail_syntax_contains_creation_time =
      _process_detail_syntax.find("{3}") != std::string::npos;
  return mask;
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 8>
    _label_to_output_index{{{"${status}", "{0}"},
                            {"${count}", "{1}"},
                            {"${problem_list}", "{2}"},
                            {"${problem-list}", "{2}"},
                            {"{status}", "{0}"},
                            {"{count}", "{1}"},
                            {"{problem_list}", "{2}"},
                            {"{problem-list}", "{2}"}}};

/**
 * @brief Calculate the output format by replacing labels with their
 * corresponding indices.
 *
 * This method updates the output format strings (_empty_output, _output_syntax,
 * _ok_syntax) by replacing the labels (e.g., ${status}, ${count},
 * ${problem_list}) with their corresponding indices defined in
 * _label_to_output_index.
 */
void check_process::_calc_output_format() {
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
void check_process::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  std::string output;
  common::perfdata perf;
  _processes->refresh();
  e_status status = compute(*_processes, &output, &perf);

  asio::post(
      *_io_context, [me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perf)]() {
        on_completion(_get_running_check_index(), status, {performance}, {out});
      });
}

void check_process::_print_process(const process::process_data& proc,
                                   std::string* to_append) const {
  std::string creation_time;
  if (_process_detail_syntax_contains_creation_time) {
    creation_time = proc.get_creation_time_str();
  }
  unsigned percent_kernel_time = proc.get_percent_kernel_time();
  unsigned percent_user_time = proc.get_percent_user_time();
  unsigned percent_cpu_time = proc.get_percent_cpu_time();
  auto cpu_time = proc.get_kernel_time() + proc.get_user_time();
  std::vformat_to(
      std::back_inserter(*to_append), _process_detail_syntax,
      std::make_format_args(
          proc.get_exe(), proc.get_file_name(), proc.get_str_state(),
          creation_time, proc.get_kernel_time(), percent_kernel_time,
          proc.get_user_time(), percent_user_time, cpu_time, percent_cpu_time,
          proc.get_memory_counters().PrivateUsage, proc.get_gdi_handle_count(),
          proc.get_user_handle_count(), proc.get_pid()));
}

e_status check_process::compute(process::container& cont,
                                std::string* output,
                                common::perfdata* perfs) {
  e_status ret = e_status::ok;

  std::string problem_list;

  const std::string* output_format;
  ret = cont.check_container();
  if (cont.empty()) {
    output_format = &_empty_output;
  } else if (ret == e_status::critical) {
    output_format = &_output_syntax;
  } else if (ret == e_status::warning) {
    output_format = &_output_syntax;
  } else {
    output_format = &_ok_syntax;
  }

  try {
    if (output_format->find("{2}") != std::string::npos) {
      for (const process::process_data& to_dump :
           cont.get_critical_processes()) {
        _print_process(to_dump, &problem_list);
        problem_list.push_back(' ');
      }
      for (const process::process_data& to_dump :
           cont.get_warning_processes()) {
        _print_process(to_dump, &problem_list);
        problem_list.push_back(' ');
      }
    }

    size_t problem_count = cont.get_critical_processes().size() +
                           cont.get_warning_processes().size();
    *output = std::vformat(
        *output_format,
        std::make_format_args(status_label[ret], problem_count, problem_list));

    if (_verbose) {
      for (const process::process_data& to_dump :
           cont.get_critical_processes()) {
        output->push_back('\n');
        _print_process(to_dump, output);
      }
      for (const process::process_data& to_dump :
           cont.get_warning_processes()) {
        output->push_back('\n');
        _print_process(to_dump, output);
      }
    }
  } catch (const std::exception& e) {
    *output = std::format(
        "fail to format output string with these pattern '{}' and '{}' : {}",
        *output_format, _process_detail_syntax, e.what());
    return e_status::critical;
  }

  perfs->name("process.count");
  perfs->value(cont.get_ok_processes().size() +
               cont.get_warning_processes().size() +
               cont.get_critical_processes().size());

  return ret;
}

void check_process::help(std::ostream& help_stream) {
  help_stream << R"(
- process params:
  verbose : display all process in long plugins output format (one line per process), default: true
  empty-state : message to display when no event is found, default: "Empty or no match for this filter"
  output-syntax : output format when status is not ok, default: "{status}: {problem_list}"
  ok-syntax : output format when status is ok, default: "{status}: All processes are ok"
  process-detail-syntax : output format for each event, default: "{exe}={state}"
  filter-process: first filter applied to process before applying warning and critical ones, default: "state = 'started'"
  warning-status: filter to apply on process to get warning processes. 
                You can also add count labels to put for example service in warning state if you have less than 2 processes
                default: "state != 'started'"
  criticalt-status: filter to apply on process to get critical processes. 
                You can also add count labels to put for example service in critical state if you have zero process
                default: "count = 0"
  filter keywords:
    - count: total number of processes filtered by filter-process
    - ok-count: number of processes filtered by filter-process but not filtered by warning-status nor critical-status
    - warning-count: number of processes filtered by warning-status
    - critical-count: number of processes filtered by critical-status
    - creation: delay from process start to now (units can be h, m, s, d, w)
    - pid: process pid
    - gdi_handles: number of gdi handles opened by process
    - user_handles: number of user handles opened by process
    - handles: gdi_handles + user_handles
    - kernel: amount of time that the process has executed in kernel mode
    - user: amount of time that the process has executed in user mode
    - time: kernel + user
    - kernel_percent: kernel * 100 / creation
    - user_percent: user * 100 / creation
    - time_percent: kernel_percent + user_percent
    - pagefile: Total amount of private memory that the memory manager has committed for a running process
    - peak_pagefile: The peak value in bytes of the Commit Charge during the lifetime of this process
    - peak_virtual: same as peak_pagefile
    - peak_working_set: The peak working set size, in (g, m, k, b)
    - working_set: The current working set size
  output keywords:
    - status : status of the check
    - count : number of processes (ok + critical + warning)
    - problem_list : list of no ok processes seperated by a space
  process detail print keywords:
  Examples of output:
    with these params: { "check":"process_nscp", 
        "args": { 
            "warning-status": " exe = 'RuntimeBroker.exe' && warn_count > 2"
            }
      }
    output will be: WARNING: 'RuntimeBroker.exe=started' 'RuntimeBroker.exe=started' 'RuntimeBroker.exe=started' 
  Metrics:
    - process.count: numbers of processes filtered by filter-process

)";
}