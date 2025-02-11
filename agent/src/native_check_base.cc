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

#include "native_check_base.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::native_check_detail;

/**
 * @brief construct a snapshot to status converter
 *
 * @tparam nb_metric
 * @param status e_warning or e_critical
 * @param data_index index of the data to compare
 * @param threshold
 * @param total_data_index index of the total data in order to do a percent
 * compare
 * @param free_threshold if true, status is set if value < threshold
 */
template <unsigned nb_metric>
measure_to_status<nb_metric>::measure_to_status(e_status status,
                                                unsigned data_index,
                                                double threshold,
                                                unsigned total_data_index,
                                                bool percent,
                                                bool free_threshold)
    : _status(status),
      _data_index(data_index),
      _threshold(threshold),
      _total_data_index(total_data_index),
      _percent(percent),
      _free_threshold(free_threshold) {}

template <unsigned nb_metric>
void measure_to_status<nb_metric>::compute_status(
    const snapshot<nb_metric>& to_test,
    e_status* status) const {
  if (_status <= *status) {
    return;
  }
  double value =
      _percent ? to_test.get_proportional_value(_data_index, _total_data_index)
               : to_test.get_metric(_data_index);
  if (_free_threshold) {
    if (value < _threshold) {
      *status = _status;
    }
  } else {
    if (value > _threshold) {
      *status = _status;
    }
  }
}

/**
 * @brief Construct a new check native_check_base
 *
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param check_interval check interval between two checks (not only this but
 * also others)
 * @param serv service
 * @param cmd_name
 * @param cmd_line
 * @param args native plugin arguments
 * @param cnf engine configuration received object
 * @param handler called at measure completion
 */
template <unsigned nb_metric>
native_check_base<nb_metric>::native_check_base(
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
            stat) {}

/**
 * @brief start a measure
 *
 * @param timeout
 */
template <unsigned nb_metric>
void native_check_base<nb_metric>::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  try {
    std::shared_ptr<native_check_detail::snapshot<nb_metric>> mem_metrics =
        measure();

    _io_context->post([me = shared_from_this(),
                       start_check_index = _get_running_check_index(),
                       metrics = mem_metrics]() mutable {
      std::string output;
      output.reserve(1024);
      std::list<com::centreon::common::perfdata> perfs;
      e_status status = me->compute(*metrics, &output, &perfs);
      me->on_completion(start_check_index, status, perfs, {output});
    });

  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to get memory info: {}", e.what());
    _io_context->post([me = shared_from_this(),
                       start_check_index = _get_running_check_index(),
                       err = e.what()] {
      me->on_completion(start_check_index, e_status::unknown, {}, {err});
    });
  }
}

/**
 * @brief compute status, output and metrics from a measure
 *
 * @tparam nb_metric
 * @param data memory measure
 * @param output plugins output
 * @param perfs perfdatas
 * @return e_status plugins status output
 */
template <unsigned nb_metric>
e_status native_check_base<nb_metric>::compute(
    const native_check_detail::snapshot<nb_metric>& data,
    std::string* output,
    std::list<com::centreon::common::perfdata>* perfs) const {
  e_status status = e_status::ok;

  for (const auto& mem_status : _measure_to_status) {
    mem_status.second->compute_status(data, &status);
  }

  *output = status_label[status];
  data.dump_to_output(output);

  const auto& metric_definitions = get_metric_definitions();

  for (const auto& metric : metric_definitions) {
    common::perfdata& to_add = perfs->emplace_back();
    to_add.name(metric.name);
    if (metric.percent) {
      to_add.unit("%");
      to_add.min(0);
      to_add.max(100);
      to_add.value(data.get_proportional_value(metric.data_index,
                                               metric.total_data_index) *
                   100);
    } else {
      if (_no_percent_unit) {
        to_add.unit(_no_percent_unit);
      }
      if (metric.total_data_index != nb_metric) {
        to_add.min(0);
        to_add.max(data.get_metric(metric.total_data_index));
      }
      to_add.value(data.get_metric(metric.data_index));
    }
    // we search measure_to_status to get warning and critical thresholds
    // warning
    auto mem_to_status_search = _measure_to_status.find(std::make_tuple(
        metric.data_index, metric.total_data_index, e_status::warning));
    if (mem_to_status_search != _measure_to_status.end()) {
      to_add.warning_low(0);
      to_add.warning(metric.percent
                         ? 100 * mem_to_status_search->second->get_threshold()
                         : mem_to_status_search->second->get_threshold());
    }
    // critical
    mem_to_status_search = _measure_to_status.find(std::make_tuple(
        metric.data_index, metric.total_data_index, e_status::critical));
    if (mem_to_status_search != _measure_to_status.end()) {
      to_add.critical_low(0);
      to_add.critical(metric.percent
                          ? 100 * mem_to_status_search->second->get_threshold()
                          : mem_to_status_search->second->get_threshold());
    }
  }
  return status;
}