/**
 * Copyright 2026 Centreon
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

#ifndef CENTREON_AGENT_CHECK_NETWORK_HH
#define CENTREON_AGENT_CHECK_NETWORK_HH

#include <iphlpapi.h>

#include <span>

#include "check.hh"
#include "spdlog/logger.h"

namespace com::centreon::agent {

namespace check_network_detail {

/**
 * @brief metrics computed for each interface, used as index of thresholds
 * and metric definitions
 */
enum e_metric : unsigned {
  traffic_in,
  traffic_out,
  traffic_in_prct,
  traffic_out_prct,
  errors_in,
  errors_out,
  discards_in,
  discards_out,
  nb_metric
};

/**
 * @brief state and raw counters of one interface at snapshot time
 */
struct interface_counters {
  std::string name;
  std::string description;
  IF_OPER_STATUS status = IfOperStatusUnknown;
  // link speeds in bits/s, 0 when unknown
  uint64_t speed_in = 0;
  uint64_t speed_out = 0;
  uint64_t in_octets = 0;
  uint64_t out_octets = 0;
  uint64_t in_errors = 0;
  uint64_t out_errors = 0;
  uint64_t in_discards = 0;
  uint64_t out_discards = 0;
};

/**
 * @brief selects the interfaces to check
 * Regexes match the whole UTF-8 name or description, case insensitive. A
 * null regex is not applied.
 */
struct interface_filter {
  std::unique_ptr<re2::RE2> name_filter;
  std::unique_ptr<re2::RE2> name_exclude;
  std::unique_ptr<re2::RE2> description_filter;
  std::unique_ptr<re2::RE2> description_exclude;
  // only interfaces with a connector (network adapters, physical or exposed
  // by a hypervisor), as Get-NetAdapter -Physical
  bool physical_only = true;

  bool is_allowed(const MIB_IF_ROW2& row,
                  std::string_view name,
                  std::string_view description) const;
};

/**
 * @brief counters of the interfaces allowed by the filter at a given time
 */
class network_snapshot {
 public:
  // the LUID identifies an interface as long as it exists, unlike its index
  using interface_map = absl::flat_hash_map<uint64_t, interface_counters>;

 private:
  std::chrono::steady_clock::time_point _time;
  // rows of the interface table, allowed by the filter or not
  size_t _nb_rows;
  interface_map _interfaces;

 public:
  network_snapshot(std::chrono::steady_clock::time_point time,
                   std::span<const MIB_IF_ROW2> rows,
                   const interface_filter& filter,
                   const std::shared_ptr<spdlog::logger>& logger);

  std::chrono::steady_clock::time_point get_time() const { return _time; }
  size_t get_nb_rows() const { return _nb_rows; }
  const interface_map& get_interfaces() const { return _interfaces; }
};

}  // namespace check_network_detail

/**
 * @brief network interfaces check: traffic, utilization, errors, discards and
 * operational status of each interface, measured over two snapshots
 */
class check_network : public check {
  check_network_detail::interface_filter _filter;

  std::array<std::optional<common::threshold>, check_network_detail::nb_metric>
      _warning;
  std::array<std::optional<common::threshold>, check_network_detail::nb_metric>
      _critical;

  // bit masks of 1 << IF_OPER_STATUS
  unsigned _status_to_warning = 0;
  unsigned _status_to_critical = 0;

  asio::system_timer _measure_timer;

  std::unique_ptr<re2::RE2> _compile_regex(const std::string& arg_name,
                                           const rapidjson::Value& val) const;

  unsigned _parse_states(const std::string& arg_name,
                         const rapidjson::Value& val) const;

  bool _parse_threshold(const std::string& arg_name,
                        const rapidjson::Value& val);

  void _measure_timer_handler(
      const boost::system::error_code& err,
      unsigned start_check_index,
      const check_network_detail::network_snapshot& first_measure);

 protected:
  void _on_timeout() override;

 public:
  check_network(const std::shared_ptr<asio::io_context>& io_context,
                const std::shared_ptr<spdlog::logger>& logger,
                time_point first_start_expected,
                const Service& serv,
                const rapidjson::Value& args,
                const engine_to_agent_request_ptr& cnf,
                check::completion_handler&& handler,
                const checks_statistics::pointer& stat);

  std::shared_ptr<check_network> shared_from_this() {
    return std::static_pointer_cast<check_network>(check::shared_from_this());
  }

  const check_network_detail::interface_filter& get_filter() const {
    return _filter;
  }

  /**
   * @brief takes a first snapshot, a second one at the end of the measure
   * window and completes the check with the difference
   */
  void start_check(const duration& timeout) override;

  /**
   * @brief reads the interface table of the host (virtual so that tests can
   * feed fabricated snapshots)
   * @throw exceptions::msg_fmt if the table cannot be read
   */
  virtual std::unique_ptr<check_network_detail::network_snapshot> measure();

  /**
   * @brief computes rates between two snapshots, status, output and perfdata
   *
   * @param first_measure snapshot taken at the start of the measure
   * @param second_measure snapshot taken at the end of the measure
   * @param output plugin output
   * @param perfs perfdata
   * @return e_status worst status of the interfaces
   */
  e_status compute(const check_network_detail::network_snapshot& first_measure,
                   const check_network_detail::network_snapshot& second_measure,
                   std::string* output,
                   std::list<common::perfdata>* perfs) const;

  static void help(std::ostream& help_stream);
};

}  // namespace com::centreon::agent

#endif
