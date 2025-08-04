/**
 * Copyright 2025 Centreon
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

#ifndef CENTREON_AGENT_CHECK_COUNTER_HH
#define CENTREON_AGENT_CHECK_COUNTER_HH

#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>
#include <winperf.h>

#include "check.hh"
#include "filter.hh"

namespace com::centreon::agent {

struct pdh_counter {
  HQUERY query;
  HCOUNTER counter_metric;
  PDH_STATUS status;
  bool use_english = false;
  std::string unit;
  pdh_counter(std::string counter_name, bool use_english = false);
  bool need_two_samples(PDH_HCOUNTER hCounter);
  ~pdh_counter();
};

struct counter_data : public testable {
  absl::flat_hash_map<std::string, double> _map;
};

/**
 * @brief check counter
 *
 */
class check_counter : public check {
 public:
  enum class status { check_ok = 0, check_war = 1, check_crit = 2 };

 private:
  std::string _counter_name;
  std::string _output_syntax;
  std::string _detail_syntax;
  std::string _counter_filter;

  counter_data _data_counter;

  absl::btree_set<std::string> _ok_list;
  absl::btree_set<std::string> _warning_list;
  absl::btree_set<std::string> _critical_list;

  absl::flat_hash_set<std::string> _perf_filter_list;

  std::string _ok_status;
  std::string _warning_status;
  std::string _critical_status;

  unsigned _warning_threshold_count;
  unsigned _critical_threshold_count;

  bool _verbose;
  bool _use_english;
  bool _have_multi_return = false;
  bool _need_two_samples = false;
  bool _use_all_data = false;

  std::unique_ptr<pdh_counter> _pdh_counter;

  std::unique_ptr<filters::filter_combinator> _warning_rules_filter;
  std::unique_ptr<filters::filter_combinator> _critical_rules_filter;

  asio::system_timer _measure_timer;
  std::function<void(filter*)> _checker_builder;

 public:
  check_counter(const std::shared_ptr<asio::io_context>& io_context,
                const std::shared_ptr<spdlog::logger>& logger,
                time_point first_start_expected,
                duration check_interval,
                const std::string& serv,
                const std::string& cmd_name,
                const std::string& cmd_line,
                const rapidjson::Value& args,
                const engine_to_agent_request_ptr& cnf,
                check::completion_handler&& handler,
                const checks_statistics::pointer& state);

  ~check_counter();
  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  e_status compute(std::string* output,
                   std::list<com::centreon::common::perfdata>* perf);

  bool pdh_snapshot(bool first_measure);

  void _measure_timer_handler(const boost::system::error_code& err,
                              unsigned start_check_index);

  std::shared_ptr<check_counter> shared_from_this() {
    return std::static_pointer_cast<check_counter>(check::shared_from_this());
  }

  size_t get_size_data() const { return _data_counter._map.size(); }

  void build_checker();

  void _calc_output_format();
  void _calc_counter_filter(const std::string_view& param);
  void _print_counter(std::string* to_append, e_status status);

  bool have_multi_return() const { return _have_multi_return; }
  bool use_english() const { return _use_english; }
  const std::string& counter_name() const { return _counter_name; }
  bool need_two_samples() const { return _need_two_samples; }

  // only for test
  void set_counter_data(absl::flat_hash_map<std::string, double>& data) {
    _data_counter._map = data;
  }
};

}  // namespace com::centreon::agent
#endif