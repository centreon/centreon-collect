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
            stat),
      _need_message_content(false) {
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
          arg.get_string("empty_state", "Empty or no match for this filter");

      _output_syntax = arg.get_string("output-syntax",
                                      "${status}: ${count} ${problem_list}");
      _ok_syntax =
          arg.get_string("ok-syntax", "${status}: Event log seems fine");

      _calc_output_format();

      _calc_event_detail_syntax(arg.get_string("event-detail-syntax", ""));

      _data = std::make_unique<event_log::event_container>(
          arg.get_string("file"),
          arg.get_string("unique-index", "${provider}${id}"),
          arg.get_string(
              "filter-event",
              "written > 60m and level in ('error', 'warning', 'critical')"),
          arg.get_string("warning-status", "level = 'warning'"),
          arg.get_string("critical-status", "level in ('error', 'critical')"),
          scan_range, _need_message_content, logger);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_event_log, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

static const boost::container::flat_map<std::string_view, std::string_view>
    _label_to_event_index = {{"${file}", "{0}"},       {"${id}", "{1}"},
                             {"${source}", "{2}"},     {"${log}", "{2}"},
                             {"${provider}", "{2}"},   {"${message}", "{3}"},
                             {"${status}", "{4}"},     {"${written}", "{5}"},
                             {"${written_str}", "{6}"}};

void check_event_log::_calc_event_detail_syntax(const std::string_view& param) {
  _event_detail_syntax = param;
  for (const auto& translate : _label_to_event_index) {
    boost::replace_all(_event_detail_syntax, translate.first, translate.second);
  }
}

static const boost::container::flat_map<std::string_view, std::string_view>
    _label_to_output_index = {{"${status}", "{0}"},
                              {"${count}", "{1}"},
                              {"${problem_list}", "{2}"}};

void check_event_log::_calc_output_format() {
  auto replace_label = [](std::string* to_maj) {
    for (const auto& translate : _label_to_output_index) {
      boost::replace_all(*to_maj, translate.first, translate.second);
    }
  };

  replace_label(&_empty_output);
  replace_label(&_output_syntax);
  replace_label(&_output_syntax);
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

template <bool need_written_str>
std::string check_event_log::print_event_detail(
    const std::string& file,
    const event_log::event& evt) const {
  unsigned event_id = evt.event_id();
  uint64_t time_s = std::chrono::duration_cast<std::chrono::seconds>(
                        evt.time().time_since_epoch())
                        .count();
  if constexpr (need_written_str) {
    std::string written_str = std::format("%FT%T", evt.time());
    return std::vformat(
        _event_detail_syntax,
        std::make_format_args(file, event_id, evt.provider(), evt.message(),
                              status_label[evt.status()], time_s, written_str));
  } else {
    return std::vformat(
        _event_detail_syntax,
        std::make_format_args(file, event_id, evt.provider(), evt.message(),
                              status_label[evt.status()], time_s));
  }
}

e_status check_event_log::compute(
    event_log::event_container& data,
    std::string* output,
    std::list<com::centreon::common::perfdata>* perf) {
  std::lock_guard l(data);

  const std::string* out_format = &_output_syntax;

  data.clean_perempted_events();
  e_status status = e_status::ok;
  if (data.get_critical().size() > _critical_threshold) {
    status = e_status::critical;
  } else if (data.get_warning().size() > _warning_threshold) {
    status = e_status::warning;
  } else {
    if (data.get_critical().empty() && data.get_warning().empty() &&
        !data.get_nb_ok()) {
      out_format = &_empty_output;
    } else {
      out_format = &_ok_syntax;
    }
  }

  std::string (check_event_log::*event_format)(const std::string&,
                                               const event_log::event&) const;
  if (out_format->find("{6}") == std::string::npos) {  // written_str?
    event_format = &check_event_log::print_event_detail<false>;
  } else {
    event_format = &check_event_log::print_event_detail<true>;
  }

  std::string file = lpwcstr_to_acp(data.get_file().c_str());
  // need to calculate problem_list
  std::string problem_list;
  const auto& critical_ind = data.get_critical().get<1>();
  const auto& warning_ind = data.get_warning().get<1>();
  if (!_verbose && out_format->find("{2}") != std::string::npos) {
    // newest first
    for (const auto& evt : critical_ind | std::views::reverse) {
      problem_list.push_back(' ');
      problem_list += (this->*event_format)(file, evt.get_event());
    }
    for (const auto& evt : warning_ind | std::views::reverse) {
      problem_list.push_back(' ');
      problem_list += (this->*event_format)(file, evt.get_event());
    }
  }

  unsigned problem_count =
      data.get_critical().size() + data.get_warning().size();
  *output = std::vformat(
      *out_format, std::make_format_args(status_label[status], problem_list));
  if (_verbose) {
    // newest first
    for (const auto& evt : critical_ind | std::views::reverse) {
      output->push_back('\n');
      *output += (this->*event_format)(file, evt.get_event());
    }
    for (const auto& evt : warning_ind | std::views::reverse) {
      output->push_back('\n');
      *output += (this->*event_format)(file, evt.get_event());
    }
  }

  common::perfdata critical;
  critical.name("critical-count");
  critical.value(data.get_nb_critical());
  common::perfdata warning;
  critical.name("warning-count");
  critical.value(data.get_nb_warning());
  perf->emplace_back(std::move(critical));
  perf->emplace_back(std::move(warning));

  return e_status::ok;
}

void check_event_log::help(std::ostream& help_stream) {
  help_stream << R"(
- eventlog params:
)";
}