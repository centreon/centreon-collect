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

#include <boost/dll/shared_library.hpp>  // for dll::import

#include "check_cpu.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_cpu_detail;

/**
 * @brief kernel function used to measure cpu usage
 *
 * @param SystemInformationClass 8 for get performance
 * @param InputBuffer
 * @param InputBufferLength
 * @param SystemInformation
 * @param SystemInformationLength
 * @param ReturnLength
 * @return typedef
 */
typedef NTSTATUS(WINAPI nt_query_system_information)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

static nt_query_system_information* _nt_query_system_information = nullptr;

static boost::dll::shared_library _ntdll_lib;

constexpr ULONG system_processor_performance_information =
    8;  // system information class used for get
        // SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION

double
SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_WITH_USED::get_proportional_value(
    e_performance_field field) const {
  if (total) {
    switch (field) {
      case e_performance_field::user:
        return static_cast<double>(UserTime.QuadPart) / total;
      case e_performance_field::kernel:
        return static_cast<double>(KernelTime.QuadPart) / total;
      case e_performance_field::dpc:
        return static_cast<double>(DpcTime.QuadPart) / total;
      case e_performance_field::interrupt:
        return static_cast<double>(InterruptTime.QuadPart) / total;
      case e_performance_field::used:
        return static_cast<double>(used_time) / total;
    }
  }
  return 0;
}

by_proc_system_perf_info::by_proc_system_perf_info(size_t nb_core) {
  _values.reserve(nb_core + 1);

  if (!_ntdll_lib.is_loaded()) {
    _ntdll_lib.load("ntdll.dll", boost::dll::load_mode::search_system_folders);
  }

  if (!_ntdll_lib.is_loaded()) {
    throw exceptions::msg_fmt("ntdll.dll not found");
  }
  if (!_nt_query_system_information) {
    _nt_query_system_information =
        _ntdll_lib.get<nt_query_system_information>("NtQuerySystemInformation");
  }
  if (!_nt_query_system_information) {
    throw exceptions::msg_fmt("NtQuerySystemInformation not found");
  }

  std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> processor_info(nb_core);
  NTSTATUS status = (*_nt_query_system_information)(
      system_processor_performance_information, processor_info.data(),
      sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * nb_core, nullptr);
  if (status != 0) {
    throw exceptions::msg_fmt("NtQuerySystemInformation failed with status {}",
                              status);
  }

  for (size_t ii = 0; ii < nb_core; ++ii) {
    auto& val = _values[ii];
    val = processor_info[ii];
    val.used_time = val.UserTime.QuadPart + val.KernelTime.QuadPart +
                    val.DpcTime.QuadPart + val.InterruptTime.QuadPart;
    val.total = val.used_time + val.IdleTime.QuadPart;
  }

  auto& average = _values[average_cpu_index];
  memset(&average, 0,
         sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_WITH_USED));
  for (size_t ii = 0; ii < nb_core; ++ii) {
    const auto& per_core = _values[ii];
    average.IdleTime.QuadPart += per_core.IdleTime.QuadPart;
    average.KernelTime.QuadPart += per_core.KernelTime.QuadPart;
    average.UserTime.QuadPart += per_core.UserTime.QuadPart;
    average.DpcTime.QuadPart += per_core.DpcTime.QuadPart;
    average.InterruptTime.QuadPart += per_core.InterruptTime.QuadPart;
    average.InterruptCount += per_core.InterruptCount;
    average.used_time += per_core.used_time;
    average.total += per_core.total;
  }
}

index_to_cpu by_proc_system_perf_info::operator-(
    const by_proc_system_perf_info& right) const {
  index_to_cpu result;
  for (const auto& left_it : _values) {
    const auto& right_it = right._values.find(left_it.first);
    if (right_it == right._values.end()) {
      continue;
    }
    SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_WITH_USED& res =
        result[left_it.first];
    res.IdleTime.QuadPart =
        left_it.second.IdleTime.QuadPart - right_it->second.IdleTime.QuadPart;
    res.KernelTime.QuadPart = left_it.second.KernelTime.QuadPart -
                              right_it->second.KernelTime.QuadPart;
    res.UserTime.QuadPart =
        left_it.second.UserTime.QuadPart - right_it->second.UserTime.QuadPart;
    res.DpcTime.QuadPart =
        left_it.second.DpcTime.QuadPart - right_it->second.DpcTime.QuadPart;
    res.InterruptTime.QuadPart = left_it.second.InterruptTime.QuadPart -
                                 right_it->second.InterruptTime.QuadPart;
    res.InterruptCount =
        left_it.second.InterruptCount - right_it->second.InterruptCount;
    res.used_time = left_it.second.used_time - right_it->second.used_time;
    res.total = left_it.second.total - right_it->second.total;
  }
  return result;
}

/**
 * @brief compare cpu values to a threshold and update cpu status if field value
 * > threshold
 *
 * @param to_test cpus usage to compare
 * @param per_cpu_status out parameter that contains per cpu worst status
 */
void cpu_to_status::compute_status(
    const index_to_cpu& to_test,
    boost::container::flat_map<unsigned, e_status>* per_cpu_status) const {
  auto check_threshold = [&, this](const index_to_cpu::value_type& values) {
    double val = values.second.get_proportional_value(_perf_field);
    if (val > _threshold) {
      auto& to_update = (*per_cpu_status)[values.first];
      // if ok (=0) and _status is warning (=1) or critical(=2), we update
      if (_status > to_update) {
        to_update = _status;
      }
    }
  };

  if (_average) {
    index_to_cpu::const_iterator avg =
        to_test.find(check_cpu_detail::average_cpu_index);
    if (avg == to_test.end()) {
      return;
    }
    check_threshold(*avg);
  } else {
    for (const auto& by_cpu : to_test) {
      if (by_cpu.first == check_cpu_detail::average_cpu_index) {
        continue;
      }
      check_threshold(by_cpu);
    }
  }
}

check_cpu::check_cpu(const std::shared_ptr<asio::io_context>& io_context,
                     const std::shared_ptr<spdlog::logger>& logger,
                     time_point first_start_expected,
                     duration check_interval,
                     const std::string& serv,
                     const std::string& cmd_name,
                     const std::string& cmd_line,
                     const rapidjson::Value& args,
                     const engine_to_agent_request_ptr& cnf,
                     check::completion_handler&& handler)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler)),
      _nb_core(std::thread::hardware_concurrency()),
      _cpu_detailed(false),
      _measure_timer(*io_context) {
  com::centreon::common::rapidjson_helper arg(args);

  _cpu_detailed = arg.get_bool("detailed", false);
}

void check_cpu::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  std::unique_ptr<by_proc_system_perf_info> begin =
      std::make_unique<by_proc_system_perf_info>(_nb_core);

  time_point end_measure = std::chrono::system_clock::now() + timeout;
  time_point end_measure_period =
      get_start_expected() +
      std::chrono::seconds(get_conf()->config().check_interval());

  if (end_measure > end_measure_period) {
    end_measure = end_measure_period;
  }

  end_measure -= std::chrono::seconds(1);

  _measure_timer.expires_at(end_measure);
  _measure_timer.async_wait([me = shared_from_this(),
                             first_measure = std::move(begin),
                             start_check_index = _get_running_check_index()](
                                const boost::system::error_code& err) mutable {
    me->_measure_timer_handler(err, start_check_index,
                               std::move(first_measure));
  });
}

/**
 * @brief called at measure timer expiration
 * Then we take a new snapshot of /proc/stat, compute difference with
 * first_measure and generate output and perfdatas
 *
 * @param err asio error
 * @param start_check_index passed to on_completion to validate result
 * @param first_measure first snapshot to compare
 */
void check_cpu::_measure_timer_handler(
    const boost::system::error_code& err,
    unsigned start_check_index,
    std::unique_ptr<check_cpu_detail::by_proc_system_perf_info>&&
        first_measure) {
  if (err) {
    return;
  }

  std::string output;
  std::list<common::perfdata> perfs;

  by_proc_system_perf_info new_measure(_nb_core);

  e_status worst = compute(*first_measure, new_measure, &output, &perfs);

  on_completion(start_check_index, worst, perfs, {output});
}
