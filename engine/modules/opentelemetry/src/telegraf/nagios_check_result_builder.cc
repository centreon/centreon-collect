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

#include "data_point_fifo_container.hh"
#include "otl_check_result_builder.hh"

#include "telegraf/nagios_check_result_builder.hh"

using namespace com::centreon::engine::modules::opentelemetry::telegraf;

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

namespace com::centreon::engine::modules::opentelemetry::telegraf::detail {

struct perf_data {
  std::string unit;
  std::optional<double> val;
  std::optional<double> warning_le, warning_lt, warning_ge, warning_gt;
  std::optional<double> critical_le, critical_lt, critical_ge, critical_gt;
  std::optional<double> min, max;

  bool fill_from_suffix(const std::string_view& suffix,
                        double value,
                        const std::shared_ptr<spdlog::logger>& logger);

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
bool perf_data::fill_from_suffix(
    const std::string_view& suffix,
    double value,
    const std::shared_ptr<spdlog::logger>& logger) {
  auto search = _suffix_to_value.find(suffix);
  if (search != _suffix_to_value.end()) {
    this->*search->second = value;
    return true;
  }
  SPDLOG_LOGGER_WARN(logger, "unknown suffix {}", suffix);
  return false;
}

}  // namespace com::centreon::engine::modules::opentelemetry::telegraf::detail

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
  if ((last_word == "lt" || last_word == "gt" || last_word == "le" ||
       last_word == "ge") &&
      sep_pos > 0) {  // critical_lt or warning_le
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
bool nagios_check_result_builder::_build_result_from_metrics(
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
        to_fill.fill_from_suffix(suffix, data_pt_search.first->get_value(),
                                 _logger);
      }
    }
    metric_to_fifo.second.clean_oldest(last_time);
  }

  data_point_fifo_container::clean_empty_fifos(fifos);

  // then format all in a string with format:
  // 'label'=value[UOM];[warn];[crit];[min];[max]
  if (res.exit_code >= 0 && res.exit_code < 4) {
    res.output = state_str[res.exit_code];
  }
  res.output.push_back('|');
  for (const auto& perf : perfs) {
    if (perf.second.val) {
      absl::StrAppend(&res.output, perf.first, "=", *perf.second.val,
                      perf.second.unit, ";");
      if (perf.second.warning_le) {
        absl::StrAppend(&res.output, "@", *perf.second.warning_le, ":",
                        *perf.second.warning_ge);

      } else if (perf.second.warning_lt) {
        absl::StrAppend(&res.output, *perf.second.warning_lt, ":",
                        *perf.second.warning_gt);
      }
      res.output.push_back(';');
      if (perf.second.critical_le) {
        absl::StrAppend(&res.output, "@", *perf.second.critical_le, ":",
                        *perf.second.critical_ge);
      } else if (perf.second.critical_lt) {
        absl::StrAppend(&res.output, *perf.second.critical_lt, ":",
                        *perf.second.critical_gt);
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
