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

#ifndef CENTREON_AGENT_NATIVE_CHECK_MEMORY_HH
#define CENTREON_AGENT_NATIVE_CHECK_MEMORY_HH

#include "native_check_base.hh"

struct _PERFORMANCE_INFORMATION;

namespace com::centreon::agent {
namespace native_check_detail {

enum e_memory_metric : unsigned {
  phys_total,
  phys_free,
  phys_used,
  swap_total,
  swap_free,
  swap_used,
  virtual_total,
  virtual_free,
  virtual_used,
  nb_metric
};

/**
 * @brief this class compute a measure of memory metrics and store in _metrics
 * member
 *
 */
class w_memory_info
    : public snapshot<native_check_detail::e_memory_metric::nb_metric> {
  unsigned _output_flags = 0;

 public:
  enum output_flags : unsigned { dump_swap = 1, dump_virtual };

  w_memory_info(unsigned flags);
  w_memory_info(const MEMORYSTATUSEX& mem_status,
                const struct _PERFORMANCE_INFORMATION& perf_mem_status,
                unsigned flags = 0);
  void init(const MEMORYSTATUSEX& mem_status,
            const struct _PERFORMANCE_INFORMATION& perf_mem_status);

  void dump_to_output(std::string* output) const override;
};

}  // namespace native_check_detail

/**
 * @brief native final check object
 *
 */
class check_memory : public native_check_base<
                         native_check_detail::e_memory_metric::nb_metric> {
 protected:
  unsigned _output_flags = 0;

 public:
  check_memory(const std::shared_ptr<asio::io_context>& io_context,
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

  std::shared_ptr<native_check_detail::snapshot<
      native_check_detail::e_memory_metric::nb_metric>>
  measure() override;

  static void help(std::ostream& help_stream);

  const std::vector<native_check_detail::metric_definition>&
  get_metric_definitions() const override;
};

}  // namespace com::centreon::agent

#endif