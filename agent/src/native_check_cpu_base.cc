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

#include "native_check_cpu_base.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_cpu_detail;

/**
 * @brief Construct a new per cpu time base<nb metric>::per cpu time base object
 * all values are set to zero
 * @tparam nb_metric
 */
template <unsigned nb_metric>
per_cpu_time_base<nb_metric>::per_cpu_time_base() {
  _metrics.fill(0);
}

/**
 * @brief dump all values into plugin output
 *
 * @tparam nb_metric
 * @param cpu_index cpu index or average_cpu_index for all cpus
 * @param metric_label label for each metric
 * @param output output string
 */
template <unsigned nb_metric>
void per_cpu_time_base<nb_metric>::dump(const unsigned& cpu_index,
                                        const std::string_view metric_label[],
                                        std::string* output) const {
  if (cpu_index == average_cpu_index) {
    *output += fmt::format("CPU(s) average Usage: {:.2f}%",
                           (static_cast<double>(_total_used) / _total) * 100);
  } else {
    *output += fmt::format("CPU'{}' Usage: {:.2f}%", cpu_index,
                           (static_cast<double>(_total_used) / _total) * 100);
  }

  for (unsigned field_index = 0; field_index < nb_metric; ++field_index) {
    *output += metric_label[field_index];
    *output +=
        fmt::format("{:.2f}%", get_proportional_value(field_index) * 100);
  }
}

/**
 * @brief used for debugging
 *
 * @param output
 */
template <unsigned nb_metric>
void per_cpu_time_base<nb_metric>::dump_values(std::string* output) const {
  for (unsigned field_index = 0; field_index < nb_metric; ++field_index) {
    absl::StrAppend(output, " ", _metrics[field_index]);
  }
  absl::StrAppend(output, " used:", _total_used);
  absl::StrAppend(output, " total:", _total);
}

/**
 * @brief subtract a per_cpu_time_base from this
 *
 * @tparam nb_metric
 * @param to_subtract
 */
template <unsigned nb_metric>
void per_cpu_time_base<nb_metric>::subtract(
    const per_cpu_time_base<nb_metric>& to_subtract) {
  typename std::array<uint64_t, nb_metric>::iterator dest = _metrics.begin();
  typename std::array<uint64_t, nb_metric>::const_iterator src =
      to_subtract._metrics.begin();
  for (; dest < _metrics.end(); ++dest, ++src) {
    *dest -= *src;
  }
  _total_used -= to_subtract._total_used;
  _total -= to_subtract._total;
}

/**
 * @brief add a per_cpu_time_base to this
 *
 * @tparam nb_metric
 * @param to_add
 */
template <unsigned nb_metric>
void per_cpu_time_base<nb_metric>::add(const per_cpu_time_base& to_add) {
  typename std::array<uint64_t, nb_metric>::iterator dest = _metrics.begin();
  typename std::array<uint64_t, nb_metric>::const_iterator src =
      to_add._metrics.begin();
  for (; dest < _metrics.end(); ++dest, ++src) {
    *dest += *src;
  }
  _total_used += to_add._total_used;
  _total += to_add._total;
}

/**
 * @brief subtract a cpu snapshot from this
 *
 * @tparam nb_metric
 * @param to_subtract
 * @return index_to_cpu<nb_metric>
 */
template <unsigned nb_metric>
index_to_cpu<nb_metric> cpu_time_snapshot<nb_metric>::subtract(
    const cpu_time_snapshot& to_subtract) const {
  index_to_cpu<nb_metric> result;
  // in case of pdh, first measure is empty, so we use only second sample
  if (to_subtract._data.empty()) {
    return _data;
  }
  for (const auto& left_it : _data) {
    const auto& right_it = to_subtract._data.find(left_it.first);
    if (right_it == to_subtract._data.end()) {
      continue;
    }
    per_cpu_time_base<nb_metric>& res = result[left_it.first];
    res = left_it.second;
    res.subtract(right_it->second);
  }
  return result;
}

/**
 * @brief used for debug, dump all values
 *
 * @tparam nb_metric
 * @param cpus
 * @param output
 */
template <unsigned nb_metric>
void cpu_time_snapshot<nb_metric>::dump(std::string* output) const {
  output->reserve(output->size() + _data.size() * 256);
  for (const auto& cpu : _data) {
    output->push_back(cpu.first + '0');
    output->append(":{");
    for (unsigned i = 0; i < nb_metric; ++i) {
      absl::StrAppend(output, " ", cpu.second.get_proportional_value(i));
    }
    absl::StrAppend(output, " used:", cpu.second.get_proportional_used());
    output->push_back('\n');
    cpu.second.dump_values(output);

    output->append("}\n");
  }
}

/**
 * @brief update status of each cpu or all cpus if metric > threshold
 *
 * @tparam nb_metric
 * @param to_test metrics
 * @param per_cpu_status out: status per cpu index
 */
template <unsigned nb_metric>
void cpu_to_status<nb_metric>::compute_status(
    const index_to_cpu<nb_metric>& to_test,
    boost::container::flat_map<unsigned, e_status>* per_cpu_status) const {
  auto check_threshold =
      [&, this](const typename index_to_cpu<nb_metric>::value_type& values) {
        double val = _data_index >= nb_metric
                         ? values.second.get_proportional_used()
                         : values.second.get_proportional_value(_data_index);
        if (val > _threshold) {
          auto& to_update = (*per_cpu_status)[values.first];
          // if ok (=0) and _status is warning (=1) or critical(=2), we update
          if (_status > to_update) {
            to_update = _status;
          }
        }
      };

  if (_average) {
    auto avg = to_test.find(average_cpu_index);
    if (avg == to_test.end()) {
      return;
    }
    check_threshold(*avg);
  } else {
    for (const auto& by_cpu : to_test) {
      if (by_cpu.first == average_cpu_index) {
        continue;
      }
      check_threshold(by_cpu);
    }
  }
}

/**
 * @brief Construct a new check native_check_cpu cpu object
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
native_check_cpu<nb_metric>::native_check_cpu(
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

      _nb_core(std::thread::hardware_concurrency()),
      _cpu_detailed(false),
      _measure_timer(*io_context) {
  if (args.IsObject()) {
    com::centreon::common::rapidjson_helper arg(args);
    _cpu_detailed = arg.get_bool("cpu-detailed", false);
  }
}

/**
 * @brief start a measure
 * measure duration is the min of timeout - 1s, check_interval - 1s
 *
 * @param timeout
 */
template <unsigned nb_metric>
void native_check_cpu<nb_metric>::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  try {
    std::unique_ptr<check_cpu_detail::cpu_time_snapshot<nb_metric>> begin =
        get_cpu_time_snapshot(true);

    time_point end_measure = std::chrono::system_clock::now() + timeout;

    end_measure -= std::chrono::seconds(1);

    _measure_timer.expires_at(end_measure);
    _measure_timer.async_wait(
        [me = shared_from_this(), first_measure = std::move(begin),
         start_check_index = _get_running_check_index()](
            const boost::system::error_code& err) mutable {
          me->_measure_timer_handler(err, start_check_index,
                                     std::move(first_measure));
        });
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "{} fail to start check: {}",
                        get_command_name(), e.what());
    asio::post(*_io_context, [me = shared_from_this(),
                              start_check_index = _get_running_check_index(),
                              err = e.what()] {
      me->on_completion(start_check_index, e_status::unknown, {}, {err});
    });
  }
}

/**
 * @brief called at measure timer expiration
 * Then we take a new snapshot of /proc/stat, compute difference with
 * first_measure and generate output and perfdatas
 *
 * @param err asio error
 * @param start_check_index passed to on_completion to validate result
 * @param first_measure first snapshot to compare
 */
template <unsigned nb_metric>
void native_check_cpu<nb_metric>::_measure_timer_handler(
    const boost::system::error_code& err,
    unsigned start_check_index,
    std::unique_ptr<check_cpu_detail::cpu_time_snapshot<nb_metric>>&&
        first_measure) {
  if (err) {
    return;
  }

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  std::unique_ptr<check_cpu_detail::cpu_time_snapshot<nb_metric>> new_measure =
      get_cpu_time_snapshot(false);

  e_status worst = compute(*first_measure, *new_measure, &output, &perfs);

  on_completion(start_check_index, worst, perfs, {output});
}

/**
 * @brief compute the difference between second_measure and first_measure and
 * generate status, output and perfdatas
 *
 * @param first_measure first snapshot of /proc/stat
 * @param second_measure second snapshot of /proc/stat
 * @param output out plugin output
 * @param perfs perfdatas
 * @return e_status plugin out status
 */
template <unsigned nb_metric>
e_status native_check_cpu<nb_metric>::_compute(
    const check_cpu_detail::cpu_time_snapshot<nb_metric>& first_measure,
    const check_cpu_detail::cpu_time_snapshot<nb_metric>& second_measure,
    const std::string_view summary_labels[],
    const std::string_view perfdata_labels[],
    std::string* output,
    std::list<com::centreon::common::perfdata>* perfs) {
  index_to_cpu<nb_metric> delta = second_measure.subtract(first_measure);

  // we need to know per cpu status to provide no ok cpu details
  boost::container::flat_map<unsigned, e_status> by_proc_status;

  for (const auto& checker : _cpu_to_status) {
    checker.second.compute_status(delta, &by_proc_status);
  }

  e_status worst = e_status::ok;
  for (const auto& to_cmp : by_proc_status.sequence()) {
    if (to_cmp.second > worst) {
      worst = to_cmp.second;
    }
  }

  if (worst == e_status::ok) {  // all is ok
    auto average_data = delta.find(check_cpu_detail::average_cpu_index);
    if (average_data != delta.end()) {
      *output = fmt::format("OK: CPU(s) average usage is {:.2f}%",
                            average_data->second.get_proportional_used() * 100);
    } else {
      *output = "OK: CPUs usages are ok.";
    }
  } else {
    bool first = true;
    // not all cpus ok => display detail per cpu nok
    for (const auto& cpu_status : by_proc_status) {
      if (cpu_status.second != e_status::ok) {
        if (first) {
          first = false;
        } else {
          output->push_back(' ');
        }
        *output += status_label[cpu_status.second];
        delta[cpu_status.first].dump(cpu_status.first, summary_labels, output);
      }
    }
  }

  auto fill_perfdata = [&, this](
                           const std::string_view& label, unsigned index,
                           unsigned cpu_index,
                           const per_cpu_time_base<nb_metric>& per_cpu_data) {
    double val = index >= nb_metric
                     ? per_cpu_data.get_proportional_used()
                     : per_cpu_data.get_proportional_value(index);
    bool is_average = cpu_index == check_cpu_detail::average_cpu_index;
    common::perfdata to_add;
    to_add.name(label);
    to_add.unit("%");
    to_add.min(0);
    to_add.max(100);
    to_add.value(val * 100);
    // we search cpu_to_status to get warning and critical thresholds
    // warning
    auto cpu_to_status_search = _cpu_to_status.find(
        std::make_tuple(index, is_average, e_status::warning));
    if (cpu_to_status_search != _cpu_to_status.end()) {
      to_add.warning_low(0);
      to_add.warning(100 * cpu_to_status_search->second.get_threshold());
    }
    // critical
    cpu_to_status_search = _cpu_to_status.find(
        std::make_tuple(index, is_average, e_status::critical));
    if (cpu_to_status_search != _cpu_to_status.end()) {
      to_add.critical_low(0);
      to_add.critical(100 * cpu_to_status_search->second.get_threshold());
    }
    perfs->emplace_back(std::move(to_add));
  };

  if (_cpu_detailed) {
    for (const auto& by_core : delta) {
      std::string cpu_name;
      const char* suffix;
      if (by_core.first != check_cpu_detail::average_cpu_index) {
        absl::StrAppend(&cpu_name, by_core.first, "~");
        suffix = "#core.cpu.utilization.percentage";
      } else {
        suffix = "#cpu.utilization.percentage";
      }
      for (unsigned stat_ind = 0; stat_ind < nb_metric; ++stat_ind) {
        fill_perfdata((cpu_name + perfdata_labels[stat_ind].data()) + suffix,
                      stat_ind, by_core.first, by_core.second);
      }
      fill_perfdata((cpu_name + "used") + suffix, nb_metric, by_core.first,
                    by_core.second);
    }

  } else {
    for (const auto& by_core : delta) {
      std::string cpu_name;
      if (by_core.first != check_cpu_detail::average_cpu_index) {
        absl::StrAppend(&cpu_name, by_core.first,
                        "#core.cpu.utilization.percentage");
      } else {
        cpu_name = "cpu.utilization.percentage";
      }

      fill_perfdata(cpu_name, nb_metric, by_core.first, by_core.second);
    }
  }
  return worst;
}
