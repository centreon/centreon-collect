/**
 * Copyright 2026 Centreon
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

#include "check_network.hh"
#include <memory>
#include "spdlog/spdlog.h"
#include "windows_util.hh"

#include "com/centreon/common/utf8.hh"

// GetIfTable2Ex and FreeMibTable
#pragma comment(lib, "iphlpapi.lib")

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_network_detail;
using namespace com::centreon;

namespace {

/**
 * IF-MIB ifOperStatus names in lower case, indexed by IF_OPER_STATUS value
 */
constexpr std::array<std::string_view, 8> _status_labels = {
    "",        "up",      "down",       "testing",
    "unknown", "dormant", "notpresent", "lowerlayerdown"};

/**
 * @brief what is known of a metric: threshold arguments, perfdata label
 * (prefixed by the interface name and #), perfdata unit and output label
 */
struct metric_definition {
  std::string_view warning_arg;
  std::string_view critical_arg;
  std::string_view perf_label;
  std::string_view unit;
  std::string_view output_label;
};

/**
 * indexed by e_metric
 */
constexpr std::array<metric_definition, e_metric::nb_metric> _metrics = {{
    {"warning-traffic-in", "critical-traffic-in",
     "interface.traffic.in.bitspersecond", "b/s", "traffic in"},
    {"warning-traffic-out", "critical-traffic-out",
     "interface.traffic.out.bitspersecond", "b/s", "traffic out"},
    {"warning-traffic-in-prct", "critical-traffic-in-prct",
     "interface.traffic.in.percentage", "%", "traffic in"},
    {"warning-traffic-out-prct", "critical-traffic-out-prct",
     "interface.traffic.out.percentage", "%", "traffic out"},
    {"warning-errors-in", "critical-errors-in",
     "interface.packets.in.error.persecond", "packets/s", "errors in"},
    {"warning-errors-out", "critical-errors-out",
     "interface.packets.out.error.persecond", "packets/s", "errors out"},
    {"warning-discards-in", "critical-discards-in",
     "interface.packets.in.discard.persecond", "packets/s", "discards in"},
    {"warning-discards-out", "critical-discards-out",
     "interface.packets.out.discard.persecond", "packets/s", "discards out"},
}};

/**
 * @brief bit rate with decimal prefixes, as link speeds are expressed
 */
std::string _format_bit_rate(double bits_per_second) {
  constexpr std::array<std::string_view, 5> units = {"b/s", "Kb/s", "Mb/s",
                                                     "Gb/s", "Tb/s"};
  size_t unit_index = 0;
  while (bits_per_second >= 1000 && unit_index + 1 < units.size()) {
    bits_per_second /= 1000;
    ++unit_index;
  }
  return fmt::format("{:.2f} {}", bits_per_second, units[unit_index]);
}

std::string _format_value(unsigned metric, double value) {
  switch (metric) {
    case e_metric::traffic_in:
    case e_metric::traffic_out:
      return _format_bit_rate(value);
    case e_metric::traffic_in_prct:
    case e_metric::traffic_out_prct:
      return fmt::format("{:.2f}%", value);
    default:
      return fmt::format("{:.2f}/s", value);
  }
}

/**
 * @brief traffic with its share of the link speed when the speed is known
 */
std::string _format_traffic(const std::optional<double>& bits_per_second,
                            const std::optional<double>& percent) {
  if (percent) {
    return fmt::format("{} ({:.2f}%)", _format_bit_rate(*bits_per_second),
                       *percent);
  }
  return _format_bit_rate(*bits_per_second);
}

std::string _format_speed(uint64_t speed_in, uint64_t speed_out) {
  auto format = [](uint64_t speed) {
    return speed ? _format_bit_rate(static_cast<double>(speed))
                 : std::string("unknown");
  };
  if (speed_in == speed_out) {
    return format(speed_in);
  }
  return fmt::format("in {} out {}", format(speed_in), format(speed_out));
}

/**
 * @brief counter increase between two snapshots. A counter only decreases
 * when the interface is reset during the measure: the count since the reset
 * is then the best estimate
 */
uint64_t _delta(uint64_t first, uint64_t second) {
  return second >= first ? second - first : second;
}

/**
 * @brief link speed in bits/s, 0 when unknown
 */
uint64_t _known_speed(ULONG64 speed) {
  return speed == NET_IF_LINK_SPEED_UNKNOWN ? 0 : speed;
}

/**
 * @brief Windows fixed size strings are null terminated, the bound only
 * protects against a corrupted row
 */
template <size_t size>
std::string _to_utf8(const WCHAR (&str)[size]) {
  return common::wstring_to_utf8(std::wstring_view(str, wcsnlen(str, size)));
}

}  // namespace

/***********************************************************************************************
 *     interface_filter
 ***********************************************************************************************/

/**
 * @brief tells if an interface must be checked
 *
 * @param row interface as returned by GetIfTable2Ex
 * @param name UTF-8 interface name (Alias)
 * @param description UTF-8 adapter description
 */
bool interface_filter::is_allowed(const MIB_IF_ROW2& row,
                                  std::string_view name,
                                  std::string_view description) const {
  // a filter interface is a module bound to an adapter, it repeats the
  // counters of that adapter
  if (row.InterfaceAndOperStatusFlags.FilterInterface) {
    return false;
  }
  if (physical_only && !row.InterfaceAndOperStatusFlags.ConnectorPresent) {
    return false;
  }
  if (name_exclude && RE2::FullMatch(name, *name_exclude)) {
    return false;
  }
  if (name_filter && !RE2::FullMatch(name, *name_filter)) {
    return false;
  }
  if (description_exclude &&
      RE2::FullMatch(description, *description_exclude)) {
    return false;
  }
  if (description_filter && !RE2::FullMatch(description, *description_filter)) {
    return false;
  }
  return true;
}

/***********************************************************************************************
 *     network_snapshot
 ***********************************************************************************************/

/**
 * @brief keeps the counters of the interfaces allowed by the filter
 *
 * @param time time of the measure
 * @param rows interface table
 * @param filter interfaces to keep
 */
network_snapshot::network_snapshot(
    std::chrono::steady_clock::time_point time,
    std::span<const MIB_IF_ROW2> rows,
    const interface_filter& filter,
    const std::shared_ptr<spdlog::logger>& logger)
    : _time(time), _logger(logger) {
  for (const MIB_IF_ROW2& row : rows) {
    std::string name = _to_utf8(row.Alias);
    std::string description = _to_utf8(row.Description);
    if (!filter.is_allowed(row, name, description)) {
      SPDLOG_LOGGER_DEBUG(logger, "the value {} is filtered", name);
      continue;
    }
    interface_counters& counters = _interfaces[row.InterfaceLuid.Value];
    counters.name = std::move(name);
    counters.description = std::move(description);
    // the status is also a shift count and an index of _status_labels
    counters.status = row.OperStatus >= IfOperStatusUp &&
                              row.OperStatus <= IfOperStatusLowerLayerDown
                          ? row.OperStatus
                          : IfOperStatusUnknown;
    counters.speed_in = _known_speed(row.ReceiveLinkSpeed);
    counters.speed_out = _known_speed(row.TransmitLinkSpeed);
    counters.in_octets = row.InOctets;
    counters.out_octets = row.OutOctets;
    counters.in_errors = row.InErrors;
    counters.out_errors = row.OutErrors;
    counters.in_discards = row.InDiscards;
    counters.out_discards = row.OutDiscards;
  }
}

/***********************************************************************************************
 *     check_network
 ***********************************************************************************************/

/**
 * @brief Construct a new check network object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param serv service
 * @param args native plugin arguments
 * @param cnf engine configuration received object
 * @param handler called at measure completion
 * @param stat statistics shared by checks
 * @throw exceptions::msg_fmt on an invalid argument
 */
check_network::check_network(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    const Service& serv,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            serv,
            cnf,
            std::move(handler),
            stat),
      _measure_timer(*io_context) {
  if (!args.IsObject()) {
    return;
  }

  for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
       ++member_iter) {
    std::string key = absl::AsciiStrToLower(member_iter->name.GetString());
    const rapidjson::Value& val = member_iter->value;
    if (key == "filter-name") {
      _filter.name_filter = _compile_regex(key, val);
    } else if (key == "exclude-name") {
      _filter.name_exclude = _compile_regex(key, val);
    } else if (key == "filter-description") {
      _filter.description_filter = _compile_regex(key, val);
    } else if (key == "exclude-description") {
      _filter.description_exclude = _compile_regex(key, val);
    } else if (key == "physical-only") {
      std::optional<bool> physical_only =
          get_bool(get_command_name(), key.c_str(), val);
      if (physical_only) {
        _filter.physical_only = *physical_only;
      }
    } else if (key == "warning-state") {
      _status_to_warning = _parse_states(key, val);
    } else if (key == "critical-state") {
      _status_to_critical = _parse_states(key, val);
    } else if (key != "timeout" && !_parse_threshold(key, val)) {
      SPDLOG_LOGGER_WARN(_logger, "command: {}, unknown parameter: {}",
                         get_command_name(), member_iter->name.GetString());
    }
  }
}

/**
 * @brief compile a case insensitive regex
 *
 * @param arg_name argument name, used in error message
 * @param val argument value
 * @return a null pointer if the value is empty (unset macro)
 * @throw exceptions::msg_fmt if the value is not a valid regex
 */
std::unique_ptr<re2::RE2> check_network::_compile_regex(
    const std::string& arg_name,
    const rapidjson::Value& val) const {
  std::optional<std::string> pattern =
      get_string(get_command_name(), arg_name.c_str(), val);
  if (!pattern) {
    return {};
  }
  // RE2 is UTF-8 aware, so É matches é
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  options.set_log_errors(false);
  auto regex = std::make_unique<re2::RE2>(*pattern, options);
  if (!regex->ok()) {
    throw exceptions::msg_fmt("command: {}, {}: {} is not a valid regex: {}",
                              get_command_name(), arg_name, *pattern,
                              regex->error());
  }
  return regex;
}

/**
 * @brief convert a regex of operational states to a mask of 1 << state
 *
 * @param arg_name argument name, used in error message
 * @param val argument value
 * @return 0 if the value is empty
 * @throw exceptions::msg_fmt if the regex is invalid or matches no state
 */
unsigned check_network::_parse_states(const std::string& arg_name,
                                      const rapidjson::Value& val) const {
  std::unique_ptr<re2::RE2> regex = _compile_regex(arg_name, val);
  if (!regex) {
    return 0;
  }
  unsigned mask = 0;
  for (unsigned status = IfOperStatusUp; status < _status_labels.size();
       ++status) {
    if (RE2::FullMatch(_status_labels[status], *regex)) {
      mask |= 1u << status;
    }
  }
  // a typo would silently disable the threshold
  if (!mask) {
    throw exceptions::msg_fmt(
        "command: {}, {}: {} matches no state among up, down, testing, "
        "unknown, dormant, notpresent, lowerlayerdown",
        get_command_name(), arg_name, regex->pattern());
  }
  return mask;
}

/**
 * @brief parse a warning-<metric> or critical-<metric> threshold
 *
 * @param arg_name argument name in lower case
 * @param val argument value
 * @return false if arg_name is not a threshold argument
 * @throw exceptions::msg_fmt if the threshold is invalid
 */
bool check_network::_parse_threshold(const std::string& arg_name,
                                     const rapidjson::Value& val) {
  for (unsigned metric = 0; metric < e_metric::nb_metric; ++metric) {
    bool warning = arg_name == _metrics[metric].warning_arg;
    if (!warning && arg_name != _metrics[metric].critical_arg) {
      continue;
    }
    std::optional<std::string> thr_str =
        get_string(get_command_name(), arg_name.c_str(), val);
    if (thr_str) {
      common::threshold thr(*thr_str);
      if (!thr.is_valid()) {
        throw exceptions::msg_fmt("command: {}, {}: invalid threshold: {}",
                                  get_command_name(), arg_name, *thr_str);
      }
      (warning ? _warning : _critical)[metric] = std::move(thr);
    }
    return true;
  }
  return false;
}

/**
 * @brief read the interface table of the host
 *
 * @return snapshot of the interfaces allowed by the filter
 */
std::unique_ptr<network_snapshot> check_network::measure() {
  struct table_deleter {
    void operator()(MIB_IF_TABLE2* table) const { FreeMibTable(table); }
  };

  PMIB_IF_TABLE2 table = nullptr;
  DWORD res = GetIfTable2Ex(MibIfTableNormal, &table);
  if (res != NO_ERROR) {
    throw exceptions::msg_fmt("fail to read the interface table: {}",
                              error_as_string(res));
  }
  std::unique_ptr<MIB_IF_TABLE2, table_deleter> table_owner(table);
  return std::make_unique<network_snapshot>(
      std::chrono::steady_clock::now(),
      std::span<const MIB_IF_ROW2>(table->Table, table->NumEntries), _filter,
      _logger);
}

/**
 * @brief take a first snapshot and arm the timer of the second one
 * The measure lasts until min(now + timeout, next check start) minus one
 * second, so that the result comes before the timeout
 *
 * @param timeout check timeout, replaced by the custom timeout when set
 */
void check_network::start_check(const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  const unsigned start_check_index = _get_running_check_index();

  try {
    std::unique_ptr<network_snapshot> first_measure = measure();

    const duration effective_timeout = get_custom_timeout().value_or(timeout);
    time_point now = std::chrono::system_clock::now();
    time_point end_measure = now + effective_timeout;
    time_step next = get_raw_start_expected();
    next.increment_to_after_min(now);
    if (next.value() < end_measure) {
      end_measure = next.value();
    }
    if (end_measure - now > std::chrono::seconds(1)) {
      end_measure -= std::chrono::seconds(1);
    }

    _measure_timer.expires_at(end_measure);
    _measure_timer.async_wait([me = shared_from_this(), start_check_index,
                               first = std::move(first_measure)](
                                  const boost::system::error_code& err) {
      me->_measure_timer_handler(err, start_check_index, *first);
    });
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "service {}, command {}: {}", get_service(),
                        get_command_name(), e.what());
    asio::post(*_io_context, [me = shared_from_this(), start_check_index,
                              err = std::string(e.what())]() {
      me->on_completion(
          start_check_index, e_status::unknown, {},
          {fmt::format("{}: {}", status_label[e_status::unknown], err)});
    });
  }
}

/**
 * @brief take the second snapshot and complete the check
 *
 * @param err asio error, set when the timer is cancelled
 * @param start_check_index running index captured at check start
 * @param first_measure snapshot taken at check start
 */
void check_network::_measure_timer_handler(
    const boost::system::error_code& err,
    unsigned start_check_index,
    const network_snapshot& first_measure) {
  if (err) {
    return;
  }

  std::string output;
  std::list<common::perfdata> perfs;
  e_status status;
  try {
    std::unique_ptr<network_snapshot> second_measure = measure();
    status = compute(first_measure, *second_measure, &output, &perfs);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "service {}, command {}: {}", get_service(),
                        get_command_name(), e.what());
    status = e_status::unknown;
    output = fmt::format("{}: {}", status_label[status], e.what());
    perfs.clear();
  }
  on_completion(start_check_index, status, perfs, {output});
}

/**
 * @brief the timeout has completed the check, the measure is useless
 */
void check_network::_on_timeout() {
  _measure_timer.cancel();
}

/**
 * @brief compute rates, statuses, output and perfdata of the interfaces
 * present in both snapshots
 *
 * @param first_measure snapshot taken at the start of the measure
 * @param second_measure snapshot taken at the end of the measure
 * @param output plugin output
 * @param perfs perfdata
 * @return e_status worst interface status, unknown if no interface
 */
e_status check_network::compute(const network_snapshot& first_measure,
                                const network_snapshot& second_measure,
                                std::string* output,
                                std::list<common::perfdata>* perfs) const {
  const double elapsed =
      std::chrono::duration<double>(second_measure.get_time() -
                                    first_measure.get_time())
          .count();
  if (elapsed <= 0) {
    *output = fmt::format("{}: no time elapsed between the two measures",
                          status_label[e_status::unknown]);
    return e_status::unknown;
  }

  struct interface_result {
    const interface_counters* counters;
    std::array<std::optional<double>, e_metric::nb_metric> values;
    e_status status = e_status::ok;
    std::vector<std::string> critical_reasons;
    std::vector<std::string> warning_reasons;
  };

  auto rate = [elapsed](uint64_t first, uint64_t second) {
    return static_cast<double>(_delta(first, second)) / elapsed;
  };

  std::vector<interface_result> results;
  results.reserve(second_measure.get_interfaces().size());
  for (const auto& [luid, current] : second_measure.get_interfaces()) {
    auto previous = first_measure.get_interfaces().find(luid);
    // an interface that appeared during the measure has no rate yet
    if (previous == first_measure.get_interfaces().end()) {
      continue;
    }
    const interface_counters& before = previous->second;
    interface_result& res = results.emplace_back();
    res.counters = &current;

    auto& values = res.values;
    values[e_metric::traffic_in] =
        rate(before.in_octets, current.in_octets) * 8;
    values[e_metric::traffic_out] =
        rate(before.out_octets, current.out_octets) * 8;
    if (current.speed_in) {
      values[e_metric::traffic_in_prct] = *values[e_metric::traffic_in] * 100 /
                                          static_cast<double>(current.speed_in);
    }
    if (current.speed_out) {
      values[e_metric::traffic_out_prct] =
          *values[e_metric::traffic_out] * 100 /
          static_cast<double>(current.speed_out);
    }
    values[e_metric::errors_in] = rate(before.in_errors, current.in_errors);
    values[e_metric::errors_out] = rate(before.out_errors, current.out_errors);
    values[e_metric::discards_in] =
        rate(before.in_discards, current.in_discards);
    values[e_metric::discards_out] =
        rate(before.out_discards, current.out_discards);

    const unsigned status_flag = 1u << current.status;
    if (status_flag & _status_to_critical) {
      res.status = e_status::critical;
      res.critical_reasons.push_back(
          fmt::format("is {}", _status_labels[current.status]));
    } else if (status_flag & _status_to_warning) {
      res.status = e_status::warning;
      res.warning_reasons.push_back(
          fmt::format("is {}", _status_labels[current.status]));
    }

    for (unsigned metric = 0; metric < e_metric::nb_metric; ++metric) {
      if (!values[metric]) {
        continue;
      }
      const double value = *values[metric];
      if (_critical[metric] && _critical[metric]->is_triggered(value)) {
        res.status = e_status::critical;
        res.critical_reasons.push_back(
            fmt::format("{} is {}", _metrics[metric].output_label,
                        _format_value(metric, value)));
      } else if (_warning[metric] && _warning[metric]->is_triggered(value)) {
        if (res.status < e_status::warning) {
          res.status = e_status::warning;
        }
        res.warning_reasons.push_back(
            fmt::format("{} is {}", _metrics[metric].output_label,
                        _format_value(metric, value)));
      }
    }
  }

  if (results.empty()) {
    *output = fmt::format("{}: no network interface matches the filters",
                          status_label[e_status::unknown]);
    return e_status::unknown;
  }

  std::sort(results.begin(), results.end(),
            [](const interface_result& left, const interface_result& right) {
              return left.counters->name < right.counters->name;
            });

  e_status worst = e_status::ok;
  std::vector<std::string> critical_problems;
  std::vector<std::string> warning_problems;
  std::array<unsigned, _status_labels.size()> count_by_status{};
  for (const interface_result& res : results) {
    worst = std::max(worst, res.status);
    ++count_by_status[res.counters->status];
    if (!res.critical_reasons.empty()) {
      critical_problems.push_back(
          fmt::format("'{}' {}", res.counters->name,
                      boost::algorithm::join(res.critical_reasons, ", ")));
    }
    if (!res.warning_reasons.empty()) {
      warning_problems.push_back(
          fmt::format("'{}' {}", res.counters->name,
                      boost::algorithm::join(res.warning_reasons, ", ")));
    }
  }

  // first line: the problems, critical ones first, or a summary when all is
  // ok
  *output = status_label[worst];
  output->append(": ");
  if (worst == e_status::ok) {
    fmt::format_to(std::back_inserter(*output),
                   "{} interface{}:", results.size(),
                   results.size() > 1 ? "s" : "");
    bool first = true;
    for (size_t status = IfOperStatusUp; status < count_by_status.size();
         ++status) {
      if (count_by_status[status]) {
        output->append(first ? " " : ", ");
        first = false;
        fmt::format_to(std::back_inserter(*output), "{} {}",
                       count_by_status[status], _status_labels[status]);
      }
    }
  } else {
    output->append(boost::algorithm::join(critical_problems, ", "));
    if (!warning_problems.empty()) {
      if (!critical_problems.empty()) {
        output->push_back(' ');
        output->append(status_label[e_status::warning]);
        output->append(": ");
      }
      output->append(boost::algorithm::join(warning_problems, ", "));
    }
  }

  // following lines: one per interface
  for (const interface_result& res : results) {
    const interface_counters& itf = *res.counters;
    const auto& values = res.values;
    fmt::format_to(
        std::back_inserter(*output),
        "\n'{}' {}, speed {}, traffic in {}, out {}, errors in {}, out {}, "
        "discards in {}, out {}",
        itf.name, _status_labels[itf.status],
        _format_speed(itf.speed_in, itf.speed_out),
        _format_traffic(values[e_metric::traffic_in],
                        values[e_metric::traffic_in_prct]),
        _format_traffic(values[e_metric::traffic_out],
                        values[e_metric::traffic_out_prct]),
        _format_value(e_metric::errors_in, *values[e_metric::errors_in]),
        _format_value(e_metric::errors_out, *values[e_metric::errors_out]),
        _format_value(e_metric::discards_in, *values[e_metric::discards_in]),
        _format_value(e_metric::discards_out, *values[e_metric::discards_out]));
  }

  for (const interface_result& res : results) {
    for (unsigned metric = 0; metric < e_metric::nb_metric; ++metric) {
      if (!res.values[metric]) {
        continue;
      }
      const metric_definition& definition = _metrics[metric];
      common::perfdata& perf = perfs->emplace_back();
      perf.name(
          fmt::format("{}#{}", res.counters->name, definition.perf_label));
      perf.unit(std::string(definition.unit));
      perf.value(static_cast<float>(*res.values[metric]));
      perf.min(0);
      if (metric == e_metric::traffic_in_prct ||
          metric == e_metric::traffic_out_prct) {
        perf.max(100);
      } else if (metric == e_metric::traffic_in && res.counters->speed_in) {
        perf.max(static_cast<float>(res.counters->speed_in));
      } else if (metric == e_metric::traffic_out && res.counters->speed_out) {
        perf.max(static_cast<float>(res.counters->speed_out));
      }
      if (_warning[metric]) {
        _warning[metric]->set_pref_details_w(perf);
      }
      if (_critical[metric]) {
        _critical[metric]->set_pref_details_c(perf);
      }
    }
  }

  return worst;
}

/**
 * @brief help printed by --help
 */
void check_network::help(std::ostream& help_stream) {
  help_stream << R"(
- network params:
    Traffic, errors and discards are averaged between two measures of the
    interface counters. The second measure is taken one second before the check
    timeout, and never after the next check start.
    filter-name: regex that the whole interface name (Get-NetAdapter Name:
      Ethernet, Wi-Fi...) must match, case insensitive
    exclude-name: regex of interface names to exclude
    filter-description: regex that the whole adapter description
      (Get-NetAdapter InterfaceDescription) must match, case insensitive
    exclude-description: regex of adapter descriptions to exclude
    physical-only: true (default): only the interfaces of network adapters,
      physical or exposed by a hypervisor (Get-NetAdapter -Physical)
      false: all interfaces, also virtual ones (Hyper-V vEthernet, VPN,
      loopback, tunnels...)
    Empty filters are ignored.
    warning-state: regex of the operational states that trigger a WARNING
    critical-state: regex of the operational states that trigger a CRITICAL
      states are: up, down, testing, unknown, dormant, notpresent,
      lowerlayerdown
    warning-traffic-in, critical-traffic-in: threshold on inbound traffic in
      bits/s
    warning-traffic-out, critical-traffic-out: threshold on outbound traffic
      in bits/s
    warning-traffic-in-prct, critical-traffic-in-prct: threshold on inbound
      traffic in percent of the link speed, ignored when the speed is unknown
    warning-traffic-out-prct, critical-traffic-out-prct: threshold on
      outbound traffic in percent of the link speed
    warning-errors-in, critical-errors-in: threshold on inbound packets in
      error per second
    warning-errors-out, critical-errors-out: threshold on outbound packets in
      error per second
    warning-discards-in, critical-discards-in: threshold on inbound packets
      discarded per second
    warning-discards-out, critical-discards-out: threshold on outbound
      packets discarded per second
    Thresholds use the Nagios range syntax. No threshold is set by default.
  An example of configuration:
  {
    "check": "network",
    "args": {
      "filter-name": "Ethernet.*",
      "critical-state": "down",
      "warning-traffic-in-prct": "80",
      "critical-traffic-in-prct": "90",
      "warning-errors-in": "1"
    }
  }
  Examples of output:
    OK: 2 interfaces: 1 up, 1 down
    'Ethernet' up, speed 1.00 Gb/s, traffic in 1.25 Mb/s (0.13%), out 340.20 Kb/s (0.03%), errors in 0.00/s, out 0.00/s, discards in 0.00/s, out 0.00/s
    'Ethernet 2' down, speed unknown, traffic in 0.00 b/s, out 0.00 b/s, errors in 0.00/s, out 0.00/s, discards in 0.00/s, out 0.00/s
    In case of problems:
      CRITICAL: 'Ethernet 2' is down WARNING: 'Ethernet' traffic in is 85.00%
    In case of a too restricted filter:
      UNKNOWN: no network interface matches the filters
  Metrics, per interface:
    <interface>#interface.traffic.in.bitspersecond
    <interface>#interface.traffic.out.bitspersecond
    <interface>#interface.traffic.in.percentage (when the link speed is known)
    <interface>#interface.traffic.out.percentage (when the link speed is known)
    <interface>#interface.packets.in.error.persecond
    <interface>#interface.packets.out.error.persecond
    <interface>#interface.packets.in.discard.persecond
    <interface>#interface.packets.out.discard.persecond
)";
}
