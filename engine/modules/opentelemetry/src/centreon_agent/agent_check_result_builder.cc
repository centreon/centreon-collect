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

#include "centreon_agent/agent_check_result_builder.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

namespace com::centreon::engine::modules::opentelemetry::centreon_agent::
    detail {

/**
 * @brief used to create centreon perfdata from agent metric data
 *
 */
struct perf_data {
  std::optional<double> warning_le, warning_lt, warning_ge, warning_gt;
  std::optional<double> critical_le, critical_lt, critical_ge, critical_gt;
  std::optional<double> min, max;

  void apply_exemplar(
      const ::opentelemetry::proto::metrics::v1::Exemplar& exemplar);

  void append_to_string(std::string* to_append);

  static const absl::flat_hash_map<std::string_view,
                                   std::optional<double> perf_data::*>
      _suffix_to_value;
};

const absl::flat_hash_map<std::string_view, std::optional<double> perf_data::*>
    perf_data::_suffix_to_value = {{"warn_le", &perf_data::warning_le},
                                   {"warn_lt", &perf_data::warning_lt},
                                   {"warn_ge", &perf_data::warning_ge},
                                   {"warn_gt", &perf_data::warning_gt},
                                   {"crit_le", &perf_data::critical_le},
                                   {"crit_lt", &perf_data::critical_lt},
                                   {"crit_ge", &perf_data::critical_ge},
                                   {"crit_gt", &perf_data::critical_gt},
                                   {"min", &perf_data::min},
                                   {"max", &perf_data::max}};

/**
 * @brief all metrics sub values are stored in exemplars, so we apply above
 * table to perfdata
 *
 * @param exemplar
 */
void perf_data::apply_exemplar(
    const ::opentelemetry::proto::metrics::v1::Exemplar& exemplar) {
  if (!exemplar.filtered_attributes().empty()) {
    auto search =
        _suffix_to_value.find(exemplar.filtered_attributes().begin()->key());
    if (search != _suffix_to_value.end()) {
      this->*search->second = exemplar.as_double();
    }
  }
}

/**
 * @brief create a nagios style perfdata string from protobuf received data
 *
 * @param to_append
 */
void perf_data::append_to_string(std::string* to_append) {
  if (warning_le) {
    absl::StrAppend(to_append, "@", *warning_le, ":");
    if (warning_ge)
      absl::StrAppend(to_append, *warning_ge);
  } else if (warning_ge) {
    absl::StrAppend(to_append, "@~:", *warning_ge);
  } else if (warning_lt) {
    absl::StrAppend(to_append, *warning_lt, ":");
    if (warning_gt)
      absl::StrAppend(to_append, *warning_gt);
  } else if (warning_gt) {
    absl::StrAppend(to_append, "~:", *warning_gt);
  }
  to_append->push_back(';');
  if (critical_le) {
    absl::StrAppend(to_append, "@", *critical_le, ":");
    if (critical_ge)
      absl::StrAppend(to_append, *critical_ge);
  } else if (critical_ge) {
    absl::StrAppend(to_append, "@~:", *critical_ge);
  } else if (critical_lt) {
    absl::StrAppend(to_append, *critical_lt, ":");
    if (critical_gt)
      absl::StrAppend(to_append, *critical_gt);
  } else if (critical_gt) {
    absl::StrAppend(to_append, "~:", *critical_gt);
  }
  to_append->push_back(';');
  if (min)
    absl::StrAppend(to_append, *min);
  to_append->push_back(';');
  if (max)
    absl::StrAppend(to_append, *max);
}

}  // namespace
   // com::centreon::engine::modules::opentelemetry::centreon_agent::detail

/**
 * @brief
 *
 * @param fifos all metrics for a given service
 * @param res
 * @return true
 * @return false
 */
bool agent_check_result_builder::_build_result_from_metrics(
    metric_name_to_fifo& fifos,
    commands::result& res) {
  // first we search last state timestamp from status
  uint64_t last_time = 0;

  for (auto& metric_to_fifo : fifos) {
    if (metric_to_fifo.first == "status") {
      auto& fifo = metric_to_fifo.second.get_fifo();
      if (!fifo.empty()) {
        const auto& last_sample = *fifo.rbegin();
        last_time = last_sample.get_nano_timestamp();
        res.exit_code = last_sample.get_value();
        // output of plugins is stored in description metric field
        res.output = last_sample.get_metric().description();
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
  res.end_time = res.start_time =
      timestamp(last_time / 1000000000, (last_time / 1000) % 1000000);

  res.output.push_back('|');

  for (auto& metric_to_fifo : fifos) {
    if (metric_to_fifo.first == "status")
      continue;
    auto& fifo = metric_to_fifo.second.get_fifo();
    auto data_pt_search = fifo.find(last_time);
    if (data_pt_search != fifo.end()) {
      res.output.push_back(' ');
      const otl_data_point& data_pt = *data_pt_search;
      absl::StrAppend(&res.output, metric_to_fifo.first, "=",
                      data_pt.get_value(), data_pt.get_metric().unit(), ";");

      // all other metric value (warning_lt, critical_gt, min... are stored in
      // exemplars)
      detail::perf_data to_append;
      for (const auto& exemplar : data_pt.get_exemplars()) {
        to_append.apply_exemplar(exemplar);
      }
      to_append.append_to_string(&res.output);
    }
    metric_to_fifo.second.clean_oldest(last_time);
  }

  data_point_fifo_container::clean_empty_fifos(fifos);

  return true;
}
