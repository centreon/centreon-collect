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

#include "check.hh"

namespace com::centreon::agent {

namespace check_cpu_detail {

// all data is indexed by processor number, this fake index points to cpus
// average value
constexpr unsigned average_cpu_index = std::numeric_limits<unsigned>::max();

enum class e_performance_field { user = 0, kernel, dpc, interrupt, used };

struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
  LARGE_INTEGER IdleTime;
  LARGE_INTEGER KernelTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER DpcTime;
  LARGE_INTEGER InterruptTime;
  ULONG InterruptCount;
};

struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_WITH_USED
    : public SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
  uint64_t used_time;
  uint64_t total;

  double get_proportional_value(e_performance_field field) const;
};

/**
 * @brief cpu statistics index by cpu number (cpu0,1...)
 * a special index average_cpu_index is the cpus average
 *
 */
using index_to_cpu = boost::container::
    flat_map<unsigned, SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_WITH_USED>;

void dump(const index_to_cpu& cpus, std::string* output);

class by_proc_system_perf_info {
  index_to_cpu _values;

 public:
  by_proc_system_perf_info(size_t nb_core);

  const index_to_cpu& get_values() const { return _values; }

  index_to_cpu operator-(const by_proc_system_perf_info& right) const;
};

/**
 * @brief this little class compare cpu usages values to threshold and set
 * plugin status
 *
 */
class cpu_to_status {
  e_status _status;
  e_performance_field _perf_field;
  bool _average;
  double _threshold;

 public:
  cpu_to_status(e_status status,
                e_performance_field perf_field,
                bool average,
                double threshold)
      : _status(status),
        _perf_field(perf_field),
        _average(average),
        _threshold(threshold) {}

  e_performance_field get_perf_field() const { return _perf_field; }
  bool is_critical() const { return _status == e_status::critical; }
  bool is_average() const { return _average; }
  double get_threshold() const { return _threshold; }
  e_status get_status() const { return _status; }

  void compute_status(
      const index_to_cpu& to_test,
      boost::container::flat_map<unsigned, e_status>* per_cpu_status) const;
};

}  // namespace check_cpu_detail

/**
 * @brief despite the fact that microsoft advise of risk of using
 * NtQuerySystemInformation, we use it because we can obtain more details of cpu
 * usage than other official methods
 *
 */
class check_cpu : public check {
  unsigned _nb_core;

  bool _cpu_detailed;

  asio::system_timer _measure_timer;

  void _measure_timer_handler(
      const boost::system::error_code& err,
      unsigned start_check_index,
      std::unique_ptr<check_cpu_detail::by_proc_system_perf_info>&&
          first_measure);

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
            check::completion_handler&& handler);

  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  std::shared_ptr<check_cpu> shared_from_this() {
    return std::static_pointer_cast<check_cpu>(check::shared_from_this());
  }

  e_status compute(
      const check_cpu_detail::by_proc_system_perf_info& first_measure,
      const check_cpu_detail::by_proc_system_perf_info& second_measure,
      std::string* output,
      std::list<common::perfdata>* perfs);
};
}  // namespace com::centreon::agent
#endif
