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

#ifndef CENTREON_AGENT_NATIVE_CHECK_CPU_BASE_HH
#define CENTREON_AGENT_NATIVE_CHECK_CPU_BASE_HH

#include "check.hh"

namespace com::centreon::agent {

namespace check_cpu_detail {
// all data is indexed by processor number, this fake index points to cpus
// average
constexpr unsigned average_cpu_index = std::numeric_limits<unsigned>::max();

/**
 * @brief this class contains all counter for one core
 *
 * @tparam nb_metric number of metrics given by the kernel
 */
template <unsigned nb_metric>
class per_cpu_time_base {
 protected:
  std::array<uint64_t, nb_metric> _metrics;
  uint64_t _total_used = 0;
  uint64_t _total = 0;

 public:
  per_cpu_time_base();

  double get_proportional_value(unsigned data_index) const {
    if (!_total || data_index >= nb_metric) {
      return 0.0;
    }
    return (static_cast<double>(_metrics[data_index])) / _total;
  }

  double get_proportional_used() const {
    if (!_total) {
      return 0.0;
    }
    return (static_cast<double>(_total_used)) / _total;
  }

  /**
   * @brief Set the metric object
   *
   * @param index index of the metric like user or cpu
   * @param value
   */
  void set_metric(unsigned index, uint64_t value) {
    if (index < nb_metric) {
      _metrics[index] = value;
    }
  }

  /**
   * @brief Set the metric object and add value to the total
   *
   * @param index index of the metric like user or cpu
   * @param value
   */
  void set_metric_total(unsigned index, uint64_t value) {
    if (index < nb_metric) {
      _metrics[index] = value;
      _total += value;
    }
  }

  /**
   * @brief Set the metric object and add value to the total and total_used
   *
   * @param index index of the metric like user or cpu
   * @param value
   */
  void set_metric_total_used(unsigned index, uint64_t value) {
    if (index < nb_metric) {
      _metrics[index] = value;
      _total_used += value;
      _total += value;
    }
  }

  void set_total(uint64_t total) { _total = total; }

  void set_total_used(uint64_t total_used) { _total_used = total_used; }

  uint64_t get_total() const { return _total; }

  void dump(const unsigned& cpu_index,
            const std::string_view metric_label[],
            std::string* output) const;

  void dump_values(std::string* output) const;

  void subtract(const per_cpu_time_base& to_subtract);

  void add(const per_cpu_time_base& to_add);
};

template <unsigned nb_metric>
using index_to_cpu =
    boost::container::flat_map<unsigned, per_cpu_time_base<nb_metric>>;

/**
 * @brief contains one per_cpu_time_base per core and a total one
 *
 * @tparam nb_metric number of metrics given by the kernel
 */
template <unsigned nb_metric>
class cpu_time_snapshot {
 protected:
  index_to_cpu<nb_metric> _data;

 public:
  index_to_cpu<nb_metric> subtract(const cpu_time_snapshot& to_subtract) const;

  const index_to_cpu<nb_metric>& get_values() const { return _data; }

  void dump(std::string* output) const;
};

/**
 * @brief this little class compare cpu usages values to threshold and set
 * plugin status
 *
 */
template <unsigned nb_metric>
class cpu_to_status {
  e_status _status;
  unsigned _data_index;
  bool _average;
  double _threshold;

 public:
  cpu_to_status(e_status status,
                unsigned data_index,
                bool average,
                double threshold)
      : _status(status),
        _data_index(data_index),
        _average(average),
        _threshold(threshold) {}

  unsigned get_proc_stat_index() const { return _data_index; }
  bool is_critical() const { return _status == e_status::critical; }
  bool is_average() const { return _average; }
  double get_threshold() const { return _threshold; }
  e_status get_status() const { return _status; }

  void compute_status(
      const index_to_cpu<nb_metric>& to_test,
      boost::container::flat_map<unsigned, e_status>* per_cpu_status) const;
};

}  // namespace check_cpu_detail

/**
 * @brief native cpu check base class
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class native_check_cpu : public check {
 protected:
  unsigned _nb_core;

  /**
   * @brief key used to store cpu_to_status
   * @tparam 1 index (user, system, iowait.... and idle for all except idle)
   * @tparam 2 true if average, false if per core
   * @tparam 3 e_status warning or critical
   *
   */
  using cpu_to_status_key = std::tuple<unsigned, bool, e_status>;

  boost::container::flat_map<cpu_to_status_key,
                             check_cpu_detail::cpu_to_status<nb_metric>>
      _cpu_to_status;

  bool _cpu_detailed;

  asio::system_timer _measure_timer;

  void _measure_timer_handler(
      const boost::system::error_code& err,
      unsigned start_check_index,
      std::unique_ptr<check_cpu_detail::cpu_time_snapshot<nb_metric>>&&
          first_measure);

  e_status _compute(
      const check_cpu_detail::cpu_time_snapshot<nb_metric>& first_measure,
      const check_cpu_detail::cpu_time_snapshot<nb_metric>& second_measure,
      const std::string_view summary_labels[],
      const std::string_view perfdata_labels[],
      std::string* output,
      std::list<common::perfdata>* perfs);

 public:
  native_check_cpu(const std::shared_ptr<asio::io_context>& io_context,
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

  virtual ~native_check_cpu() = default;

  std::shared_ptr<native_check_cpu<nb_metric>> shared_from_this() {
    return std::static_pointer_cast<native_check_cpu<nb_metric>>(
        check::shared_from_this());
  }

  virtual std::unique_ptr<check_cpu_detail::cpu_time_snapshot<nb_metric>>
  get_cpu_time_snapshot(bool first_measure) = 0;

  void start_check(const duration& timeout) override;

  virtual e_status compute(
      const check_cpu_detail::cpu_time_snapshot<nb_metric>& first_measure,
      const check_cpu_detail::cpu_time_snapshot<nb_metric>& second_measure,
      std::string* output,
      std::list<common::perfdata>* perfs) = 0;
};
}  // namespace com::centreon::agent

#endif
