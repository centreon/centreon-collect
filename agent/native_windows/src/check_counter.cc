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
      // the items to check
      _calc_counter_filter(arg.get_string("counter-filter", ""));
      _output_syntax = arg.get_string("output-syntax", "{status}: {list}");
      _detail_syntax = arg.get_string("detail_syntax", "{label} : {value}");
      // format the output
      _calc_output_format();
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
 * @return true if the data was collected successfully, false otherwise.
 *
 * @throws std::runtime_error if the collection fails.
 */
bool check_counter::pdh_snapshot(bool first_measure) {
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

  // if we need two samples , we need to wait for the second one
  if (_need_two_samples && first_measure)
    return false;

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
          _data[key] = pItems[i].FmtValue.doubleValue;
        }
      } else {
        // if we have an error, we need to free the buffer and throw an error
        free(pItems);
        SPDLOG_LOGGER_ERROR(_logger,
                            "Failed to get formatted counter array: "
                            "PdhGetFormattedCounterArray "
                            "failed: 0x{:08X}",
                            status);
        return false;
      }
    } else {
      // if we have an error, we need to free the buffer and throw an error
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
    _data[_counter_name] = counterValue.doubleValue;
  }
  return true;
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
    std::list<com::centreon::common::perfdata>* perfs) {
  e_status ret = e_status::ok;

  if (_data.empty()) {
    SPDLOG_LOGGER_ERROR(_logger, "No data collected from the counter");
    throw std::runtime_error("No data collected from the counter");
  }

  bool is_warning = false;
  bool is_critical = false;

  // this lambda add the label to the correct list(ok,warning,critical)
  auto process_label = [this, &is_critical, &is_warning, &perfs](
                           const std::string& label, double value) {
    if (!_critical_status.empty() &&
        _critical_rules_filter->check(counter_data{label, value})) {
      _critical_list.insert(label);
      is_critical = true;
    } else if (!_warning_status.empty() &&
               _warning_rules_filter->check(counter_data{label, value})) {
      _warning_list.insert(label);
      is_warning = true;
    } else {
      _ok_list.insert(label);
    }
    common::perfdata perfdata;
    perfdata.name(label);
    perfdata.value(value);
    perfdata.unit(std::string(_pdh_counter->unit));
    perfs->emplace_back(std::move(perfdata));
  };

  // check in the filter list if we have a warning or critical status
  if (_perf_filter_list.empty() ||
      _perf_filter_list.find("any") != _perf_filter_list.end()) {
    for (const auto& [label, value] : _data) {
      process_label(label, value);
    }
  } else {
    for (const auto& label : _perf_filter_list) {
      auto it = _data.find(label);
      if (it != _data.end()) {
        process_label(label, it->second);
      }
    }
  }

  if (_have_multi_return) {
    common::perfdata c_warn_pref;
    c_warn_pref.name("warning-count");
    c_warn_pref.value(_warning_list.size());
    perfs->emplace_back(std::move(c_warn_pref));
    common::perfdata c_crit_pref;
    c_crit_pref.name("critical-count");
    c_crit_pref.value(_critical_list.size());
    perfs->emplace_back(std::move(c_crit_pref));
  }

  // if all list are empty, that means that we have no data to check
  if (_ok_list.empty() && _warning_list.empty() && _critical_list.empty()) {
    ret = e_status::unknown;
    *output = "No data to check in this counter-filter : " + _counter_filter;
  } else {
    // check the status
    if (is_critical && _critical_list.size() >= _critical_threshold_count) {
      ret = e_status::critical;
    } else if (is_warning && _warning_list.size() >= _warning_threshold_count) {
      ret = e_status::warning;
    } else {
      ret = e_status::ok;
    }
    _print_counter(output, ret);

    // if verbose is enable
    if (_verbose) {
      *output += "\n Versobe output: \n";
      for (const auto& [label, value] : _data)
        *output +=
            std::vformat(_detail_syntax, std::make_format_args(label, value));
    }
  }
  // clear the data to free the memory
  _ok_list.clear();
  _warning_list.clear();
  _critical_list.clear();
  _data.clear();

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
    bool info_stored = pdh_snapshot(true);
    if (!_need_two_samples && info_stored) {
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
  try {
    pdh_snapshot(false);
    std::string output;
    std::list<com::centreon::common::perfdata> perfs;

    e_status status = compute(&output, &perfs);
    on_completion(start_check_index, status, perfs, {output});
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to take snapshot: {}", e.what());
    on_completion(start_check_index, e_status::unknown, {}, {e.what()});
  }
}

/**
 * @brief Display help information for the check_counter command.
 *
 * @param help_stream The stream to write the help information to.
 * @return void
 */
void check_counter::help(std::ostream& help_stream) {
  help_stream << R"(
Check_counter - Windows PDH check for Centreon
--------------------------------------------------------
JSON arguments
==============
  {
    "counter"            : string,                 # countre name :(ex: "\\System\\System Up Time")
    "counter-filter"     : string,                 # Comma-separated list of
                                                     instance names to include
                                                     ("any" to keep every one).
    "output-syntax"      : string,                 # format the output.
                                                     Place-holders: {status},
                                                     {count}, {total},
                                                     {list}, {warn_count},
                                                     {crit_count}, …
    "detail_syntax"      : string,                 # Format for each element
                                                     inside {list}.
                                                     Place-holders: {label},
                                                     ${value}.
    "warning-status"     : string,                 # Filter expression that
                                                     marks an item WARNING.
                                                     Ex: "value > 80".
    "critical-status"    : string,                 # Filter expression that
                                                     marks an item CRITICAL.
                                                     Ex: "value > 90".
    "warning-count"      : integer (default 1),    # Minimum WARNING items
                                                     before overall status is
                                                     WARNING.
    "critical-count"     : integer (default 1),    # Minimum CRITICAL items
                                                     before overall status is
                                                     CRITICAL.
    "verbose"            : bool (default false),   # add verbose output in the end.
    "use_english"        : bool (default false)    # Force English counter
                                                     names.
  }
Place-holder reference
----------------------

output-syntax supports  
{status} {count} {total} {list}  
{warn_count} {warn_list} {crit_count} {crit_list}  
{problem_count} {problem_list} {ok_count} {ok_list}

Detail-syntax supports  
{label} (alias) and {value}

Example
-------

```json
{
  "counter"         : "\\Processor(*)\\% Processor Time",
  "counter-filter"  : "_total",
  "output-syntax"   : "{status}: {warn-count}/{total} {list}",
  "detail_syntax"   : "{label}={value}",
  "warning-status"  : "value > 75",
  "critical-status" : "value > 90",
  "warning-count"   : 10,
  "verbose"         : false
}")";
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
        // for specific label, we use the getter
        filt->set_checker_from_getter(
            [label, threshold](const testable& t) -> double {
              auto const& data = static_cast<const counter_data&>(t);
              if (label != "any" && label != "value")
                if (data._value.first != label)
                  return 0.0;
              return data._value.second;
            });
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
  SPDLOG_LOGGER_INFO(_logger,
                     "check_counter {}, filter for warning: {}, critical: {}",
                     _counter_name, _warning_status, _critical_status);
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 8>
    _label_to_counter_detail{{{"${label}", "{0}"},
                              {"{label}", "{0}"},
                              {"${alias}", "{0}"},
                              {"{alias}", "{0}"},
                              {"{value}", "{1:.2f} {2}"},
                              {"${value}", "{1:.2f} {2}"}}};

constexpr std::array<std::pair<std::string_view, std::string_view>, 40>
    _label_to_output_index{{
        {"${status}", "{0}"},        {"${count}", "{1}"},
        {"${total}", "{2}"},         {"${list}", "{3}"},
        {"${warn_count}", "{4}"},    {"${warn-count}", "{4}"},
        {"${warn_list}", "{5}"},     {"${warn-list}", "{5}"},
        {"${crit_count}", "{6}"},    {"${crit-count}", "{6}"},
        {"${crit_list}", "{7}"},     {"${crit-list}", "{7}"},
        {"${problem_count}", "{8}"}, {"${problem-count}", "{8}"},
        {"${problem_list}", "{9}"},  {"${problem-list}", "{9}"},
        {"${ok_count}", "{10}"},     {"${ok-count}", "{10}"},
        {"${ok_list}", "{11}"},      {"${ok-list}", "{11}"},
        {"{status}", "{0}"},         {"{count}", "{1}"},
        {"{total}", "{2}"},          {"{list}", "{3}"},
        {"{warn_count}", "{4}"},     {"{warn-count}", "{4}"},
        {"{warn_list}", "{5}"},      {"{warn-list}", "{5}"},
        {"{crit_count}", "{6}"},     {"{crit-count}", "{6}"},
        {"{crit_list}", "{7}"},      {"{crit-list}", "{7}"},
        {"{problem_count}", "{8}"},  {"{problem-count}", "{8}"},
        {"{problem_list}", "{9}"},   {"{problem-list}", "{9}"},
        {"{ok_count}", "{10}"},      {"{ok-count}", "{10}"},
        {"{ok_list}", "{11}"},       {"{ok-list}", "{11}"},
    }};

/**
 * @brief Calculate the output format for the check counter.
 *
 * @param param The output format string.
 * @return void
 */
void check_counter::_calc_output_format() {
  for (const auto& translate : _label_to_output_index) {
    boost::replace_all(_output_syntax, translate.first, translate.second);
  }
  for (const auto& translate : _label_to_counter_detail) {
    boost::replace_all(_detail_syntax, translate.first, translate.second);
  }
}

/**
 * @brief Calculate the preferred display format for the check counter.
 *
 * @param param The preferred display format string.
 * @return void
 */
void check_counter::_calc_counter_filter(const std::string_view& param) {
  _counter_filter = param;
  if (param.empty()) {
    return;
  }
  for (std::string_view label : absl::StrSplit(param, ',')) {
    // make sure to transform the label to lower case
    std::string lower_label(label);
    std::ranges::transform(lower_label, lower_label.begin(), ::tolower);
    _perf_filter_list.insert(std::move(lower_label));
  }
}

void check_counter::_print_counter(std::string* output, e_status status) {
  int total = static_cast<int>(_data.size());
  int ok_count = static_cast<int>(_ok_list.size());
  int warn_count = static_cast<int>(_warning_list.size());
  int crit_count = static_cast<int>(_critical_list.size());
  int problem_count = warn_count + crit_count;
  int count = ok_count + problem_count;
  // format the detail output for a label,value
  auto format_detail = [this](std::string label, double value) {
    return std::vformat(_detail_syntax, std::make_format_args(
                                            label, value, _pdh_counter->unit));
  };

  // format a map
  auto format_list =
      [this, &format_detail](const absl::flat_hash_set<std::string>& data_map) {
        std::string result = "";
        for (const auto& label : data_map) {
          result += format_detail(label, _data[label]) + ",";
        }
        // remove the last comma
        if (!result.empty()) {
          result.pop_back();
        }
        return result;
      };

  std::string _ok_list_str = format_list(_ok_list);
  std::string _warning_list_str = format_list(_warning_list);
  std::string _critical_list_str = format_list(_critical_list);
  std::string _problem_list_str = _critical_list_str;
  if (!_problem_list_str.empty() && !_warning_list_str.empty()) {
    _problem_list_str += ",";
  }
  _problem_list_str += _warning_list_str;

  std::string list_str = _ok_list_str;
  if (!list_str.empty() && !_problem_list_str.empty()) {
    list_str += ",";
  }
  list_str += _problem_list_str;

  std::string status_label;
  if (status == e_status::ok) {
    status_label = "OK";
  } else if (status == e_status::warning) {
    status_label = "WARNING";
  } else if (status == e_status::critical) {
    status_label = "CRITICAL";
  } else {
    status_label = "UNKNOWN";
  }
  // format the output string
  *output = std::vformat(
      _output_syntax,
      std::make_format_args(status_label, count, total, list_str, warn_count,
                            _warning_list_str, crit_count, _critical_list_str,
                            problem_count, _problem_list_str, ok_count,
                            _ok_list_str));
}
