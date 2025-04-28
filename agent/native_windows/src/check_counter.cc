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

#include "check_counter.hh"
#include "check.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;

/*
 *
 * @brief Convert a string to a wide string (UTF-16).
 *
 * @param string The input string (UTF-8).
 * @return The converted wide string (UTF-16).
 */
std::wstring string_to_wide_string(const std::string& string) {
  if (string.empty()) {
    return L"";
  }

  const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(),
                                               (int)string.size(), nullptr, 0);
  if (size_needed <= 0) {
    throw std::runtime_error("MultiByteToWideChar() failed: " +
                             std::to_string(size_needed));
  }

  std::wstring result(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(),
                      result.data(), size_needed);
  return result;
}

/**
 * @brief Construct a new check counter::check counter object
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
check_counter::check_counter(
    const std::shared_ptr<asio::io_context>& io_context,
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
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat),
      _measure_timer(*io_context) {
  com::centreon::common::rapidjson_helper arg(args);
  // check if the arguments are valid
  try {
    if (args.IsObject()) {
      _counter_name = arg.get_string("counter", "");
      // format the output
      _calc_output_format(
          arg.get_string("output-syntax", "{status}: {label} : {value}"));

      // format the output for the perfdata
      _calc_pref_display(arg.get_string("pref-display-syntax", ""));

      _warning_status = arg.get_string("warning-status", "");
      _critical_status = arg.get_string("critical-status", "");
      _warning_threshold_count = arg.get_int("warning-count", 1);
      _critical_threshold_count = arg.get_int("critical-count", 1);
      _verbose = arg.get_bool("verbose", false);
      _use_english = arg.get_bool("use_english", false);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_counter, fail to parse arguments: {}",
                        e.what());
    throw;
  }

  if (_counter_name.empty()) {
    SPDLOG_LOGGER_ERROR(_logger, "check_counter, counter name is empty");
    throw std::runtime_error("counter name is empty");
  }
  // if counter name has (*) it means that we have to use the multi-instance
  if (_counter_name.find("(*)") != std::string::npos) {
    _have_multi_return = true;
  }
  // create the counter
  _pdh_counter = std::make_unique<::pdh_counter>(_counter_name, _use_english);

  // check if the counter need two samples
  _need_two_samples =
      _pdh_counter->need_two_samples(_pdh_counter->counter_metric);

  // build the checker for the warning and critical thresholds
  build_checker();
}

/**
 * @brief Destructor for the check_counter class.
 * Cleans up the resources used by the check_counter instance.
 */
check_counter::~check_counter() {}

/**************************************************************************
      Pdh measure method
***************************************************************************/
/**
 * @brief Create a new counter object.
 *
 * @param counter_name The name of the counter to be created.
 * @param use_english Flag indicating whether to use English for the counter.
 * @throws std::runtime_error if the counter creation fails.
 */
::pdh_counter::pdh_counter(std::string counter_name, bool use_english)
    : query(nullptr), use_english(use_english) {
  // open the query
  if (PdhOpenQuery(nullptr, 0, &query) != ERROR_SUCCESS) {
    throw std::runtime_error("Failed to open pdh query for counter: " +
                             counter_name);
  }
  // for english, we use the PdhAddEnglishCounterA function
  // for other language, we use the PdhAddCounterW function
  if (use_english) {
    if (PdhAddEnglishCounterA(query, counter_name.c_str(), 0,
                              &counter_metric) != ERROR_SUCCESS) {
      throw std::runtime_error("Failed to add counter: " + counter_name);
    }
  } else {
    // convert to widestring :: needed for language with é, è, ê,â etc.
    std::wstring wide_counter_name = string_to_wide_string(counter_name);
    if (PdhAddCounterW(query, wide_counter_name.c_str(), 0, &counter_metric) !=
        ERROR_SUCCESS) {
      throw std::runtime_error("Failed to add counterW: " + counter_name);
    }
  }
}

/**
 * @brief Check if the counter needs two samples.
 *
 * @param hCounter The handle to the counter.
 * @return true if two samples are needed, false otherwise.
 */
bool ::pdh_counter::need_two_samples(PDH_HCOUNTER hCounter) {
  DWORD size = 0;
  // First call just asks for required buffer size
  PDH_STATUS s = PdhGetCounterInfo(hCounter, FALSE, &size, nullptr);
  if (s != PDH_MORE_DATA)  // should always be PDH_MORE_DATA
    throw std::runtime_error("PdhGetCounterInfo failed : cannot get size");

  // Allocate the buffer for the counter info.
  std::vector<BYTE> buf(size);
  auto info = reinterpret_cast<PDH_COUNTER_INFO*>(buf.data());
  s = PdhGetCounterInfo(hCounter, FALSE, &size, info);
  if (s != ERROR_SUCCESS)
    throw std::runtime_error("PdhGetCounterInfo failed : cannot get info");

  // get the unit of the counter
  switch (info->dwType & 0xF0000000) {
    case PERF_DISPLAY_PER_SEC:
      unit = "/sec";
      break;
    case PERF_DISPLAY_PERCENT:
      unit = "%";
      break;
    case PERF_DISPLAY_SECONDS:
      unit = "s";
      break;
    default:
      unit = "";
      break;
  }
  // Mask to check if the counter is a delta counter or a base counter. for both
  // case we need two samples
  constexpr DWORD deltaMask = PERF_DELTA_COUNTER  // 0x00400000
                              | PERF_DELTA_BASE;

  return (info->dwType & deltaMask) != 0;
}

/*
 * @brief Destructor for the pdh_counter class.
 * Closes the PDH query if it is open.
 */
::pdh_counter::~pdh_counter() {
  if (query)
    PdhCloseQuery(query);
}

/**
 * @brief Collects data from the PDH counter.
 *
 * @param first_measure Indicates if this is the first measurement.
 *
 * @throws std::runtime_error if the collection fails.
 */
void check_counter::pdh_snapshot(bool first_measure) {
  // collect the data from the query.
  PDH_STATUS status;
  status = PdhCollectQueryData(_pdh_counter->query);
  if (status != ERROR_SUCCESS && status != PDH_NO_DATA) {
    SPDLOG_LOGGER_ERROR(
        _logger,
        "Failed to collect query data : PdhCollectQueryData failed: 0x{:08X}",
        status);
    throw std::runtime_error("Failed to collect query data");
  }

  _is_first_measure = !first_measure;
  // if we need two samples , we need to wait for the second one
  if (_need_two_samples && first_measure)
    return;

  // if the counter have multiple return value
  if (_have_multi_return) {
    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_FMT_COUNTERVALUE_ITEM* pItems = NULL;

    // Get the size of the buffer needed for the counter array.
    status = PdhGetFormattedCounterArray(_pdh_counter->counter_metric,
                                         PDH_FMT_DOUBLE, &bufferSize,
                                         &itemCount, nullptr);
    if (status != PDH_MORE_DATA) {
      throw std::runtime_error("Failed to get buffer size for counter array");
    }

    // Allocate the buffer for the counter array.
    pItems = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    if (pItems) {
      status = PdhGetFormattedCounterArray(_pdh_counter->counter_metric,
                                           PDH_FMT_DOUBLE, &bufferSize,
                                           &itemCount, pItems);
      if (ERROR_SUCCESS == status) {
        // Loop through the array and store the instance name and counter
        // value.
        for (DWORD i = 0; i < itemCount; i++) {
          // transform the instance name to lower case
          std::string key = pItems[i].szName;
          std::ranges::transform(key, key.begin(), ::tolower);
          _data.values.emplace(std::move(key), pItems[i].FmtValue.doubleValue);
        }
      } else {
        // if we have an error, we need to free the buffer and throw an error
        free(pItems);
        throw std::runtime_error(
            "Failed to get formatted counter array: 0x" +
            fmt::format("{:08X}", static_cast<uint64_t>(status)));
      }
    } else {
      // if we have an error, we need to free the buffer and throw an error
      free(pItems);
      throw std::runtime_error("Failed to allocate memory for counter array");
    }
    // free the buffer after use
    free(pItems);
  } else {
    // single-instance counter
    PDH_FMT_COUNTERVALUE counterValue;
    status = PdhGetFormattedCounterValue(
        _pdh_counter->counter_metric, PDH_FMT_DOUBLE, nullptr, &counterValue);
    if (status != ERROR_SUCCESS) {
      throw std::runtime_error(
          "Failed to get formatted counter value: 0x" +
          fmt::format("{:08X}", static_cast<uint64_t>(status)));
    }
    _data.values[_counter_name] = counterValue.doubleValue;
  }
}

/**
 * @brief Compute the status of the check based on the collected data.
 *
 * @param output The output string to be filled with the result.
 * @param perf The list of performance data to be filled.
 * @return The status of the check (OK, WARNING, CRITICAL).
 */
e_status check_counter::compute(
    std::string* output,
    std::list<com::centreon::common::perfdata>* perf) {
  e_status ret = e_status::ok;

  if (_data.values.empty()) {
    SPDLOG_LOGGER_ERROR(_logger, "No data collected from the counter");
    throw std::runtime_error("No data collected from the counter");
  }

  unsigned int count = 0;
  unsigned int warning_count = 0;
  unsigned int critical_count = 0;

  std::string prefix = "";
  std::string output_filter = "";

  std::string ok_output = "";
  std::string warning_output = "";
  std::string critical_output = "";

  // format the output string
  std::string verbose_output = "";

  std::string unit = _pdh_counter->unit;

  bool is_warning = false;
  bool is_critical = false;

  // check if we have a warning or critical status
  if (!_warning_status.empty()) {
    _warning_rules_filter->clear_failures();
    is_warning = _warning_rules_filter->check(_data);
  }

  if (!_critical_status.empty()) {
    _critical_rules_filter->clear_failures();
    is_critical = _critical_rules_filter->check(_data);
  }
  // if we have a warning or critical status, we need to visit the filter and
  // get the output
  visitor v = [this, &output_filter, &prefix, &unit, &count](const filter* f) {
    for (const auto& [label, value] : f->failures()) {
      // we don t have to add output if we are in verbose mode

      output_filter += std::vformat(
          _output_syntax, std::make_format_args(prefix, label, value, unit));
      output_filter += "\n";

      count++;
    }
  };

  if (is_warning && !is_critical) {
    prefix = "WARNING";
    // visit the filter and get the output
    _warning_rules_filter->visit(v);
    warning_count = count;
    warning_output = output_filter;
  }

  if (is_critical) {
    // clear the output filter and count to clear the previous values
    output_filter.clear();
    count = 0;
    prefix = "CRITICAL";
    // visit the filter and get the output
    _critical_rules_filter->visit(v);
    critical_count = count;
    critical_output = output_filter;
  }

  if (!_have_multi_return) {
    // for single-instance counter, we need to add the value of the counter
    if (!is_warning && !is_critical && !_verbose) {
      ok_output = std::vformat(
          _output_syntax,
          std::make_format_args("OK", _data.values.begin()->first,
                                _data.values.begin()->second, unit));
    }
    // if we have only one instance, we need to add the value of the counter to
    // the perfdata
    common::perfdata perfdata;
    perfdata.name(_counter_name);
    perfdata.value(_data.values.begin()->second);
    perfdata.unit(std::string(unit));
    perf->emplace_back(std::move(perfdata));
  } else {
    // for multi-instance counter
    if (_pref_filter_list.empty() &&
        (!is_warning && !is_critical && !_verbose)) {
      ok_output = fmt::format("OK: {} \n", _counter_name);
    } else {
      // if we have a filter, we need to add the value of the counter to the
      // perfdata
      if (_pref_filter_list.size() == 1 &&
          _pref_filter_list.find("any") != _pref_filter_list.end()) {
        for (const auto& [label, value] : _data.values) {
          if (!_verbose) {
            ok_output +=
                std::vformat(_output_syntax,
                             std::make_format_args("OK", label, value, unit));
            ok_output += "\n";
          }
          // add the value of the counter to the perfdata
          common::perfdata pref;
          pref.name(label);
          pref.value(value);
          pref.unit(std::string(unit));
          perf->emplace_back(std::move(pref));
        }
      } else {
        for (const auto& label : _pref_filter_list) {
          auto it = _data.values.find(label);
          if (it != _data.values.end()) {
            if (!is_warning && !is_critical && !_verbose) {
              ok_output += std::vformat(
                  _output_syntax,
                  std::make_format_args("OK", label, it->second, unit));
              ok_output += "\n";
            }
            // add the value of the counter to the perfdata
            common::perfdata pref;
            pref.name(label);
            pref.value(it->second);
            pref.unit(std::string(unit));
            perf->emplace_back(std::move(pref));
          }
        }
      }
    }

    common::perfdata warning;
    warning.name("warning-count");
    warning.value(warning_count);
    perf->emplace_back(std::move(warning));

    common::perfdata critical;
    critical.name("critical-count");
    critical.value(critical_count);
    perf->emplace_back(std::move(critical));
  }

  // check the status
  if (is_critical && critical_count >= _critical_threshold_count) {
    ret = e_status::critical;
    *output = critical_output;
  } else if (is_warning && warning_count >= _warning_threshold_count) {
    ret = e_status::warning;
    *output = warning_output;
  } else {
    ret = e_status::ok;
    *output = ok_output;
  }

  // if verbose is enabled
  if (_verbose) {
    for (const auto& [label, value] : _data.values) {
      verbose_output += std::vformat(
          _output_syntax, std::make_format_args("", label, value, unit));
      verbose_output += "\n";
    }
    *output = verbose_output;
  }

  // clear the data to free the memory
  _data.values.clear();
  return ret;
}

/**
 * * @brief Start the check process.
 *
 * @param timeout The timeout duration for the check.
 * @return void
 */
void check_counter::start_check(const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }

  std::string output;
  std::list<common::perfdata> perf;
  try {
    // take the first snapshot of the counter
    pdh_snapshot(true);
    if (!_need_two_samples) {
      // if we don't need two samples, we can compute the result
      e_status status = compute(&output, &perf);
      asio::post(*_io_context, [me = shared_from_this(), this,
                                out = std::move(output), status,
                                performance = std::move(perf)]() {
        on_completion(_get_running_check_index(), status, performance, {out});
      });
    } else {
      // if we need two samples, we need to wait for the second one
      time_point end_measure = std::chrono::system_clock::now() + timeout;
      end_measure -= std::chrono::seconds(1);

      _measure_timer.expires_at(end_measure);
      _measure_timer.async_wait(
          [me = shared_from_this(),
           start_check_index = _get_running_check_index()](
              const boost::system::error_code& err) mutable {
            me->_measure_timer_handler(err, start_check_index);
          });
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "{} fail to start check: {}",
                        get_command_name(), e.what());
    asio::post(*_io_context, [me = shared_from_this(),
                              start_check_index = _get_running_check_index(),
                              err = std::string(e.what())] {
      me->on_completion(start_check_index, e_status::unknown, {}, {err});
    });
  }
}

/**
 * @brief Handle the timer event for the measurement.
 *
 * @param err The error code from the timer event.
 * @param start_check_index The index of the check that started the timer.
 * @return void
 */
void check_counter::_measure_timer_handler(const boost::system::error_code& err,
                                           unsigned start_check_index) {
  if (err) {
    return;
  }
  pdh_snapshot(false);
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  e_status status = compute(&output, &perfs);
  on_completion(start_check_index, status, perfs, {output});
}

/**
 * @brief Display help information for the check_counter command.
 *
 * @param help_stream The stream to write the help information to.
 * @return void
 */
void check_counter::help(std::ostream& help_stream) {
  help_stream << "check_counter: execute a Performance Data Helper (PDH) check "
                 "on a Windows counter"
              << std::endl;
  help_stream << std::endl;
  help_stream << "Input (JSON):" << std::endl;
  help_stream << "  {" << std::endl;
  help_stream
      << "    \"counter\": string,              // PDH counter name (required)"
      << std::endl;
  help_stream << "    \"output-syntax\": string,        // Output format "
                 "(optional, default '{status} {label}: {value}')"
              << std::endl;
  help_stream << "    \"pref-display-syntax\": string,  // Instance labels for "
                 "perfdata, comma-separated (optional)"
              << std::endl;
  help_stream << "    \"warning-status\": string,       // Filter expression "
                 "for WARNING (optional)"
              << std::endl;
  help_stream << "    \"critical-status\": string,      // Filter expression "
                 "for CRITICAL (optional)"
              << std::endl;
  help_stream << "    \"warning-count\": integer,       // Min instances to "
                 "trigger WARNING (optional, default 1)"
              << std::endl;
  help_stream << "    \"critical-count\": integer,      // Min instances to "
                 "trigger CRITICAL (optional, default 1)"
              << std::endl;
  help_stream << "    \"verbose\": bool,                // Always output all "
                 "values if true (optional, default false)"
              << std::endl;
  help_stream << "    \"use_english\": bool              // Use English "
                 "counter names if true (optional, default false)"
              << std::endl;
  help_stream << "  }" << std::endl;
  help_stream << std::endl;
  help_stream << "Example JSON:" << std::endl;
  help_stream << "  {" << std::endl;
  help_stream << "    \"counter\": \"\\Processor(*)\\% Processor Time\","
              << std::endl;
  help_stream << "    \"pref-display-syntax\": _total," << std::endl;
  help_stream << "    \"warning-status\": \"value>75\"," << std::endl;
  help_stream << "    \"critical-status\": \"value>90\"" << std::endl;
  help_stream << "  }" << std::endl;
}

/**
 * @brief Build the checker and the filter for the warning and critical
 * thresholds
 *
 * @return void
 */
void check_counter::build_checker() {
  // create the checker for the filters
  _checker_builder = [](filter* f) {
    switch (f->get_type()) {
      case filter::filter_type::label_compare_to_value: {
        filters::label_compare_to_value* filt =
            static_cast<filters::label_compare_to_value*>(f);
        double threshold = filt->get_value();
        const std::string& label = filt->get_label();
        auto comp = filt->get_comparison();

        if (label == "any" || label == "value") {
          filt->set_checker([threshold, comp, filt](const testable& t) -> bool {
            const auto& data = static_cast<const counter_data&>(t);
            for (auto const& [key, value] : data.values) {
              bool ok = false;
              switch (comp) {
                case filters::label_compare_to_value::comparison::greater_than:
                  ok = (value > threshold);
                  break;
                case filters::label_compare_to_value::comparison::
                    greater_than_or_equal:
                  ok = (value >= threshold);
                  break;
                case filters::label_compare_to_value::comparison::less_than:
                  ok = (value < threshold);
                  break;
                case filters::label_compare_to_value::comparison::
                    less_than_or_equal:
                  ok = (value <= threshold);
                  break;
                case filters::label_compare_to_value::comparison::equal:
                  ok = (value == threshold);
                  break;

                default:
                  // case we don't know what operation to do, we don't record
                  // the failure
                  ok = false;
                  break;
              }
              // record the failure if the condition is met
              if (ok) {
                filt->record_failure(key, value);
              }
            }
            // if we don't have any failure, we return false
            return !filt->failures().empty();
          });

        } else {
          // for specific label, we use the getter
          filt->set_checker_from_getter(
              [label, threshold](const testable& t) -> double {
                auto const& tt = static_cast<const counter_data&>(t);
                auto it = tt.values.find(label);
                if (it == tt.values.end())
                  return 0.0;
                return it->second;
              });
        }
        break;
      }
      default:
        break;
    }
  };

  // create the filter for the warning
  if (!_warning_status.empty()) {
    _warning_rules_filter = std::make_unique<filters::filter_combinator>();
    if (!filter::create_filter(_warning_status, _logger,
                               _warning_rules_filter.get())) {
      throw std::runtime_error("Failed to create filter for warning status");
    }
    _warning_rules_filter->apply_checker(_checker_builder);
  }

  // create the filter for the critical
  if (!_critical_status.empty()) {
    _critical_rules_filter = std::make_unique<filters::filter_combinator>();

    if (!filter::create_filter(_critical_status, _logger,
                               _critical_rules_filter.get())) {
      throw std::runtime_error("Failed to create filter for critical status");
    }

    _critical_rules_filter->apply_checker(_checker_builder);
  }
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 8>
    _label_to_event_index{{{"${status}", "{0}"},
                           {"{status}", "{0}"},
                           {"${label}", "{1}"},
                           {"{label}", "{1}"},
                           {"${alias}", "{1}"},
                           {"{alias}", "{1}"},
                           {"{value}", "{2:.2f} {3}"},
                           {"${value}", "{2:.2f} {3}"}}};

/**
 * @brief Calculate the output format for the check counter.
 *
 * @param param The output format string.
 * @return void
 */
void check_counter::_calc_output_format(const std::string_view& param) {
  _output_syntax = param;
  for (const auto& translate : _label_to_event_index) {
    boost::replace_all(_output_syntax, translate.first, translate.second);
  }
}

/**
 * @brief Calculate the preferred display format for the check counter.
 *
 * @param param The preferred display format string.
 * @return void
 */
void check_counter::_calc_pref_display(const std::string_view& param) {
  if (param.empty()) {
    return;
  }
  for (std::string_view label : absl::StrSplit(param, ',')) {
    // make sure to transform the label to lower case
    std::string lower_label(label);
    std::ranges::transform(lower_label, lower_label.begin(), ::tolower);
    _pref_filter_list.insert(std::move(lower_label));
  }
}
