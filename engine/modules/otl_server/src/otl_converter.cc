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
    : _command_id(command_id),
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
  std::string params =
      sep_pos == std::string::npos ? "" : cmd_line.substr(sep_pos + 1);

  boost::trim(params);

  if (conf_type == "nagios_telegraf") {
    return std::make_shared<otl_nagios_telegraf_converter>(
        params, command_id, host, service, timeout, std::move(handler));
  } else {
    SPDLOG_LOGGER_ERROR(log_v2::otl(), "unknown converter type:{}", conf_type);
    throw exceptions::msg_fmt("unknown converter type:{}", conf_type);
  }
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
  std::string_view unit;
  std::optional<double> val;
  std::optional<double> warning_le, warning_lt, warning_ge, warning_gt;
  std::optional<double> critical_le, critical_lt, critical_ge, critical_gt;
  std::optional<double> min, max;
};

}  // namespace com::centreon::engine::modules::otl_server::detail

static std::string_view get_nagios_telegraf_suffix(
    const std::string_view metric_name) {
  std::size_t sep_pos = metric_name.rfind('_');
  if (sep_pos == std::string::npos) {
    return "";
  }
  std::string_view last_word = metric_name.substr(sep_pos + 1);
  if (last_word == "lt" || last_word == "gt" || last_word == "lt" ||
      last_word == "gt" && sep_pos > 0) {  // critical_lt or warning_le
    sep_pos = metric_name.find('_', sep_pos - 1);
    if (sep_pos != std::string_view::npos) {
      return metric_name.substr(sep_pos + 1);
    }
  }
  return last_word;
}

/**
 * @brief
 *
 * @param fifos
 * @return com::centreon::engine::commands::result
 */
bool otl_nagios_telegraf_converter::_build_result_from_metrics(
    metric_name_to_fifo& fifos,
    commands::result& res) {
  // first we search last state timestamp
  uint64_t last_time = 0;

  for (const auto& metric_to_fifo : fifos) {
    if (get_nagios_telegraf_suffix(metric_to_fifo.first) == "state") {
      const auto& fifo = metric_to_fifo.second.get_fifo();
      if (!fifo.empty()) {
        const auto& last_sample = *fifo.rbegin();
        last_time = last_sample.get_nano_timestamp();
        res.exit_code = last_sample.get_value();
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
  absl::flat_hash_map<std::string_view, detail::perf_data> perfs;

  for (const auto& metric_to_fifo : fifos) {
    const auto& data_pt_search =
        metric_to_fifo.second.get_fifo().find(last_time);
    if (data_pt_search != metric_to_fifo.second.get_fifo().end()) {
      std::string_view suffix =
          get_nagios_telegraf_suffix(metric_to_fifo.first);
      const auto attributes = data_pt_search->get_data_point_attributes();
      std::string_view perfdata_name;
      std::string_view unit;
      for (const auto& attrib : attributes) {
        if (attrib.key() == "perfdata") {
          perfdata_name = attrib.value().string_value();
          unit = attrib.value().string_value();
          ;
        }
      }
      if (!perfdata_name.empty()) {
        detail::perf_data& to_fill = perfs[perfdata_name];
        if (!unit.empty()) {
          to_fill.unit = unit;
        }
        if (suffix == "critical_le") {
          to_fill.critical_le = data_pt_search->get_value();
        } else if (suffix == "critical_ge") {
          to_fill.critical_ge = data_pt_search->get_value();
        } else if (suffix == "critical_lt") {
          to_fill.critical_lt = data_pt_search->get_value();
        } else if (suffix == "critical_gt") {
          to_fill.critical_gt = data_pt_search->get_value();
        } else if (suffix == "critical_le") {
          to_fill.critical_le = data_pt_search->get_value();
        } else if (suffix == "critical_ge") {
          to_fill.critical_ge = data_pt_search->get_value();
        } else if (suffix == "critical_lt") {
          to_fill.critical_lt = data_pt_search->get_value();
        } else if (suffix == "critical_gt") {
          to_fill.critical_gt = data_pt_search->get_value();
        } else if (suffix == "min") {
          to_fill.min = data_pt_search->get_value();
        } else if (suffix == "max") {
          to_fill.max = data_pt_search->get_value();
        } else if (suffix == "value") {
          to_fill.val = data_pt_search->get_value();
        }
      }
    }
  }

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
  return true;
}
