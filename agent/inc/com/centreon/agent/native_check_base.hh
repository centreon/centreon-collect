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

#ifndef CENTREON_AGENT_NATIVE_CHECK_MEMORY_BASE_HH
#define CENTREON_AGENT_NATIVE_CHECK_MEMORY_BASE_HH

#include "check.hh"

namespace com::centreon::agent {

namespace native_check_detail {

/**
 * @brief we store the result of a measure in this struct
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class snapshot {
 protected:
  std::array<uint64_t, nb_metric> _metrics;

 public:
  virtual ~snapshot() = default;

  uint64_t get_metric(unsigned data_index) const {
    return _metrics[data_index];
  }

  double get_proportional_value(unsigned data_index,
                                unsigned total_data_index) const {
    const uint64_t& total = _metrics[total_data_index];
    if (!total) {
      return 0.0;
    }
    return (static_cast<double>(_metrics[data_index]) / total);
  }

  virtual void dump_to_output(std::string* output) const = 0;
};

/**
 * @brief this class compare a measure with threshold and returns a plugins
 * status
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class measure_to_status {
  e_status _status;
  unsigned _data_index;
  double _threshold;
  unsigned _total_data_index;
  bool _percent;
  bool _free_threshold;

 public:
  measure_to_status(e_status status,
                    unsigned data_index,
                    double threshold,
                    unsigned total_data_index,
                    bool _percent,
                    bool free_threshold);

  virtual ~measure_to_status() = default;

  unsigned get_data_index() const { return _data_index; }
  unsigned get_total_data_index() const { return _total_data_index; }
  e_status get_status() const { return _status; }
  double get_threshold() const { return _threshold; }

  virtual void compute_status(const snapshot<nb_metric>& to_test,
                              e_status* status) const;
};

/**
 * @brief this struct will be used to create metrics
 *
 */
struct metric_definition {
  std::string_view name;
  unsigned data_index;
  unsigned total_data_index;
  bool percent;
};

}  // namespace native_check_detail

/**
 * @brief native check base (to inherit)
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class native_check_base : public check {
 protected:
  /**
   * @brief key used to store measure_to_status
   * @tparam 1 index (phys, virtual..)
   * @tparam 2 total index (phys, virtual..)
   * @tparam 3 e_status warning or critical
   *
   */
  using mem_to_status_key = std::tuple<unsigned, unsigned, e_status>;

  boost::container::flat_map<
      mem_to_status_key,
      std::unique_ptr<native_check_detail::measure_to_status<nb_metric>>>
      _measure_to_status;

  const char* _no_percent_unit = nullptr;

 public:
  native_check_base(const std::shared_ptr<asio::io_context>& io_context,
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

  std::shared_ptr<native_check_base<nb_metric>> shared_from_this() {
    return std::static_pointer_cast<native_check_base<nb_metric>>(
        check::shared_from_this());
  }

  void start_check(const duration& timeout) override;

  virtual std::shared_ptr<native_check_detail::snapshot<nb_metric>>
  measure() = 0;

  e_status compute(const native_check_detail::snapshot<nb_metric>& data,
                   std::string* output,
                   std::list<common::perfdata>* perfs) const;

  virtual const std::vector<native_check_detail::metric_definition>&
  get_metric_definitions() const = 0;
};

}  // namespace com::centreon::agent

#endif
