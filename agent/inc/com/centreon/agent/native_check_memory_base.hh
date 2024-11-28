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

namespace check_memory_detail {

/**
 * @brief we store the result of a measure in this struct
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class memory_info {
 protected:
  std::array<uint64_t, nb_metric> _metrics;

 public:
  virtual ~memory_info() = default;

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

  virtual void dump_to_output(std::string* output, unsigned flags) const = 0;
};

/**
 * @brief this class compare a measure with threshold and returns a plugins
 * status
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class mem_to_status {
  e_status _status;
  unsigned _data_index;
  double _threshold;
  unsigned _total_data_index;
  bool _percent;
  bool _free_threshold;

 public:
  mem_to_status(e_status status,
                unsigned data_index,
                double threshold,
                unsigned total_data_index,
                bool _percent,
                bool free_threshold);

  unsigned get_data_index() const { return _data_index; }
  unsigned get_total_data_index() const { return _total_data_index; }
  e_status get_status() const { return _status; }
  double get_threshold() const { return _threshold; }

  void compute_status(const memory_info<nb_metric>& to_test,
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

/**
 * @brief this must be defined and filled in final OS implementation
 *
 */
extern const std::vector<metric_definition> metric_definitions;

}  // namespace check_memory_detail

/**
 * @brief native check base (to inherit)
 *
 * @tparam nb_metric
 */
template <unsigned nb_metric>
class check_memory_base : public check {
 protected:
  /**
   * @brief key used to store mem_to_status
   * @tparam 1 index (phys, virtual..)
   * @tparam 2 total index (phys, virtual..)
   * @tparam 3 e_status warning or critical
   *
   */
  using mem_to_status_key = std::tuple<unsigned, unsigned, e_status>;

  boost::container::flat_map<mem_to_status_key,
                             check_memory_detail::mem_to_status<nb_metric>>
      _mem_to_status;

  unsigned _output_flags = 0;

 public:
  check_memory_base(const std::shared_ptr<asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger,
                    time_point first_start_expected,
                    duration check_interval,
                    const std::string& serv,
                    const std::string& cmd_name,
                    const std::string& cmd_line,
                    const rapidjson::Value& args,
                    const engine_to_agent_request_ptr& cnf,
                    check::completion_handler&& handler);

  std::shared_ptr<check_memory_base<nb_metric>> shared_from_this() {
    return std::static_pointer_cast<check_memory_base<nb_metric>>(
        check::shared_from_this());
  }

  void start_check(const duration& timeout) override;

  virtual std::shared_ptr<check_memory_detail::memory_info<nb_metric>> measure()
      const = 0;

  e_status compute(const check_memory_detail::memory_info<nb_metric>& data,
                   std::string* output,
                   std::list<common::perfdata>* perfs) const;
};

}  // namespace com::centreon::agent

#endif
