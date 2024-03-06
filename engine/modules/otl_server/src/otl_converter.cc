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

#include "com/centreon/exceptions/msg_fmt.hh"

#include "data_point_fifo_container.hh"
#include "otl_converter.hh"

#include "absl/flags/commandlineflag.h"
#include "absl/strings/str_split.h"

using namespace com::centreon::engine::modules::otl_server;

/**
 * @brief create a otl_converter_config from a command line
 * first field identify type of config
 * Example:
 * @code {.c++}
 * std::shared_ptr<otl_converter_config> conf =
 * otl_converter_config::load("nagios_telegraf");
 * @endcode
 *
 * @param cmd_line
 * @return std::shared_ptr<otl_converter_config>
 * @throw if cmd_line can't be parsed
 */
/******************************************************************
 * otl_converter base
 ******************************************************************/
otl_converter::otl_converter(const std::string& cmd_line,
                             uint64_t command_id,
                             const host& host,
                             const service* service,
                             std::chrono::system_clock::time_point timeout,
                             commands::otel::result_callback&& handler)
    : _cmd_line(cmd_line),
      _command_id(command_id),
      _host_serv{host.name(), service ? service->description() : ""},
      _timeout(timeout),
      _callback(handler) {}

bool otl_converter::sync_build_result_from_metrics(
    data_point_container& data_pts,
    commands::result& res) {
  std::lock_guard l(data_pts);
  auto& fifos = data_pts.get_fifos(_host_serv.first, _host_serv.second);
  if (!fifos.empty() && _build_result_from_metrics(fifos, res)) {
    return true;
  }
  // no data available
  return false;
}

/**
 * @brief called  when data is received from otel
 * clients
 *
 * @param data_pts
 * @return true otl_converter has managed to create check result
 * @return false
 */
bool otl_converter::async_build_result_from_metrics(
    data_point_container& data_pts) {
  commands::result res;
  bool success = false;
  {
    std::lock_guard l(data_pts);
    auto& fifos = data_pts.get_fifos(_host_serv.first, _host_serv.second);
    success = !fifos.empty() && _build_result_from_metrics(fifos, res);
  }
  if (success) {
    _callback(res);
  }
  return success;
}

/**
 * @brief called  when no data is received before
 * _timeout
 *
 */
void otl_converter::async_time_out() {
  commands::result res;
  res.exit_status = process::timeout;
  res.command_id = _command_id;
  _callback(res);
}

/**
 * @brief create a otl_converter_config from a command line
 * first field identify type of config
 * Example:
 * @code {.c++}
 * std::shared_ptr<otl_converter> converter =
 * otl_converter::create("nagios_telegraf --fifo_depth=5", 5, *host, serv,
 * timeout_point, [](const commads::result &res){});
 * @endcode
 *
 * @param cmd_line
 * @param command_id
 * @param host
 * @param service
 * @param timeout
 * @param handler
 * @return std::shared_ptr<otl_converter>
 */
std::shared_ptr<otl_converter> otl_converter::create(
    const std::string& cmd_line,
    uint64_t command_id,
    const host& host,
    const service* service,
    std::chrono::system_clock::time_point timeout,
    commands::otel::result_callback&& handler) {
  // type of the converter is the first field
  size_t sep_pos = cmd_line.find(' ');
  std::string conf_type =
      sep_pos == std::string::npos ? cmd_line : cmd_line.substr(0, sep_pos);
  boost::trim(conf_type);
  if (conf_type == "nagios_telegraf") {
    return std::make_shared<otl_nagios_telegraf_converter>(
        cmd_line, command_id, host, service, timeout, std::move(handler));
  } else {
    SPDLOG_LOGGER_ERROR(log_v2::otl(), "unknown converter type:{}", conf_type);
    throw exceptions::msg_fmt("unknown converter type:{}", conf_type);
  }
}

void otl_converter::dump(std::string& output) const {
  output = fmt::format(
      "host:{}, service:{}, command_id={}, timeout:{} cmdline: \"{}\"",
      _host_serv.first, _host_serv.second, _command_id, _timeout, _cmd_line);
}

constexpr std::array<std::string_view, 4> state_str{"OK", "WARNING", "CRITICAL",
                                                    "UNKNOWN"};

/******************************************************************
 *
 * nagios telegraf converter
 * inspired by
https://github.com/influxdata/telegraf/blob/master/plugins/parsers/nagios/parser.go
 * informations at https://nagios-plugins.org/doc/guidelines.html#AEN200
 * example of received data:
 * @code {.json}
    {
    "name": "check_icmp_critical_lt",
    "gauge": {
        "dataPoints": [
            {
                "timeUnixNano": "1707744430000000000",
                "asDouble": 0,
                "attributes": [
                    {
                        "key": "unit",
                        "value": {
                            "stringValue": "ms"
                        }
                    },
                    {
                        "key": "host",
                        "value": {
                            "stringValue": "localhost"
                        }
                    },
                    {
                        "key": "perfdata",
                        "value": {
                            "stringValue": "rta"
                        }
                    },
                    {
                        "key": "service",
                        "value": {
                            "stringValue": "check_icmp"
                        }
                    }
                ]
            }
        }
    }
 * @endcode
 *
*******************************************************************/

namespace com::centreon::engine::modules::otl_server::detail {

struct perf_data {
  std::string unit;
  std::optional<double> val;
  std::optional<double> warning_le, warning_lt, warning_ge, warning_gt;
  std::optional<double> critical_le, critical_lt, critical_ge, critical_gt;
  std::optional<double> min, max;

  bool fill_from_suffix(const std::string_view& suffix, double value);

  static const absl::flat_hash_map<std::string_view,
                                   std::optional<double> perf_data::*>
      _suffix_to_value;
};

const absl::flat_hash_map<std::string_view, std::optional<double> perf_data::*>
    perf_data::_suffix_to_value = {{"warning_le", &perf_data::warning_le},
                                   {"warning_lt", &perf_data::warning_lt},
                                   {"warning_ge", &perf_data::warning_ge},
                                   {"warning_gt", &perf_data::warning_gt},
                                   {"critical_le", &perf_data::critical_le},
                                   {"critical_lt", &perf_data::critical_lt},
                                   {"critical_ge", &perf_data::critical_ge},
                                   {"critical_gt", &perf_data::critical_gt},
                                   {"min", &perf_data::min},
                                   {"max", &perf_data::max},
                                   {"value", &perf_data::val}};

/**
 * @brief fill field identified by suffix
 *
 * @param suffix
 * @param value
 * @return true suffix is known
 * @return false suffix is unknown and no value is set
 */
bool perf_data::fill_from_suffix(const std::string_view& suffix, double value) {
  auto search = _suffix_to_value.find(suffix);
  if (search != _suffix_to_value.end()) {
    this->*search->second = value;
    return true;
  }
  SPDLOG_LOGGER_WARN(log_v2::otl(), "unknown suffix {}", suffix);
  return false;
}

}  // namespace com::centreon::engine::modules::otl_server::detail

/**
 * @brief metric name in nagios telegraf are like check_icmp_min,
 * check_icmp_critical_lt, check_icmp_value we extract all after check_icmp
 * @param metric_name
 * @return std::string_view suffix like min or critical_lt
 */
static std::string_view get_nagios_telegraf_suffix(
    const std::string_view metric_name) {
  std::size_t sep_pos = metric_name.rfind('_');
  if (sep_pos == std::string::npos) {
    return "";
  }
  std::string_view last_word = metric_name.substr(sep_pos + 1);
  if (last_word == "lt" || last_word == "gt" || last_word == "le" ||
      last_word == "ge" && sep_pos > 0) {  // critical_lt or warning_le
    sep_pos = metric_name.rfind('_', sep_pos - 1);
    if (sep_pos != std::string_view::npos) {
      return metric_name.substr(sep_pos + 1);
    }
  }
  return last_word;
}

/**
 * @brief
 *
 * @param fifos fifos indexed by metric_name such as check_icmp_critical_gt,
 * check_icmp_state
 * @return com::centreon::engine::commands::result
 */
bool otl_nagios_telegraf_converter::_build_result_from_metrics(
    metric_name_to_fifo& fifos,
    commands::result& res) {
  // first we search last state timestamp
  uint64_t last_time = 0;

  for (auto& metric_to_fifo : fifos) {
    if (get_nagios_telegraf_suffix(metric_to_fifo.first) == "state") {
      auto& fifo = metric_to_fifo.second.get_fifo();
      if (!fifo.empty()) {
        const auto& last_sample = *fifo.rbegin();
        last_time = last_sample.get_nano_timestamp();
        res.exit_code = last_sample.get_value();
        metric_to_fifo.second.clean_oldest(last_time);
      }
      break;
    }
  }
  if (!last_time) {
    return false;
  }
  res.command_id = get_command_id();
  res.exit_status = process::normal;
  res.end_time = res.start_time = last_time / 1000000000;

  // construct perfdata list by perfdata name
  std::map<std::string, detail::perf_data> perfs;

  for (auto& metric_to_fifo : fifos) {
    std::string_view suffix = get_nagios_telegraf_suffix(metric_to_fifo.first);
    const data_point_fifo::container& data_points =
        metric_to_fifo.second.get_fifo();
    // we scan all data points for that metric (example check_icmp_critical_gt
    // can contain a data point for pl and another for rta)
    auto data_pt_search = data_points.equal_range(last_time);
    for (; data_pt_search.first != data_pt_search.second;
         ++data_pt_search.first) {
      const auto attributes = data_pt_search.first->get_data_point_attributes();
      std::string perfdata_name;
      std::string unit;
      for (const auto& attrib : attributes) {
        if (attrib.key() == "perfdata") {
          perfdata_name = attrib.value().string_value();
        } else if (attrib.key() == "unit") {
          unit = attrib.value().string_value();
        }
        if (!perfdata_name.empty() && !unit.empty()) {
          break;
        }
      }
      if (!perfdata_name.empty()) {
        detail::perf_data& to_fill = perfs[perfdata_name];
        if (!unit.empty()) {
          to_fill.unit = unit;
        }
        to_fill.fill_from_suffix(suffix, data_pt_search.first->get_value());
      }
    }
    metric_to_fifo.second.clean_oldest(last_time);
  }

  data_point_container::clean_empty_fifos(fifos);

  // then format all in a string with format:
  // 'label'=value[UOM];[warn];[crit];[min];[max]
  if (res.exit_code >= 0 && res.exit_code < 4) {
    res.output = state_str[res.exit_code];
  }
  res.output.push_back('|');
  for (const auto& perf : perfs) {
    if (perf.second.val) {
      res.output += perf.first;
      res.output.push_back('=');
      absl::StrAppend(&res.output, *perf.second.val);
      res.output += perf.second.unit;
      res.output.push_back(';');
      if (perf.second.warning_le) {
        res.output.push_back('@');
        absl::StrAppend(&res.output, *perf.second.warning_le);
        res.output.push_back(':');
        absl::StrAppend(&res.output, *perf.second.warning_ge);

      } else if (perf.second.warning_lt) {
        absl::StrAppend(&res.output, *perf.second.warning_lt);
        res.output.push_back(':');
        absl::StrAppend(&res.output, *perf.second.warning_gt);
      }
      res.output.push_back(';');
      if (perf.second.critical_le) {
        res.output.push_back('@');
        absl::StrAppend(&res.output, *perf.second.critical_le);
        res.output.push_back(':');
        absl::StrAppend(&res.output, *perf.second.critical_ge);

      } else if (perf.second.critical_lt) {
        absl::StrAppend(&res.output, *perf.second.critical_lt);
        res.output.push_back(':');
        absl::StrAppend(&res.output, *perf.second.critical_gt);
      }
      res.output.push_back(';');
      if (perf.second.min) {
        absl::StrAppend(&res.output, *perf.second.min);
      }
      res.output.push_back(';');
      if (perf.second.max) {
        absl::StrAppend(&res.output, *perf.second.max);
      }

      res.output.push_back(' ');
    }
  }
  // remove last space
  res.output.pop_back();
  return true;
}

/**
 * @brief remove converter_type from command_line
 *
 * @param cmd_line exemple nagios_telegraf
 * @return std::string
 */
std::string otl_converter::remove_converter_type(const std::string& cmd_line) {
  size_t sep_pos = cmd_line.find(' ');
  std::string params =
      sep_pos == std::string::npos ? "" : cmd_line.substr(sep_pos + 1);

  boost::trim(params);
  return params;
}
