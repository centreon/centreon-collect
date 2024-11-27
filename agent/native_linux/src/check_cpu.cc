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

#include "native_check_cpu_base.cc"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_cpu_detail;

namespace com::centreon::agent::check_cpu_detail {
template class per_cpu_time_base<e_proc_stat_index::nb_field>;
}

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

  auto to_fill = _metrics.begin();
  auto end = _metrics.end();
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
  _total_used = _total - _metrics[e_proc_stat_index::idle];
}

/**
 * @brief Construct a new proc stat file::proc stat file object
 *
 * @param proc_file path of the proc file usually: /proc/stat, other for unit
 * tests
 * @param nb_to_reserve nb host cores
 */
proc_stat_file::proc_stat_file(const char* proc_file, size_t nb_to_reserve) {
  _data.reserve(nb_to_reserve + 1);
  std::ifstream proc_stat(proc_file);
  char line_buff[1024];
  while (1) {
    try {
      proc_stat.getline(line_buff, sizeof(line_buff));
      line_buff[1023] = 0;
      per_cpu_time to_ins(line_buff);
      _data.emplace(to_ins.get_cpu_index(), to_ins);
    } catch (const std::exception&) {
      return;
    }
  }
}

using linux_cpu_to_status = cpu_to_status<e_proc_stat_index::nb_field>;

using cpu_to_status_constructor =
    std::function<linux_cpu_to_status(double /*threshold*/)>;

#define BY_TYPE_CPU_TO_STATUS(TYPE_METRIC)                                     \
  {"warning-core-" #TYPE_METRIC,                                               \
   [](double threshold) {                                                      \
     return linux_cpu_to_status(                                               \
         e_status::warning, e_proc_stat_index::TYPE_METRIC, false, threshold); \
   }},                                                                         \
      {"critical-core-" #TYPE_METRIC,                                          \
       [](double threshold) {                                                  \
         return linux_cpu_to_status(e_status::critical,                        \
                                    e_proc_stat_index::TYPE_METRIC, false,     \
                                    threshold);                                \
       }},                                                                     \
      {"warning-average-" #TYPE_METRIC,                                        \
       [](double threshold) {                                                  \
         return linux_cpu_to_status(e_status::warning,                         \
                                    e_proc_stat_index::TYPE_METRIC, true,      \
                                    threshold);                                \
       }},                                                                     \
  {                                                                            \
    "critical-average-" #TYPE_METRIC, [](double threshold) {                   \
      return linux_cpu_to_status(e_status::critical,                           \
                                 e_proc_stat_index::TYPE_METRIC, true,         \
                                 threshold);                                   \
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
           return linux_cpu_to_status(e_status::warning,
                                      e_proc_stat_index::nb_field, false,
                                      threshold);
         }},
        {"critical-core",
         [](double threshold) {
           return linux_cpu_to_status(e_status::critical,
                                      e_proc_stat_index::nb_field, false,
                                      threshold);
         }},
        {"warning-average",
         [](double threshold) {
           return linux_cpu_to_status(
               e_status::warning, e_proc_stat_index::nb_field, true, threshold);
         }},
        {"critical-average",
         [](double threshold) {
           return linux_cpu_to_status(e_status::critical,
                                      e_proc_stat_index::nb_field, true,
                                      threshold);
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
    : native_check_cpu<check_cpu_detail::e_proc_stat_index::nb_field>(
          io_context,
          logger,
          first_start_expected,
          check_interval,
          serv,
          cmd_name,
          cmd_line,
          args,
          cnf,
          std::move(handler))

{
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
      } else if (member_iter->name != "cpu-detailed") {
        SPDLOG_LOGGER_ERROR(logger, "command: {}, unknown parameter: {}",
                            cmd_name, member_iter->name);
      }
    }
  }
}

std::unique_ptr<
    check_cpu_detail::cpu_time_snapshot<e_proc_stat_index::nb_field>>
check_cpu::get_cpu_time_snapshot([[maybe_unused]] bool first_measure) {
  return std::make_unique<check_cpu_detail::proc_stat_file>(_nb_core);
}

constexpr std::array<std::string_view, e_proc_stat_index::nb_field>
    _sz_summary_labels = {", User ",      ", Nice ",   ", System ",
                          ", Idle ",      ", IOWait ", ", Interrupt ",
                          ", Soft Irq ",  ", Steal ",  ", Guest ",
                          ", Guest Nice "};

constexpr std::array<std::string_view, e_proc_stat_index::nb_field>
    _sz_perfdata_name = {"user",   "nice",      "system",  "idle",
                         "iowait", "interrupt", "softirq", "steal",
                         "guest",  "guestnice"};

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
    const check_cpu_detail::cpu_time_snapshot<
        check_cpu_detail::e_proc_stat_index::nb_field>& first_measure,
    const check_cpu_detail::cpu_time_snapshot<
        check_cpu_detail::e_proc_stat_index::nb_field>& second_measure,
    std::string* output,
    std::list<common::perfdata>* perfs) {
  output->reserve(256 * _nb_core);

  return _compute(first_measure, second_measure, _sz_summary_labels.data(),
                  _sz_perfdata_name.data(), output, perfs);
}
