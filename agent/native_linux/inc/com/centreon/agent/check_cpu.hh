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
  used,  // used = user + nice + system + iowait+ irq + soft_irq + steal +
         // guest+ guest_nice
  nb_field
};

/**
 * @brief this class is the result of /proc/stat one line parsing like
 *  cpu0 2930565 15541 1250726 10453908 54490 0 27068 0 0 0
 * if _cpu_index == std::numeric_limits<unsigned>::max(), it represents the sum
 * of all cpus
 *
 */
class per_cpu_time {
  static constexpr size_t nb_field = e_proc_stat_index::nb_field;
  unsigned _data[nb_field];
  unsigned _cpu_index = 0;
  unsigned _total = 0;

 public:
  per_cpu_time(const std::string_view& line);
  per_cpu_time() {}
  unsigned get_cpu_index() const { return _cpu_index; }
  unsigned get_user() const { return _data[e_proc_stat_index::user]; }
  unsigned get_nice() const { return _data[e_proc_stat_index::nice]; }
  unsigned get_idle() const { return _data[e_proc_stat_index::idle]; }
  unsigned get_iowait() const { return _data[e_proc_stat_index::iowait]; }
  unsigned get_irq() const { return _data[e_proc_stat_index::irq]; }
  unsigned get_soft_irq() const { return _data[e_proc_stat_index::soft_irq]; }
  unsigned get_steal() const { return _data[e_proc_stat_index::steal]; }
  unsigned get_guest() const { return _data[e_proc_stat_index::guest]; }
  unsigned get_guest_nice() const {
    return _data[e_proc_stat_index::guest_nice];
  }

  unsigned get_value(e_proc_stat_index data_index) const {
    return _data[data_index];
  }

  double get_proportional_value(unsigned data_index) const {
    return (static_cast<double>(_data[data_index])) / _total;
  }

  unsigned get_total() const { return _total; }

  per_cpu_time& operator-=(const per_cpu_time& to_add);

  void dump(std::string* output) const;
};

/**
 * @brief cpu statistics index by cpu number (cpu0,1...)
 * a special index average_cpu_index is the cpus average given by first line of
 * /proc/stat
 *
 */
using index_to_cpu = boost::container::flat_map<unsigned, per_cpu_time>;

void dump(const index_to_cpu& cpus, std::string* output);

/**
 * @brief datas of /proc/stat
 *
 */
class proc_stat_file {
  index_to_cpu _values;

 public:
  proc_stat_file(size_t nb_to_reserve)
      : proc_stat_file("/proc/stat", nb_to_reserve) {}

  proc_stat_file(const char* proc_file, size_t nb_to_reserve);

  const index_to_cpu& get_values() const { return _values; }

  index_to_cpu operator-(const proc_stat_file& right) const;

  void dump(std::string* output) const;
};

/**
 * @brief this little class compare cpu usages values to threshold and set
 * plugin status
 *
 */
class cpu_to_status {
  e_status _status;
  e_proc_stat_index _data_index;
  bool _average;
  double _threshold;

 public:
  cpu_to_status(e_status status,
                e_proc_stat_index data_index,
                bool average,
                double threshold)
      : _status(status),
        _data_index(data_index),
        _average(average),
        _threshold(threshold) {}

  e_proc_stat_index get_proc_stat_index() const { return _data_index; }
  bool is_critical() const { return _status == e_status::critical; }
  bool is_average() const { return _average; }
  double get_threshold() const { return _threshold; }
  e_status get_status() const { return _status; }

  void compute_status(
      const index_to_cpu& to_test,
      boost::container::flat_map<unsigned, e_status>* per_cpu_status) const;
};

};  // namespace check_cpu_detail

/**
 * @brief native linux check_cpu
 * every _measure_interval, we read /proc/stat and we calculate cpu usage
 * when a check starts, we read last measure and passed it to completion_handler
 * If we not have yet done a measure, we wait to timeout to calculate cpu usage
 */
class check_cpu : public check {
  unsigned _nb_core;

  bool _cpu_detailed;

  /**
   * @brief key used to store cpu_to_status
   * @tparam 1 index (user, system, iowait.... and idle for all except idle)
   * @tparam 2 true if average, false if per core
   * @tparam 3 e_status warning or critical
   *
   */
  using cpu_to_status_key = std::tuple<unsigned, bool, e_status>;

  boost::container::flat_map<cpu_to_status_key, check_cpu_detail::cpu_to_status>
      _cpu_to_status;

  asio::system_timer _measure_timer;

  void _measure_timer_handler(
      const boost::system::error_code& err,
      unsigned start_check_index,
      std::unique_ptr<check_cpu_detail::proc_stat_file>&& first_measure);

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

  e_status compute(const check_cpu_detail::proc_stat_file& first_measure,
                   const check_cpu_detail::proc_stat_file& second_measure,
                   std::string* output,
                   std::list<common::perfdata>* perfs);
};
}  // namespace com::centreon::agent

#endif
