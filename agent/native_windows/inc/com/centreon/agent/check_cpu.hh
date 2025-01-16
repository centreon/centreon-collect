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

#include "ntdll.hh"

#include "native_check_cpu_base.hh"

namespace com::centreon::agent {

namespace check_cpu_detail {
enum e_proc_stat_index { user = 0, system, idle, interrupt, dpc, nb_field };

/**
 * @brief this class contains all counter for one core contained in a
 * M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION structure
 */
class kernel_per_cpu_time
    : public per_cpu_time_base<e_proc_stat_index::nb_field> {
 public:
  kernel_per_cpu_time() = default;

  kernel_per_cpu_time(const M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION& info);
};

/**
 * we can collect cpu metrics in two manners, the first one is to use
microsoft
 * internal NtQuerySystemInformation, the second one is to use the official
 * Performance Data Helper
 * So we have two classes to collect metrics
** /

/**
  * @brief metrics collected by NtQuerySystemInformation
  *
  */
class kernel_cpu_time_snapshot
    : public cpu_time_snapshot<e_proc_stat_index::nb_field> {
 public:
  kernel_cpu_time_snapshot(unsigned nb_core);

  // used by TU
  template <class processor_performance_info_iter>
  kernel_cpu_time_snapshot(processor_performance_info_iter begin,
                           processor_performance_info_iter end);

  void dump(std::string* output) const;
};

template <class processor_performance_info_iter>
kernel_cpu_time_snapshot::kernel_cpu_time_snapshot(
    processor_performance_info_iter begin,
    processor_performance_info_iter end) {
  unsigned cpu_index = 0;
  for (processor_performance_info_iter it = begin; it != end;
       ++it, ++cpu_index) {
    _data[cpu_index] = kernel_per_cpu_time(*it);
  }

  per_cpu_time_base<e_proc_stat_index::nb_field>& total =
      _data[average_cpu_index];
  for (auto to_add_iter = _data.begin();
       to_add_iter != _data.end() && to_add_iter->first != average_cpu_index;
       ++to_add_iter) {
    total.add(to_add_iter->second);
  }
}

struct pdh_counters;

/**
 * @brief metrics collected by Performance Data Helper
 *
 */
class pdh_cpu_time_snapshot
    : public cpu_time_snapshot<e_proc_stat_index::nb_field> {
 public:
  pdh_cpu_time_snapshot(unsigned nb_core,
                        const pdh_counters& counters,
                        bool first_measure);
};

}  // namespace check_cpu_detail

/**
 * @brief native windows check cpu
 *
 */
class check_cpu
    : public native_check_cpu<check_cpu_detail::e_proc_stat_index::nb_field> {
  // this check can collect metrics in two manners, the first one is to use the
  // unofficial NtQuerySystemInformation, the second one is to use the official
  // Performance Data Helper
  bool _use_nt_query_system_information = true;

  std::unique_ptr<check_cpu_detail::pdh_counters> _pdh_counters;

 public:
  check_cpu(const std::shared_ptr<asio::io_context>& io_context,
            const std::shared_ptr<spdlog::logger>& logger,
            time_point first_start_expected,
            duration check_interval,
            const std::string& serv,
            const std::string& cmd_name,
            const std::string& cmd_line,
            const rapidjson::Value& args,
            const engine_to_agent_request_ptr& cnf,
            check::completion_handler&& handler,
            const checks_statistics::pointer& stat);

  ~check_cpu();

  static void help(std::ostream& help_stream);

  std::shared_ptr<check_cpu> shared_from_this() {
    return std::static_pointer_cast<check_cpu>(check::shared_from_this());
  }

  std::unique_ptr<check_cpu_detail::cpu_time_snapshot<
      check_cpu_detail::e_proc_stat_index::nb_field>>
  check_cpu::get_cpu_time_snapshot(bool first_measure) override;

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
