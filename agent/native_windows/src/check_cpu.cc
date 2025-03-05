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

#include <windows.h>

#include <pdh.h>
#include <pdhmsg.h>

#include "check_cpu.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "native_check_cpu_base.cc"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_cpu_detail;

/**************************************************************************
      Kernel measure method
***************************************************************************/

/**
 * @brief Construct a kernel_per_cpu_time object from a
 * SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
 *
 * @param info SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION collected by
 * NtQuerySystemInformation
 */
kernel_per_cpu_time::kernel_per_cpu_time(
    const M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION& info) {
  _metrics[e_proc_stat_index::user] = info.UserTime.QuadPart;
  _metrics[e_proc_stat_index::system] =
      info.KernelTime.QuadPart - info.IdleTime.QuadPart;
  _metrics[e_proc_stat_index::idle] = info.IdleTime.QuadPart;
  _metrics[e_proc_stat_index::interrupt] = info.InterruptTime.QuadPart;
  _metrics[e_proc_stat_index::dpc] = info.DpcTime.QuadPart;
  _total = _metrics[e_proc_stat_index::user] +
           _metrics[e_proc_stat_index::system] +
           _metrics[e_proc_stat_index::idle] +
           _metrics[e_proc_stat_index::interrupt];
  _total_used = _total - _metrics[e_proc_stat_index::idle];
}

/**
 * @brief Construct a new kernel cpu time snapshot::kernel cpu time snapshot
 * object it loads alls CPUs time and compute the average
 *
 * @param nb_core
 */
kernel_cpu_time_snapshot::kernel_cpu_time_snapshot(unsigned nb_core) {
  std::unique_ptr<M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[]> buffer(
      new M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[nb_core]);
  ULONG buffer_size =
      sizeof(M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * nb_core;
  ULONG return_length = 0;

  memset(buffer.get(), 0, buffer_size);

  if (nt_query_system_information(
          8 /*SystemProcessorPerformanceInformationClass*/
          ,
          buffer.get(), buffer_size, &return_length) != 0) {
    throw std::runtime_error("Failed to get kernel cpu time");
  }

  for (unsigned i = 0; i < nb_core; ++i) {
    _data[i] = kernel_per_cpu_time(buffer[i]);
  }
  per_cpu_time_base<e_proc_stat_index::nb_field>& total =
      _data[average_cpu_index];
  for (auto to_add_iter = _data.begin();
       to_add_iter != _data.end() && to_add_iter->first != average_cpu_index;
       ++to_add_iter) {
    total.add(to_add_iter->second);
  }
}

/**
 * @brief used for debug, dump all values
 *
 * @param output
 */
void kernel_cpu_time_snapshot::dump(std::string* output) const {
  cpu_time_snapshot<e_proc_stat_index::nb_field>::dump(output);
}

/**************************************************************************
      Pdh measure method
***************************************************************************/

namespace com::centreon::agent::check_cpu_detail {
struct pdh_counters {
  HQUERY query;
  HCOUNTER user;
  HCOUNTER idle;
  HCOUNTER kernel;
  HCOUNTER interrupt;
  HCOUNTER dpc;

  pdh_counters();

  ~pdh_counters();
};
}  // namespace com::centreon::agent::check_cpu_detail

pdh_counters::pdh_counters() : query(nullptr) {
  if (PdhOpenQuery(nullptr, 0, &query) != ERROR_SUCCESS) {
    throw std::runtime_error("Failed to open pdh query");
  }

  if (PdhAddEnglishCounterA(query, "\\Processor(*)\\% User Time", 0, &user) !=
      ERROR_SUCCESS) {
    throw std::runtime_error("Failed to add counter user");
  }

  if (PdhAddEnglishCounterA(query, "\\Processor(*)\\% Idle Time", 0, &idle) !=
      ERROR_SUCCESS) {
    throw std::runtime_error("Failed to add counter idle");
  }

  if (PdhAddEnglishCounterA(query, "\\Processor(*)\\% Privileged Time", 0,
                            &kernel) != ERROR_SUCCESS) {
    throw std::runtime_error("Failed to add counter kernel");
  }

  if (PdhAddEnglishCounterA(query, "\\Processor(*)\\% Interrupt Time", 0,
                            &interrupt) != ERROR_SUCCESS) {
    throw std::runtime_error("Failed to add counter interrupt");
  }

  if (PdhAddEnglishCounterA(query, "\\Processor(*)\\% DPC Time", 0, &dpc) !=
      ERROR_SUCCESS) {
    throw std::runtime_error("Failed to add counter dpc");
  }
}

pdh_counters::~pdh_counters() {
  if (query)
    PdhCloseQuery(query);
}

/**
 * @brief Construct a new pdh cpu time snapshot::pdh cpu time snapshot object
 * when we use pdh, we collect data twice, the first time we only collect query,
 * the second collect and get counters values
 * @param nb_core
 * @param first_measure if true, we only collect query data
 */
pdh_cpu_time_snapshot::pdh_cpu_time_snapshot(unsigned nb_core,
                                             const pdh_counters& counters,
                                             bool first_measure) {
  if (PdhCollectQueryData(counters.query) != ERROR_SUCCESS) {
    throw std::runtime_error("Failed to collect query data");
  }

  if (first_measure) {
    return;
  }

  DWORD orginal_buffer_size = 0;
  DWORD item_count = 0;
  unsigned cpu_index = 0;

  PDH_STATUS status =
      PdhGetFormattedCounterArrayA(counters.user, PDH_FMT_DOUBLE,
                                   &orginal_buffer_size, &item_count, nullptr);
  if (status != PDH_MORE_DATA) {
    throw exceptions::msg_fmt("Failed to get user pdh counter array size: {:x}",
                              static_cast<uint64_t>(status));
  }

  orginal_buffer_size =
      (orginal_buffer_size / sizeof(PDH_FMT_COUNTERVALUE_ITEM_A)) *
          sizeof(PDH_FMT_COUNTERVALUE_ITEM_A) +
      sizeof(PDH_FMT_COUNTERVALUE_ITEM_A);
  std::unique_ptr<PDH_FMT_COUNTERVALUE_ITEM_A[]> buffer(
      new PDH_FMT_COUNTERVALUE_ITEM_A[orginal_buffer_size /
                                      sizeof(PDH_FMT_COUNTERVALUE_ITEM_A)]);
  const PDH_FMT_COUNTERVALUE_ITEM_A* buffer_end = buffer.get() + nb_core + 1;

  DWORD buffer_size = orginal_buffer_size;
  if (PdhGetFormattedCounterArrayA(counters.user, PDH_FMT_DOUBLE, &buffer_size,
                                   &item_count,
                                   buffer.get()) == ERROR_SUCCESS) {
    for (const PDH_FMT_COUNTERVALUE_ITEM_A* it = buffer.get(); it < buffer_end;
         ++it) {
      if (!absl::SimpleAtoi(it->szName, &cpu_index)) {
        cpu_index = average_cpu_index;
      }
      // we multiply by 100 to store 2 decimal after comma in an integer
      _data[cpu_index].set_metric_total_used(e_proc_stat_index::user,
                                             it->FmtValue.doubleValue * 100);
    }
  }

  buffer_size = orginal_buffer_size;
  if (PdhGetFormattedCounterArrayA(counters.kernel, PDH_FMT_DOUBLE,
                                   &buffer_size, &item_count,
                                   buffer.get()) == ERROR_SUCCESS) {
    for (const PDH_FMT_COUNTERVALUE_ITEM_A* it = buffer.get(); it < buffer_end;
         ++it) {
      if (!absl::SimpleAtoi(it->szName, &cpu_index)) {
        cpu_index = average_cpu_index;
      }
      _data[cpu_index].set_metric_total_used(e_proc_stat_index::system,
                                             it->FmtValue.doubleValue * 100);
    }
  }

  buffer_size = orginal_buffer_size;
  if (PdhGetFormattedCounterArrayA(counters.idle, PDH_FMT_DOUBLE, &buffer_size,
                                   &item_count,
                                   buffer.get()) == ERROR_SUCCESS) {
    for (const PDH_FMT_COUNTERVALUE_ITEM_A* it = buffer.get(); it < buffer_end;
         ++it) {
      if (!absl::SimpleAtoi(it->szName, &cpu_index)) {
        cpu_index = average_cpu_index;
      }
      _data[cpu_index].set_metric_total(e_proc_stat_index::idle,
                                        it->FmtValue.doubleValue * 100);
    }
  }

  buffer_size = orginal_buffer_size;
  if (PdhGetFormattedCounterArrayA(counters.interrupt, PDH_FMT_DOUBLE,
                                   &buffer_size, &item_count,
                                   buffer.get()) == ERROR_SUCCESS) {
    for (const PDH_FMT_COUNTERVALUE_ITEM_A* it = buffer.get(); it < buffer_end;
         ++it) {
      if (!absl::SimpleAtoi(it->szName, &cpu_index)) {
        cpu_index = average_cpu_index;
      }
      _data[cpu_index].set_metric_total_used(e_proc_stat_index::interrupt,
                                             it->FmtValue.doubleValue * 100);
    }
  }

  buffer_size = orginal_buffer_size;
  if (PdhGetFormattedCounterArrayA(counters.dpc, PDH_FMT_DOUBLE, &buffer_size,
                                   &item_count,
                                   buffer.get()) == ERROR_SUCCESS) {
    for (const PDH_FMT_COUNTERVALUE_ITEM_A* it = buffer.get(); it < buffer_end;
         ++it) {
      if (!absl::SimpleAtoi(it->szName, &cpu_index)) {
        cpu_index = average_cpu_index;
      }
      _data[cpu_index].set_metric(e_proc_stat_index::dpc,
                                  it->FmtValue.doubleValue * 100);
    }
  }
}

/**************************************************************************
            Check cpu
***************************************************************************/
using windows_cpu_to_status = cpu_to_status<e_proc_stat_index::nb_field>;

using cpu_to_status_constructor =
    std::function<windows_cpu_to_status(double /*threshold*/)>;

#define BY_TYPE_CPU_TO_STATUS(TYPE_METRIC)                                     \
  {"warning-core-" #TYPE_METRIC,                                               \
   [](double threshold) {                                                      \
     return windows_cpu_to_status(                                             \
         e_status::warning, e_proc_stat_index::TYPE_METRIC, false, threshold); \
   }},                                                                         \
      {"critical-core-" #TYPE_METRIC,                                          \
       [](double threshold) {                                                  \
         return windows_cpu_to_status(e_status::critical,                      \
                                      e_proc_stat_index::TYPE_METRIC, false,   \
                                      threshold);                              \
       }},                                                                     \
      {"warning-average-" #TYPE_METRIC,                                        \
       [](double threshold) {                                                  \
         return windows_cpu_to_status(e_status::warning,                       \
                                      e_proc_stat_index::TYPE_METRIC, true,    \
                                      threshold);                              \
       }},                                                                     \
  {                                                                            \
    "critical-average-" #TYPE_METRIC, [](double threshold) {                   \
      return windows_cpu_to_status(e_status::critical,                         \
                                   e_proc_stat_index::TYPE_METRIC, true,       \
                                   threshold);                                 \
    }                                                                          \
  }

/**
 * @brief this map is used to generate cpus values comparator from check
 * configuration fields
 *
 */
static const absl::flat_hash_map<std::string_view, cpu_to_status_constructor>
    _label_to_cpu_to_status = {
        {"warning-core",
         [](double threshold) {
           return windows_cpu_to_status(e_status::warning,
                                        e_proc_stat_index::nb_field, false,
                                        threshold);
         }},
        {"critical-core",
         [](double threshold) {
           return windows_cpu_to_status(e_status::critical,
                                        e_proc_stat_index::nb_field, false,
                                        threshold);
         }},
        {"warning-average",
         [](double threshold) {
           return windows_cpu_to_status(
               e_status::warning, e_proc_stat_index::nb_field, true, threshold);
         }},
        {"critical-average",
         [](double threshold) {
           return windows_cpu_to_status(e_status::critical,
                                        e_proc_stat_index::nb_field, true,
                                        threshold);
         }},
        BY_TYPE_CPU_TO_STATUS(user),
        BY_TYPE_CPU_TO_STATUS(system)};

/**
 * @brief Construct a new check cpu::check cpu object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param check_interval check interval between two checks (not only this but
 * also others)
 * @param serv service
 * @param cmd_name
 * @param cmd_line
 * @param args native plugin arguments
 * @param cnf engine configuration received object
 * @param handler called at measure completion
 */
check_cpu::check_cpu(const std::shared_ptr<asio::io_context>& io_context,
                     const std::shared_ptr<spdlog::logger>& logger,
                     time_point first_start_expected,
                     duration check_interval,
                     const std::string& serv,
                     const std::string& cmd_name,
                     const std::string& cmd_line,
                     const rapidjson::Value& args,
                     const engine_to_agent_request_ptr& cnf,
                     check::completion_handler&& handler,
                     const checks_statistics::pointer& stat)
    : native_check_cpu<check_cpu_detail::e_proc_stat_index::nb_field>(
          io_context,
          logger,
          first_start_expected,
          check_interval,
          serv,
          cmd_name,
          cmd_line,
          args,
          cnf,
          std::move(handler),
          stat)

{
  try {
    if (args.IsObject()) {
      for (auto member_iter = args.MemberBegin();
           member_iter != args.MemberEnd(); ++member_iter) {
        auto cpu_to_status_search = _label_to_cpu_to_status.find(
            absl::AsciiStrToLower(member_iter->name.GetString()));
        if (cpu_to_status_search != _label_to_cpu_to_status.end()) {
          std::optional<double> threshold =
              check::get_double(cmd_name, member_iter->name.GetString(),
                                member_iter->value, true);
          if (threshold) {
            check_cpu_detail::cpu_to_status cpu_checker =
                cpu_to_status_search->second(*threshold / 100);
            _cpu_to_status.emplace(
                std::make_tuple(cpu_checker.get_proc_stat_index(),
                                cpu_checker.is_average(),
                                cpu_checker.get_status()),
                cpu_checker);
          }
        } else if (member_iter->name == "use-nt-query-system-information") {
          std::optional<bool> val = get_bool(
              cmd_name, "use-nt-query-system-information", member_iter->value);
          if (val) {
            _use_nt_query_system_information = *val;
          }
        } else if (member_iter->name != "cpu-detailed") {
          SPDLOG_LOGGER_ERROR(logger, "command: {}, unknown parameter: {}",
                              cmd_name, member_iter->name);
        }
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_cpu fail to parse check params: {}",
                        e.what());
    throw;
  }

  if (!_use_nt_query_system_information) {
    _pdh_counters = std::make_unique<pdh_counters>();
  }
}

check_cpu::~check_cpu() {}

std::unique_ptr<
    check_cpu_detail::cpu_time_snapshot<e_proc_stat_index::nb_field>>
check_cpu::get_cpu_time_snapshot(bool first_measure) {
  if (_use_nt_query_system_information) {
    return std::make_unique<check_cpu_detail::kernel_cpu_time_snapshot>(
        _nb_core);
  } else {
    return std::make_unique<check_cpu_detail::pdh_cpu_time_snapshot>(
        _nb_core, *_pdh_counters, first_measure);
  }
}

constexpr std::array<std::string_view, e_proc_stat_index::nb_field>
    _sz_summary_labels = {", User ", ", System ", ", Idle ", ", Interrupt ",
                          ", Dpc Interrupt "};

constexpr std::array<std::string_view, e_proc_stat_index::nb_field>
    _sz_perfdata_name = {"user", "system", "idle", "interrupt",
                         "dpc_interrupt"};

/**
 * @brief compute the difference between second_measure and first_measure and
 * generate status, output and perfdatas
 *
 * @param first_measure first snapshot of /proc/stat
 * @param second_measure second snapshot of /proc/stat
 * @param output out plugin output
 * @param perfs perfdatas
 * @return e_status plugin out status
 */
e_status check_cpu::compute(
    const check_cpu_detail::cpu_time_snapshot<
        check_cpu_detail::e_proc_stat_index::nb_field>& first_measure,
    const check_cpu_detail::cpu_time_snapshot<
        check_cpu_detail::e_proc_stat_index::nb_field>& second_measure,
    std::string* output,
    std::list<common::perfdata>* perfs) {
  output->reserve(256 * _nb_core);

  return _compute(first_measure, second_measure, _sz_summary_labels.data(),
                  _sz_perfdata_name.data(), output, perfs);
}

void check_cpu::help(std::ostream& help_stream) {
  help_stream << R"(
- cpu params:
    use-nt-query-system-information (default true): true: use NtQuerySystemInformation instead of performance counters
    cpu-detailed (default false): true: add detailed cpu usage metrics
    warning-core: threshold for warning status on core usage in percentage
    critical-core: threshold for critical status on core usage in percentage
    warning-average: threshold for warning status on average usage in percentage
    critical-average: threshold for critical status on average usage in percentage
    warning-core-user: threshold for warning status on core user usage in percentage
    critical-core-user: threshold for critical status on core user usage in percentage
    warning-average-user: threshold for warning status on average user usage in percentage
    critical-average-user: threshold for critical status on average user usage in percentage
    warning-core-system: threshold for warning status on core system usage in percentage
    critical-core-system: threshold for critical status on core system usage in percentage
    warning-average-system: threshold for warning status on average system usage in percentage
    critical-average-system: threshold for critical status on average system usage in percentage
  An example of configuration:
  { 
    "check": "cpu_percentage",
    "args": {
        "cpu-detailed": true,
        "warning-core": 80,
        "critical-core": 90,
        "warning-average": 60,
        "critical-average": 70
    }
  }
  Examples of output:
    OK: CPU(s) average usage is 50.00%
    WARNING: CPU'0' Usage: 40.00%, User 25.00%, System 10.00%, Idle 60.00%, Interrupt 5.00%, Dpc Interrupt 1.00% CRITICAL: CPU'1' Usage: 60.00%, User 45.00%, System 10.00%, Idle 40.00%, Interrupt 5.00%, Dpc Interrupt 0.00% WARNING: CPU(s) average Usage: 50.00%, User 35.00%, System 10.00%, Idle 50.00%, Interrupt 5.00%, Dpc Interrupt 0.50%
  Metrics:
    Normal mode:
      <core index>#core.cpu.utilization.percentage
      cpu.utilization.percentage
    cpu-detailed mode:
      <core index>~user#core.cpu.utilization.percentage
      <core index>~system#core.cpu.utilization.percentage
      <core index>~idle#core.cpu.utilization.percentage
      <core index>~interrupt#core.cpu.utilization.percentage
      <core index>~dpc_interrupt#core.cpu.utilization.percentage
      <code index>~used#core.cpu.utilization.percentage
      user#cpu.utilization.percentage
      system#cpu.utilization.percentage
      idle#cpu.utilization.percentage
      interrupt#cpu.utilization.percentage
      dpc_interrupt#cpu.utilization.percentage
)";
}
