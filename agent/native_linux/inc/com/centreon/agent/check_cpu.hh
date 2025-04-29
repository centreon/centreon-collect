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

#ifndef CENTREON_AGENT_CHECK_CPU_HH
#define CENTREON_AGENT_CHECK_CPU_HH

#include "native_check_cpu_base.hh"
namespace com::centreon::agent {

namespace check_cpu_detail {

enum e_proc_stat_index {
  user = 0,
  nice,
  system,
  idle,
  iowait,
  irq,
  soft_irq,
  steal,
  guest,
  guest_nice,
  nb_field
};

/**
 * @brief this class is the result of /proc/stat one line parsing like
 *  cpu0 2930565 15541 1250726 10453908 54490 0 27068 0 0 0
 * if _cpu_index == std::numeric_limits<unsigned>::max(), it represents the sum
 * of all cpus
 *
 */
class per_cpu_time : public per_cpu_time_base<nb_field> {
  unsigned _cpu_index = 0;

 public:
  per_cpu_time() = default;
  per_cpu_time(const std::string_view& line);
  unsigned get_cpu_index() const { return _cpu_index; }
};

/**
 * @brief datas of /proc/stat
 *
 */
class proc_stat_file : public cpu_time_snapshot<nb_field> {
 public:
  proc_stat_file(size_t nb_to_reserve)
      : proc_stat_file("/proc/stat", nb_to_reserve) {}

  proc_stat_file(const char* proc_file, size_t nb_to_reserve);
};

};  // namespace check_cpu_detail

/**
 * @brief native linux check_cpu
 * every _measure_interval, we read /proc/stat and we calculate cpu usage
 * when a check starts, we read last measure and passed it to completion_handler
 * If we not have yet done a measure, we wait to timeout to calculate cpu usage
 */
class check_cpu
    : public native_check_cpu<check_cpu_detail::e_proc_stat_index::nb_field> {
 public:
  check_cpu(const std::shared_ptr<asio::io_context>& io_context,
            const std::shared_ptr<spdlog::logger>& logger,
            time_point first_start_expected,
            duration inter_check_delay,
            duration check_interval,
            const std::string& serv,
            const std::string& cmd_name,
            const std::string& cmd_line,
            const rapidjson::Value& args,
            const engine_to_agent_request_ptr& cnf,
            check::completion_handler&& handler,
            const checks_statistics::pointer& stat);

  static void help(std::ostream& help_stream);

  std::shared_ptr<check_cpu> shared_from_this() {
    return std::static_pointer_cast<check_cpu>(check::shared_from_this());
  }

  std::unique_ptr<check_cpu_detail::cpu_time_snapshot<
      check_cpu_detail::e_proc_stat_index::nb_field>>
  get_cpu_time_snapshot(bool first_measure) override;

  e_status compute(
      const check_cpu_detail::cpu_time_snapshot<
          check_cpu_detail::e_proc_stat_index::nb_field>& first_measure,
      const check_cpu_detail::cpu_time_snapshot<
          check_cpu_detail::e_proc_stat_index::nb_field>& second_measure,
      std::string* output,
      std::list<common::perfdata>* perfs) override;
};
}  // namespace com::centreon::agent

#endif
