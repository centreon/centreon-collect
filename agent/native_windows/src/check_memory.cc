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

#include <psapi.h>
#include <windows.h>

#include "check_memory.hh"
#include "native_check_base.cc"

using namespace com::centreon::agent;
using namespace com::centreon::agent::native_check_detail;

namespace com::centreon::agent::native_check_detail {
/**
 * @brief little struct used to format memory output (B, KB, MB or GB)
 *
 */
struct byte_memory_metric {
  uint64_t byte_value;
};
}  // namespace com::centreon::agent::native_check_detail

namespace fmt {

/**
 * @brief formatter of byte_memory_metric
 *
 * @tparam
 */
template <>
struct formatter<
    com::centreon::agent::native_check_detail::byte_memory_metric> {
  constexpr auto parse(format_parse_context& ctx)
      -> format_parse_context::iterator {
    return ctx.begin();
  }
  auto format(
      const com::centreon::agent::native_check_detail::byte_memory_metric& v,
      format_context& ctx) const -> format_context::iterator {
    if (v.byte_value < 1024) {
      return fmt::format_to(ctx.out(), "{} B", v.byte_value);
    }
    if (v.byte_value < 1024 * 1024) {
      return fmt::format_to(
          ctx.out(), "{} KB",
          static_cast<double>(v.byte_value * 100 / 1024) / 100);
    }

    if (v.byte_value < 1024 * 1024 * 1024) {
      return fmt::format_to(
          ctx.out(), "{} MB",
          static_cast<double>(v.byte_value * 100 / 1024 / 1024) / 100);
    }
    if (v.byte_value < 1024ull * 1024 * 1024 * 1024) {
      return fmt::format_to(
          ctx.out(), "{} GB",
          static_cast<double>(v.byte_value * 100 / 1024ull / 1024 / 1024) /
              100);
    }
    return fmt::format_to(
        ctx.out(), "{} TB",
        static_cast<double>(v.byte_value * 100 / 1024ull / 1024 / 1024 / 1024) /
            100);
  }
};
}  // namespace fmt

namespace com::centreon::agent::native_check_detail {

/**
 * @brief Construct a new w_memory info
 * it measures memory usage and fill _metrics
 *
 */
w_memory_info::w_memory_info(unsigned flags) : _output_flags(flags) {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!GlobalMemoryStatusEx(&mem_status)) {
    throw std::runtime_error("fail to get memory status");
  }

  PERFORMANCE_INFORMATION perf_mem_status;
  perf_mem_status.cb = sizeof(perf_mem_status);
  if (!GetPerformanceInfo(&perf_mem_status, sizeof(perf_mem_status))) {
    throw std::runtime_error("fail to get memory status");
  }

  init(mem_status, perf_mem_status);
}

/**
 * @brief mock for tests
 *
 * @param mem_status
 */
w_memory_info::w_memory_info(const MEMORYSTATUSEX& mem_status,
                             const PERFORMANCE_INFORMATION& perf_mem_status,
                             unsigned flags)
    : _output_flags(flags) {
  init(mem_status, perf_mem_status);
}

/**
 * @brief fills _metrics
 *
 * @param mem_status
 */
void w_memory_info::init(const MEMORYSTATUSEX& mem_status,
                         const PERFORMANCE_INFORMATION& perf_mem_status) {
  _metrics[e_memory_metric::phys_total] = mem_status.ullTotalPhys;
  _metrics[e_memory_metric::phys_free] = mem_status.ullAvailPhys;
  _metrics[e_memory_metric::phys_used] =
      mem_status.ullTotalPhys - mem_status.ullAvailPhys;
  _metrics[e_memory_metric::swap_total] =
      perf_mem_status.PageSize *
      (perf_mem_status.CommitLimit - perf_mem_status.PhysicalTotal);
  _metrics[e_memory_metric::swap_used] =
      perf_mem_status.PageSize *
      (perf_mem_status.CommitTotal + perf_mem_status.PhysicalAvailable -
       perf_mem_status.PhysicalTotal);
  _metrics[e_memory_metric::swap_free] = _metrics[e_memory_metric::swap_total] -
                                         _metrics[e_memory_metric::swap_used];
  _metrics[e_memory_metric::virtual_total] = mem_status.ullTotalPageFile;
  _metrics[e_memory_metric::virtual_free] = mem_status.ullAvailPageFile;
  _metrics[e_memory_metric::virtual_used] =
      _metrics[e_memory_metric::virtual_total] -
      _metrics[e_memory_metric::virtual_free];
}

/**
 * @brief plugins output
 *
 * @param output
 */
void w_memory_info::dump_to_output(std::string* output) const {
  fmt::format_to(std::back_inserter(*output),
                 "Ram total: {}, used (-buffers/cache): {} ({:.2f}%), "
                 "free: {} ({:.2f}%)",
                 byte_memory_metric{_metrics[e_memory_metric::phys_total]},
                 byte_memory_metric{_metrics[e_memory_metric::phys_used]},
                 get_proportional_value(e_memory_metric::phys_used,
                                        e_memory_metric::phys_total) *
                     100,
                 byte_memory_metric{_metrics[e_memory_metric::phys_free]},
                 get_proportional_value(e_memory_metric::phys_free,
                                        e_memory_metric::phys_total) *
                     100);

  if (_output_flags & output_flags::dump_swap) {
    fmt::format_to(std::back_inserter(*output),
                   " Swap total: {}, used: {} ({:.2f}%), free: {} ({:.2f}%)",
                   byte_memory_metric{_metrics[e_memory_metric::swap_total]},
                   byte_memory_metric{_metrics[e_memory_metric::swap_used]},
                   get_proportional_value(e_memory_metric::swap_used,
                                          e_memory_metric::swap_total) *
                       100,
                   byte_memory_metric{_metrics[e_memory_metric::swap_free]},
                   get_proportional_value(e_memory_metric::swap_free,
                                          e_memory_metric::swap_total) *
                       100);
  }

  if (_output_flags & output_flags::dump_virtual) {
    fmt::format_to(std::back_inserter(*output),
                   " Virtual total: {}, used: {} ({:.2f}%), free: {} ({:.2f}%)",
                   byte_memory_metric{_metrics[e_memory_metric::virtual_total]},
                   byte_memory_metric{_metrics[e_memory_metric::virtual_used]},
                   get_proportional_value(e_memory_metric::virtual_used,
                                          e_memory_metric::virtual_total) *
                       100,
                   byte_memory_metric{_metrics[e_memory_metric::virtual_free]},
                   get_proportional_value(e_memory_metric::virtual_free,
                                          e_memory_metric::virtual_total) *
                       100);
  }
}

}  // namespace com::centreon::agent::native_check_detail

using windows_mem_to_status = measure_to_status<e_memory_metric::nb_metric>;

using mem_to_status_constructor =
    std::function<std::unique_ptr<windows_mem_to_status>(double /*threshold*/)>;

/**
 * @brief status threshold defines
 *
 */
static const absl::flat_hash_map<std::string_view, mem_to_status_constructor>
    _label_to_mem_to_status = {
        // phys
        {"critical-usage",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::phys_used, threshold,
               e_memory_metric::phys_total, false, false);
         }},
        {"warning-usage",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::phys_used, threshold,
               e_memory_metric::phys_total, false, false);
         }},
        {"critical-usage-free",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::phys_free, threshold,
               e_memory_metric::phys_total, false, true);
         }},
        {"warning-usage-free",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::phys_free, threshold,
               e_memory_metric::phys_total, false, true);
         }},
        {"critical-usage-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::phys_used, threshold / 100,
               e_memory_metric::phys_total, true, false);
         }},
        {"warning-usage-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::phys_used, threshold / 100,
               e_memory_metric::phys_total, true, false);
         }},
        {"critical-usage-free-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::phys_free, threshold / 100,
               e_memory_metric::phys_total, true, true);
         }},
        {"warning-usage-free-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::phys_free, threshold / 100,
               e_memory_metric::phys_total, true, true);
         }},
        // swap
        {"critical-swap",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::swap_used, threshold,
               e_memory_metric::swap_total, false, false);
         }},
        {"warning-swap",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::swap_used, threshold,
               e_memory_metric::swap_total, false, false);
         }},
        {"critical-swap-free",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::swap_free, threshold,
               e_memory_metric::swap_total, false, true);
         }},
        {"warning-swap-free",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::swap_free, threshold,
               e_memory_metric::swap_total, false, true);
         }},
        {"critical-swap-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::swap_used, threshold / 100,
               e_memory_metric::swap_total, true, false);
         }},
        {"warning-swap-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::swap_used, threshold / 100,
               e_memory_metric::swap_total, true, false);
         }},
        {"critical-swap-free-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::swap_free, threshold / 100,
               e_memory_metric::swap_total, true, true);
         }},
        {"warning-swap-free-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::swap_free, threshold / 100,
               e_memory_metric::swap_total, true, true);
         }},
        // virtual memory
        {"critical-virtual",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::virtual_used, threshold,
               e_memory_metric::virtual_total, false, false);
         }},
        {"warning-virtual",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::virtual_used, threshold,
               e_memory_metric::virtual_total, false, false);
         }},
        {"critical-virtual-free",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::virtual_free, threshold,
               e_memory_metric::virtual_total, false, true);
         }},
        {"warning-virtual-free",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::virtual_free, threshold,
               e_memory_metric::virtual_total, false, true);
         }},
        {"critical-virtual-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::virtual_used,
               threshold / 100, e_memory_metric::virtual_total, true, false);
         }},
        {"warning-virtual-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::virtual_used,
               threshold / 100, e_memory_metric::virtual_total, true, false);
         }},
        {"critical-virtual-free-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::critical, e_memory_metric::virtual_free,
               threshold / 100, e_memory_metric::virtual_total, true, true);
         }},
        {"warning-virtual-free-prct",
         [](double threshold) {
           return std::make_unique<windows_mem_to_status>(
               e_status::warning, e_memory_metric::virtual_free,
               threshold / 100, e_memory_metric::virtual_total, true, true);
         }}

};

/**
 * @brief Construct a new check memory::check memory object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected
 * @param check_interval
 * @param serv
 * @param cmd_name
 * @param cmd_line
 * @param args
 * @param cnf
 * @param handler
 */
check_memory::check_memory(const std::shared_ptr<asio::io_context>& io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           time_point first_start_expected,
                           duration time_step,
                           duration check_interval,
                           const std::string& serv,
                           const std::string& cmd_name,
                           const std::string& cmd_line,
                           const rapidjson::Value& args,
                           const engine_to_agent_request_ptr& cnf,
                           check::completion_handler&& handler,
                           const checks_statistics::pointer& stat)
    : native_check_base(io_context,
                        logger,
                        first_start_expected,
                        time_step,
                        check_interval,
                        serv,
                        cmd_name,
                        cmd_line,
                        args,
                        cnf,
                        std::move(handler),
                        stat) {
  _no_percent_unit = "B";
  if (args.IsObject()) {
    for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
         ++member_iter) {
      std::string key = absl::AsciiStrToLower(member_iter->name.GetString());
      if (key == "swap") {
        std::optional<bool> val = get_bool(
            cmd_name, member_iter->name.GetString(), member_iter->value);
        if (val && *val) {
          _output_flags |= w_memory_info::output_flags::dump_swap;
        }
        continue;
      }
      if (key == "virtual") {
        std::optional<bool> val = get_bool(
            cmd_name, member_iter->name.GetString(), member_iter->value);
        if (val && *val) {
          _output_flags |= w_memory_info::output_flags::dump_virtual;
        }
        continue;
      }

      auto mem_to_status_search = _label_to_mem_to_status.find(key);
      if (mem_to_status_search != _label_to_mem_to_status.end()) {
        std::optional<double> val = get_double(
            cmd_name, member_iter->name.GetString(), member_iter->value, true);
        if (val) {
          std::unique_ptr<windows_mem_to_status> mem_checker =
              mem_to_status_search->second(*val);
          _measure_to_status.emplace(
              std::make_tuple(mem_checker->get_data_index(),
                              mem_checker->get_total_data_index(),
                              mem_checker->get_status()),
              std::move(mem_checker));
        }
      } else {
        SPDLOG_LOGGER_ERROR(logger, "command: {}, unknown parameter {}",
                            cmd_name, member_iter->name);
      }
    }
  }
}

/**
 * @brief create a w_memory_info
 *
 * @return std::shared_ptr<
 * native_check_detail::snapshot<native_check_detail::e_memory_metric::nb_metric>>
 */
std::shared_ptr<native_check_detail::snapshot<
    native_check_detail::e_memory_metric::nb_metric>>
check_memory::measure() {
  return std::make_shared<native_check_detail::w_memory_info>(_output_flags);
}

/**
 * @brief metric defines
 *
 */
static const std::vector<native_check_detail::metric_definition>
    metric_definitions = {
        {"memory.usage.bytes", e_memory_metric::phys_used,
         e_memory_metric::phys_total, false},
        {"memory.free.bytes", e_memory_metric::phys_free,
         e_memory_metric::phys_total, false},
        {"memory.usage.percentage", e_memory_metric::phys_used,
         e_memory_metric::phys_total, true},

        {"swap.usage.bytes", e_memory_metric::swap_used,
         e_memory_metric::swap_total, false},
        {"swap.free.bytes", e_memory_metric::swap_free,
         e_memory_metric::swap_total, false},
        {"swap.usage.percentage", e_memory_metric::swap_used,
         e_memory_metric::swap_total, true},

        {"virtual-memory.usage.bytes", e_memory_metric::virtual_used,
         e_memory_metric::virtual_total, false},
        {"virtual-memory.free.bytes", e_memory_metric::virtual_free,
         e_memory_metric::virtual_total, false},
        {"virtual-memory.usage.percentage", e_memory_metric::virtual_used,
         e_memory_metric::virtual_total, true},
};

const std::vector<native_check_detail::metric_definition>&
check_memory::get_metric_definitions() const {
  return metric_definitions;
}

void check_memory::help(std::ostream& help_stream) {
  help_stream << R"(
- memory params:
    swap (default false): true: add swap to output
    virtual (default false): true: add virtual memory to output
    critical-usage: threshold for critical status on physical memory usage in bytes
    warning-usage: threshold for warning status on physyical memory usage in bytes
    critical-usage-free: threshold for critical status on free physical memory in bytes, if free memory is lower than threshold, service is critical
    warning-usage-free: threshold for warning status on free physical memory in bytes
    critical-usage-prct: threshold for critical status on memory usage in percentage
    warning-usage-prct: threshold for warning status on memory usage in percentage
    critical-usage-free-prct: threshold for critical status on free memory in percentage
    warning-usage-free-prct: threshold for warning status on free memory in percentage
    critical-swap: threshold for critical status on swap usage in bytes
    warning-swap: threshold for warning status on swap usage in bytes
    critical-swap-free: threshold for critical status on free swap in bytes
    warning-swap-free: threshold for warning status on free swap in bytes
    critical-swap-prct: threshold for critical status on swap usage in percentage
    warning-swap-prct: threshold for warning status on swap usage in percentage
    critical-swap-free-prct: threshold for critical status on free swap in percentage
    warning-swap-free-prct: threshold for warning status on free swap in percentage
    critical-virtual: threshold for critical status on virtual memory usage in bytes
    warning-virtual: threshold for warning status on virtual memory usage in bytes
    critical-virtual-free: threshold for critical status on free virtual memory in bytes
    warning-virtual-free: threshold for warning status on free virtual memory in bytes
    critical-virtual-prct: threshold for critical status on virtual memory usage in percentage
    warning-virtual-prct: threshold for warning status on virtual memory usage in percentage
    critical-virtual-free-prct: threshold for critical status on free virtual memory in percentage
    warning-virtual-free-prct: threshold for warning status on free virtual memory in percentage
  An example of configuration:
  { 
    "check": "memory",
    "args: {
      "swap": true,
      "virtual": true,
      "warning-usage-prct": 80,
      "critical-usage-prct": 90
    }
  }
  Examples of output:
    OK: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), free: 7 MB (0.04%)
    With swap flag:
      OK: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), free: 7 MB (0.04%) Swap total: 44 GB, used: 4 GB (9.11%), free: 39.99 GB (90.89%)
    With swap and virtual flag:
      OK: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), free: 7 MB (0.04%) Swap total: 44 GB, used: 4 GB (9.11%), free: 39.99 GB (90.89%) Virtual total: 24 GB, used: 18 GB (75.00%), free: 6 GB (25.00%)
  Metrics:
    memory.usage.bytes
    memory.free.bytes
    memory.usage.percentage
    swap.usage.bytes
    swap.free.bytes
    swap.usage.percentage
    virtual-memory.usage.bytes
    virtual-memory.free.bytes
    virtual-memory.usage.percentage
)";
}

namespace com::centreon::agent {
template class native_check_base<
    native_check_detail::e_memory_metric::nb_metric>;
}
