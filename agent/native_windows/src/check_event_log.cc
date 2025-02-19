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

#include <stdint.h>
#include <stdexcept>
#include "absl/strings/numbers.h"
#include "filter.hh"
#pragma comment(lib, "wevtapi.lib")

#include "check_event_log.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_event_log_detail;

namespace com::centreon::agent::check_event_log_detail {

static const absl::flat_hash_map<std::string_view, uint64_t> _str_to_keywords{
    {"auditsuccess", _keywords_audit_success},
    {"auditfailure", _keywords_audit_failure},
};

static const absl::flat_hash_map<std::string_view, uint8_t> _str_to_levels{
    {"critical", 1},      {"error", 2},       {"warning", 3},
    {"warn", 3},          {"information", 4}, {"info", 4},
    {"informational", 4}, {"verbose", 5},     {"debug", 5}};

static uint8_t str_to_level(const std::string_view& str) {
  auto level_label_it = _str_to_levels.find(str);
  if (level_label_it != _str_to_levels.end()) {
    return level_label_it->second;
  }

  uint32_t num_str_value;
  if (!absl::SimpleAtoi(str, &num_str_value)) {
    throw std::invalid_argument("unknown level value");
  }
  if (num_str_value > 0x0FF) {
    throw std::invalid_argument("too large level value");
  }
  return num_str_value;
}

}  // namespace com::centreon::agent::check_event_log_detail

/***************************************************************************
 *                                                                       *
 *                          event_data                                   *
 *                                                                       *
 ****************************************************************************/

/**
 * @brief Construct a new event_data::event_data object
 * @param render_context a context that tell to EvtRender wich data to fetch
 * @param event_handle handle of event
 * @param buffer buffer to store the data, it is reused for each received event,
 * may be reallocated
 * @param buffer_size size of the allocated buffer
 */
event_data::event_data(EVT_HANDLE render_context,
                       EVT_HANDLE event_handle,
                       uint8_t** buffer,
                       DWORD* buffer_size) {
  DWORD buffer_used = 0;
  _property_count = 0;

  // Get the size of the buffer needed.
  if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                 *buffer_size, *buffer, &buffer_used, &_property_count)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      delete[] *buffer;
      *buffer_size = buffer_used;
      *buffer = new uint8_t[*buffer_size];
      if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                     *buffer_size, *buffer, &buffer_used, &_property_count)) {
        throw exceptions::msg_fmt("Failed to render event: {}", GetLastError());
      }
    } else {
      throw exceptions::msg_fmt("Failed to render event: {}", GetLastError());
    }
  }
  _data = *buffer;
}

std::wstring_view event_data::get_provider() const {
  //
  //
  //  TODO convert to lowercase
  //
  //
  if (_property_count > EvtSystemProviderName) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemProviderName]
        .StringVal;
  } else {
    return L"";
  }
}

uint16_t event_data::get_event_id() const {
  if (_property_count > EvtSystemEventID) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventID].UInt16Val;
  } else {
    return 0;
  }
}

uint8_t event_data::get_level() const {
  if (_property_count > EvtSystemLevel) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemLevel].ByteVal;
  } else {
    return 0;
  }
}

uint16_t event_data::get_task() const {
  if (_property_count > EvtSystemTask) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTask].UInt16Val;
  } else {
    return 0;
  }
}

int64_t event_data::get_keywords() const {
  if (_property_count > EvtSystemKeywords) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemKeywords].Int64Val;
  } else {
    return 0;
  }
}

uint64_t event_data::get_time_created() const {
  if (_property_count > EvtSystemTimeCreated) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTimeCreated]
        .FileTimeVal;
  } else {
    return 0;
  }
}

uint64_t event_data::get_record_id() const {
  if (_property_count > EvtSystemEventRecordId) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventRecordId]
        .UInt64Val;
  } else {
    return 0;
  }
}

std::wstring_view event_data::get_computer() const {
  if (_property_count > EvtSystemComputer) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemComputer].StringVal;
  } else {
    return L"";
  }
}

std::wstring_view event_data::get_channel() const {
  if (_property_count > EvtSystemChannel) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemChannel].StringVal;
  } else {
    return L"";
  }
}

/***************************************************************************
 *                                                                       *
 *                          event_filter                                 *
 *                                                                       *
 ****************************************************************************/
void event_filter::check_builder::operator()(filter* filt) const {
  if (filt->get_type() == filter::filter_type::label_compare_to_value) {
    set_label_compare_to_value(
        static_cast<filters::label_compare_to_value*>(filt));
  } else if (filt->get_type() == filter::filter_type::label_compare_to_string) {
    set_label_compare_to_string(
        static_cast<filters::label_compare_to_string<wchar_t>*>(filt));
  } else {
    set_label_in(static_cast<filters::label_in<wchar_t>*>(filt));
  }
}

void event_filter::check_builder::set_label_compare_to_value(
    filters::label_compare_to_value* filt) const {
  if (filt->get_label() == "event_id") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_event_id();
    });
  } else if (filt->get_label() == "level") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_level();
    });
  }
}

void event_filter::check_builder::set_label_compare_to_string(
    filters::label_compare_to_string<wchar_t>* filt) const {
  if (filt->get_label() == "provider") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_provider();
    });
  } else if (filt->get_label() == "level") {
    uint8_t level = str_to_level(
        std::string(filt->get_value().begin(), filt->get_value().end()));
    if (filt->get_comparison() == filters::string_comparison::equal) {
      filt->set_checker([level](const testable& t) {
        return static_cast<const event_data&>(t).get_level() == level;
      });
    } else {
      filt->set_checker([level](const testable& t) {
        return static_cast<const event_data&>(t).get_level() != level;
      });
    }
  } else if (filt->get_label() == "keywords") {
    if (filt->get_value() == L"auditsuccess") {
      if (filt->get_comparison() == filters::string_comparison::equal) {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const event_data&>(t).get_keywords() &
                  _keywords_audit_mask) == _keywords_audit_success;
        });
      } else {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const event_data&>(t).get_keywords() &
                  _keywords_audit_mask) != _keywords_audit_success;
        });
      }
    } else if (filt->get_value() == L"auditfailure") {
      if (filt->get_comparison() == filters::string_comparison::equal) {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const event_data&>(t).get_keywords() &
                  _keywords_audit_mask) == _keywords_audit_failure;
        });
      } else {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const event_data&>(t).get_keywords() &
                  _keywords_audit_mask) != _keywords_audit_failure;
        });
      }
    } else {
      throw std::invalid_argument(
          "only auditFailure and auditSuccess keywords are allowed");
    }
  } else if (filt->get_label() == "computer") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_computer();
    });
  } else if (filt->get_label() == "channel") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_channel();
    });
  } else {
    throw exceptions::msg_fmt("unknwon filter label {}", filt->get_label());
  }
}

void event_filter::check_builder::set_label_in(
    filters::label_in<wchar_t>* filt) const {
  if (filt->get_label() == "provider") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_provider();
    });
  } else if (filt->get_label() == "event_id") {
    filt->set_checker_from_number_getter([](const testable& t) {
      return static_cast<uint32_t>(
          static_cast<const event_data&>(t).get_event_id());
    });
  } else if (filt->get_label() == "level") {
    if (filt->get_rule() == filters::in_not::in) {
      filt->set_checker(level_in<filters::in_not::in>(*filt));
    } else {
      filt->set_checker(level_in<filters::in_not::not_in>(*filt));
    }
  } else if (filt->get_label() == "keywords") {
    uint64_t mask = 0;
    if (filt->get_values().contains(L"auditsuccess")) {
      mask = _keywords_audit_success;
    }
    if (filt->get_values().contains(L"auditfailure")) {
      mask |= _keywords_audit_failure;
    }
    if (filt->get_rule() == filters::in_not::in) {
      filt->set_checker([mask](const testable& t) -> bool {
        return (static_cast<const event_data&>(t).get_keywords() & mask) != 0;
      });
    } else {
      filt->set_checker([mask](const testable& t) -> bool {
        return (static_cast<const event_data&>(t).get_keywords() & mask) == 0;
      });
    }
  } else if (filt->get_label() == "computer") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_computer();
    });
  } else if (filt->get_label() == "channel") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const event_data&>(t).get_channel();
    });
  } else {
    throw exceptions::msg_fmt("unknwon filter label {}", filt->get_label());
  }
}

template <filters::in_not rule>
event_filter::level_in<rule>::level_in(const filters::label_in<wchar_t>& filt) {
  std::string cval;
  for (const auto& val : filt.get_values()) {
    filters::wstring_to_string(val, &cval);
    _values.insert(str_to_level(cval));
  }
}

template <filters::in_not rule>
event_filter::level_in<rule>::level_in(
    const filters::label_compare_to_string<wchar_t>& filt) {
  std::string cval;
  filters::wstring_to_string(filt.get_value(), &cval);
  _values.insert(str_to_level(cval));
}

template <filters::in_not rule>
bool event_filter::level_in<rule>::operator()(const testable& t) const {
  if constexpr (rule == filters::in_not::in) {
    return _values.find(static_cast<const event_data&>(t).get_level()) !=
           _values.end();
  } else {
    return _values.find(static_cast<const event_data&>(t).get_level()) ==
           _values.end();
  }
}

event_filter::event_filter(const std::string_view& filter_str,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _logger(logger) {
  if (!filter::create_filter(filter_str, logger, &_filter, true)) {
    throw exceptions::msg_fmt("fail to parse filter string: {}", filter_str);
  }
  try {
    _filter.apply_checker(check_builder());
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "wrong_value for {}: {}", filter_str, e.what());
    throw;
  }
}

/***************************************************************************
 *                                                                       *
 *                          event                                        *
 *                                                                       *
 ****************************************************************************/
event::event(const event_data& raw_data, e_status status)
    : _id(raw_data.get_record_id()),
      _time(std::chrono::system_clock::from_time_t(
          raw_data.get_time_created() / 10000000 - 11644473600)),
      _audit(raw_data.get_keywords()),
      _level(raw_data.get_level()),
      _status(status),
      _computer(raw_data.get_computer()),
      _channel(raw_data.get_channel()),
      _provider(raw_data.get_provider()) {}

/***************************************************************************
 *                                                                       *
 *                          event_container                              *
 *                                                                       *
 ****************************************************************************/

event_container::event_container(const std::string_view& primary_filter,
                                 const std::string_view& warning_filter,
                                 const std::string_view& critical_filter,
                                 const std::set<std::string>& displayed_fields,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : _logger(logger) {
  _render_context = EvtCreateRenderContext(0, nullptr, EvtRenderContextValues);
  if (_render_context == nullptr) {
    throw exceptions::msg_fmt("Failed to create render context: {}",
                              GetLastError());
  }

  if (!primary_filter.empty()) {
    try {
      _primary_filter = std::make_unique<event_filter>(primary_filter, logger);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse event filter: {}", e.what());
      throw;
    }
  }

  if (!warning_filter.empty()) {
    try {
      _warning_filter = std::make_unique<event_filter>(warning_filter, logger);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse warning filter: {}", e.what());
      throw;
    }
  }

  if (!critical_filter.empty()) {
    try {
      _critical_filter =
          std::make_unique<event_filter>(critical_filter, logger);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse critical filter: {}",
                          e.what());
      throw;
    }
  }

  std::set<std::string> needed_fields = displayed_fields;
  auto get_fields = [&needed_fields](const filter* filt) mutable {
    if (filt->get_type() == filter::filter_type::label_compare_to_string) {
      needed_fields.insert(
          static_cast<const filters::label_compare_to_string<wchar_t>*>(filt)
              ->get_label());

    } else if (filt->get_type() == filter::filter_type::label_in) {
      needed_fields.insert(
          static_cast<const filters::label_in<wchar_t>*>(filt)->get_label());
    }
  };
  if (_primary_filter != nullptr) {
    _primary_filter->visit(get_fields);
  }
  if (_warning_filter != nullptr) {
    _warning_filter->visit(get_fields);
  }
  if (_critical_filter != nullptr) {
    _critical_filter->visit(get_fields);
  }
}

event_container::~event_container() {
  if (_subscription != nullptr) {
    EvtClose(_subscription);
  }
  if (_render_context != nullptr) {
    EvtClose(_render_context);
  }
}

void event_container::start() {
  std::wstring query;
  _subscription =
      EvtSubscribe(nullptr, nullptr, L"*", query.c_str(), _render_context, this,
                   nullptr, EvtSubscribeStartAtOldestRecord);
  if (_subscription == nullptr) {
    throw exceptions::msg_fmt("Failed to subscribe to event log: {}",
                              GetLastError());
  }
}

DWORD WINAPI
event_container::subscription_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                       PVOID pContext,
                                       EVT_HANDLE hEvent) {
  event_container* me = reinterpret_cast<event_container*>(pContext);
  if (action == EvtSubscribeActionError) {
    SPDLOG_LOGGER_ERROR(me->_logger, "subscription_callback error");
    return ERROR_SUCCESS;
  }
  me->on_event(hEvent);
  return ERROR_SUCCESS;
}

check_event_log::check_event_log(
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
            stat) {
  com::centreon::common::rapidjson_helper arg(args);
  try {
    if (args.IsObject()) {
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_uptime, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

/**
 * @brief get uptime with GetTickCount64
 *
 * @param timeout unused
 */
void check_event_log::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  std::string output;
  common::perfdata perf;
  e_status status = compute(GetTickCount64(), &output, &perf);

  _io_context->post([me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perf)]() {
    on_completion(_get_running_check_index(), status, {performance}, {out});
  });
}

e_status check_event_log::compute(uint64_t ms_uptime,
                                  std::string* output,
                                  com::centreon::common::perfdata* perf) {
  return e_status::ok;
}
