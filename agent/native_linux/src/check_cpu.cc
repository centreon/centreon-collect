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

#include "check_cpu.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_cpu_detail;

/**
 * @brief Construct a new per cpu time::per cpu time object
 * it parses a line like cpu0 2930565 15541 1250726 10453908 54490 0 27068 0 0 0
 *
 * @param line
 */
per_cpu_time::per_cpu_time(const std::string_view& line) {
  using namespace std::literals;
  auto split_res = absl::StrSplit(line, ' ', absl::SkipEmpty());
  auto field_iter = split_res.begin();

  if ((*field_iter).substr(0, 3) != "cpu"sv) {
    throw std::invalid_argument("no cpu");
  }
  if (!absl::SimpleAtoi(field_iter->substr(3), &_cpu_index)) {
    _cpu_index = check_cpu_detail::average_cpu_index;
  }

  unsigned* to_fill = _data;
  unsigned* end = _data + used;  // used will be calculated after

  for (++field_iter; field_iter != split_res.end(); ++field_iter, ++to_fill) {
    unsigned counter;
    if (!absl::SimpleAtoi(*field_iter, &counter)) {
      throw std::invalid_argument("not a number");
    }
    // On some OS we may have more fields than user to guest_nice, we have to
    // take them into account only for total compute
    if (to_fill < end) {
      *to_fill = counter;
    }
    _total += counter;
  }

  // On some OS, we might have fewer fields than expected, so we initialize
  // the remaining fields
  for (; to_fill < end; ++to_fill)
    *to_fill = 0;

  // Calculate the 'used' CPU time by subtracting idle time from total time
  _data[e_proc_stat_index::used] = _total - _data[e_proc_stat_index::idle];
}

/**
 * @brief substract all fields and _total
 *
 * @param to_add
 * @return per_cpu_time& (this)
 */
per_cpu_time& per_cpu_time::operator-=(const per_cpu_time& to_substract) {
  unsigned* res = _data;
  unsigned* end = _data + nb_field;
  const unsigned* val_to_substract = to_substract._data;
  for (; res < end; ++res, ++val_to_substract) {
    if (*res > *val_to_substract) {
      *res -= *val_to_substract;
    } else {
      *res = 0;
    }
  }
  if (_total > to_substract._total) {
    _total -= to_substract._total;
  } else {
    _total = 1;  // no 0 divide
  }
  return *this;
}

constexpr std::array<std::string_view, e_proc_stat_index::nb_field>
    _sz_stat_index = {", User ",   ", Nice ",       ", System ",   ", Idle ",
                      ", IOWait ", ", Interrupt ",  ", Soft Irq ", ", Steal ",
                      ", Guest ",  ", Guest Nice ", ", Usage"};

/**
 * @brief print values summary to plugin output
 *
 * @param output plugin out
 */
void per_cpu_time::dump(std::string* output) const {
  using namespace std::literals;
  if (_cpu_index == check_cpu_detail::average_cpu_index) {
    *output +=
        fmt::format("CPU(s) average Usage: {:.2f}%",
                    get_proportional_value(e_proc_stat_index::used) * 100);
  } else {
    *output +=
        fmt::format("CPU'{}' Usage: {:.2f}%", _cpu_index,
                    get_proportional_value(e_proc_stat_index::used) * 100);
  }

  for (unsigned field_index = 0; field_index < e_proc_stat_index::used;
       ++field_index) {
    *output += _sz_stat_index[field_index];
    *output +=
        fmt::format("{:.2f}%", get_proportional_value(field_index) * 100);
  }
}

void com::centreon::agent::check_cpu_detail::dump(const index_to_cpu& cpus,
                                                  std::string* output) {
  output->reserve(output->length() + cpus.size() * 256);
  for (const auto& cpu : cpus) {
    cpu.second.dump(output);
    output->push_back('\n');
  }
}

/**
 * @brief Construct a new proc stat file::proc stat file object
 *
 * @param proc_file path of the proc file usually: /proc/stat, other for unit
 * tests
 * @param nb_to_reserve nb host cores
 */
proc_stat_file::proc_stat_file(const char* proc_file, size_t nb_to_reserve) {
  _values.reserve(nb_to_reserve);
  std::ifstream proc_stat(proc_file);
  char line_buff[1024];
  while (1) {
    try {
      proc_stat.getline(line_buff, sizeof(line_buff));
      line_buff[1023] = 0;
      per_cpu_time to_ins(line_buff);
      _values.emplace(to_ins.get_cpu_index(), to_ins);
    } catch (const std::exception&) {
      return;
    }
  }
}

/**
 * @brief computes difference between two snapshots of /proc/stat
 *
 * @param right (older snapshot)
 * @return index_to_cpu by cpu difference
 */
index_to_cpu proc_stat_file::operator-(const proc_stat_file& right) const {
  index_to_cpu ret;
  const auto& latest_values = _values;
  const auto& older_values = right.get_values();
  for (const auto& latest_cpu : latest_values) {
    auto search = older_values.find(latest_cpu.first);
    if (search != older_values.end()) {
      per_cpu_time to_ins(latest_cpu.second);
      to_ins -= search->second;
      ret.emplace(latest_cpu.first, to_ins);
    }
  }
  return ret;
}

/**
 * @brief dump
 *
 * @param output
 */
void proc_stat_file::dump(std::string* output) const {
  for (const auto& cpu : _values) {
    cpu.second.dump(output);
    output->push_back('\n');
  }
}

/**
 * @brief compare cpu values to a threshold and update cpu status if field value
 * > threshold
 *
 * @param to_test cpus usage to compare
 * @param per_cpu_status out parameter that contains per cpu worst status
 */
void cpu_to_status::compute_status(
    const index_to_cpu& to_test,
    boost::container::flat_map<unsigned, e_status>* per_cpu_status) const {
  auto check_threshold = [&, this](const index_to_cpu::value_type& values) {
    double val = values.second.get_proportional_value(_data_index);
    if (val > _threshold) {
      auto& to_update = (*per_cpu_status)[values.first];
      // if ok (=0) and _status is warning (=1) or critical(=2), we update
      if (_status > to_update) {
        to_update = _status;
      }
    }
  };

  if (_average) {
    index_to_cpu::const_iterator avg =
        to_test.find(check_cpu_detail::average_cpu_index);
    if (avg == to_test.end()) {
      return;
    }
    check_threshold(*avg);
  } else {
    for (const auto& by_cpu : to_test) {
      if (by_cpu.first == check_cpu_detail::average_cpu_index) {
        continue;
      }
      check_threshold(by_cpu);
    }
  }
}

using cpu_to_status_constructor =
    std::function<cpu_to_status(double /*threshold*/)>;

#define BY_TYPE_CPU_TO_STATUS(TYPE_METRIC)                                     \
  {"warning-core-" #TYPE_METRIC,                                               \
   [](double threshold) {                                                      \
     return cpu_to_status(e_status::warning, e_proc_stat_index::TYPE_METRIC,   \
                          false, threshold);                                   \
   }},                                                                         \
      {"critical-core-" #TYPE_METRIC,                                          \
       [](double threshold) {                                                  \
         return cpu_to_status(e_status::critical,                              \
                              e_proc_stat_index::TYPE_METRIC, false,           \
                              threshold);                                      \
       }},                                                                     \
      {"warning-average-" #TYPE_METRIC,                                        \
       [](double threshold) {                                                  \
         return cpu_to_status(e_status::warning,                               \
                              e_proc_stat_index::TYPE_METRIC, true,            \
                              threshold);                                      \
       }},                                                                     \
  {                                                                            \
    "critical-average-" #TYPE_METRIC, [](double threshold) {                   \
      return cpu_to_status(e_status::critical, e_proc_stat_index::TYPE_METRIC, \
                           true, threshold);                                   \
    }                                                                          \
  }

/**
 * @brief this map is used to generate cpus values comparator from check
 * configuration fields
 *
 */
static const absl::flat_hash_map<std::string_view, cpu_to_status_constructor>
    _label_to_cpu_to_status = {
        {"warning-core",
         [](double threshold) {
           return cpu_to_status(e_status::warning, e_proc_stat_index::used,
                                false, threshold);
         }},
        {"critical-core",
         [](double threshold) {
           return cpu_to_status(e_status::critical, e_proc_stat_index::used,
                                false, threshold);
         }},
        {"warning-average",
         [](double threshold) {
           return cpu_to_status(e_status::warning, e_proc_stat_index::used,
                                true, threshold);
         }},
        {"critical-average",
         [](double threshold) {
           return cpu_to_status(e_status::critical, e_proc_stat_index::used,
                                true, threshold);
         }},
        BY_TYPE_CPU_TO_STATUS(user),
        BY_TYPE_CPU_TO_STATUS(nice),
        BY_TYPE_CPU_TO_STATUS(system),
        BY_TYPE_CPU_TO_STATUS(iowait),
        BY_TYPE_CPU_TO_STATUS(guest)};

/**
 * @brief Construct a new check cpu::check cpu object
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
check_cpu::check_cpu(const std::shared_ptr<asio::io_context>& io_context,
                     const std::shared_ptr<spdlog::logger>& logger,
                     time_point first_start_expected,
                     duration check_interval,
                     const std::string& serv,
                     const std::string& cmd_name,
                     const std::string& cmd_line,
                     const rapidjson::Value& args,
                     const engine_to_agent_request_ptr& cnf,
                     check::completion_handler&& handler)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler)),

      _nb_core(std::thread::hardware_concurrency()),
      _cpu_detailed(false),
      _measure_timer(*io_context) {
  com::centreon::common::rapidjson_helper arg(args);
  if (args.IsObject()) {
    for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
         ++member_iter) {
      auto cpu_to_status_search = _label_to_cpu_to_status.find(
          absl::AsciiStrToLower(member_iter->name.GetString()));
      if (cpu_to_status_search != _label_to_cpu_to_status.end()) {
        const rapidjson::Value& val = member_iter->value;
        if (val.IsFloat() || val.IsInt() || val.IsUint() || val.IsInt64() ||
            val.IsUint64()) {
          check_cpu_detail::cpu_to_status cpu_checker =
              cpu_to_status_search->second(member_iter->value.GetDouble() /
                                           100);
          _cpu_to_status.emplace(
              std::make_tuple(cpu_checker.get_proc_stat_index(),
                              cpu_checker.is_average(),
                              cpu_checker.get_status()),
              cpu_checker);
        } else if (val.IsString()) {
          auto to_conv = val.GetString();
          double dval;
          if (absl::SimpleAtod(to_conv, &dval)) {
            check_cpu_detail::cpu_to_status cpu_checker =
                cpu_to_status_search->second(dval / 100);
            _cpu_to_status.emplace(
                std::make_tuple(cpu_checker.get_proc_stat_index(),
                                cpu_checker.is_average(),
                                cpu_checker.get_status()),
                cpu_checker);
          } else {
            SPDLOG_LOGGER_ERROR(
                logger,
                "command: {}, value is not a number for parameter {}: {}",
                cmd_name, member_iter->name, val);
          }

        } else {
          SPDLOG_LOGGER_ERROR(logger,
                              "command: {}, bad value for parameter {}: {}",
                              cmd_name, member_iter->name, val);
        }
      } else if (member_iter->name == "cpu-detailed") {
        _cpu_detailed = arg.get_bool("cpu-detailed", false);
      } else {
        SPDLOG_LOGGER_ERROR(logger, "command: {}, unknown parameter: {}",
                            cmd_name, member_iter->name);
      }
    }
  }
}

/**
 * @brief start a measure
 * measure duration is the min of timeout - 1s, check_interval - 1s
 *
 * @param timeout
 */
void check_cpu::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  std::unique_ptr<proc_stat_file> begin =
      std::make_unique<proc_stat_file>(_nb_core);

  time_point end_measure = std::chrono::system_clock::now() + timeout;
  time_point end_measure_period =
      get_start_expected() +
      std::chrono::seconds(get_conf()->config().check_interval());

  if (end_measure > end_measure_period) {
    end_measure = end_measure_period;
  }

  end_measure -= std::chrono::seconds(1);

  _measure_timer.expires_at(end_measure);
  _measure_timer.async_wait([me = shared_from_this(),
                             first_measure = std::move(begin),
                             start_check_index = _get_running_check_index()](
                                const boost::system::error_code& err) mutable {
    me->_measure_timer_handler(err, start_check_index,
                               std::move(first_measure));
  });
}

constexpr std::array<std::string_view, 4> _sz_status = {
    "OK: ", "WARNING: ", "CRITICAL: ", "UNKNOWN: "};

constexpr std::array<const char*, e_proc_stat_index::nb_field>
    _sz_measure_name = {"user",   "nice",      "system",  "idle",
                        "iowait", "interrupt", "softirq", "steal",
                        "guest",  "guestnice", "used"};

/**
 * @brief called at measure timer expiration
 * Then we take a new snapshot of /proc/stat, compute difference with
 * first_measure and generate output and perfdatas
 *
 * @param err asio error
 * @param start_check_index passed to on_completion to validate result
 * @param first_measure first snapshot to compare
 */
void check_cpu::_measure_timer_handler(
    const boost::system::error_code& err,
    unsigned start_check_index,
    std::unique_ptr<check_cpu_detail::proc_stat_file>&& first_measure) {
  if (err) {
    return;
  }

  std::string output;
  std::list<common::perfdata> perfs;

  proc_stat_file new_measure(_nb_core);

  e_status worst = compute(*first_measure, new_measure, &output, &perfs);

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
e_status check_cpu::compute(
    const check_cpu_detail::proc_stat_file& first_measure,
    const check_cpu_detail::proc_stat_file& second_measure,
    std::string* output,
    std::list<common::perfdata>* perfs) {
  index_to_cpu delta = second_measure - first_measure;

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
      *output = fmt::format(
          "OK: CPU(s) average usage is {:.2f}%",
          average_data->second.get_proportional_value(e_proc_stat_index::used) *
              100);
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
        *output += _sz_status[cpu_status.second];
        delta[cpu_status.first].dump(output);
      }
    }
  }

  auto fill_perfdata = [&, this](const std::string_view& label, unsigned index,
                                 unsigned cpu_index,
                                 const per_cpu_time& per_cpu_data) {
    double val = per_cpu_data.get_proportional_value(index);
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
      for (unsigned stat_ind = e_proc_stat_index::user;
           stat_ind < e_proc_stat_index::nb_field; ++stat_ind) {
        fill_perfdata((cpu_name + _sz_measure_name[stat_ind]) + suffix,
                      stat_ind, by_core.first, by_core.second);
      }
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

      fill_perfdata(cpu_name, e_proc_stat_index::used, by_core.first,
                    by_core.second);
    }
  }
  return worst;
}
