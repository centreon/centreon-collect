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

#include "windows_util.hh"

#include "filter.hh"
#pragma comment(lib, "wevtapi.lib")

#include "com/centreon/exceptions/msg_fmt.hh"
#include "event_log/data.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::event_log;

namespace com::centreon::agent::event_log {

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

}  // namespace com::centreon::agent::event_log

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
                       void** buffer,
                       DWORD* buffer_size) {
  DWORD buffer_used = 0;
  _property_count = 0;

  // Get the size of the buffer needed.
  if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                 *buffer_size, *buffer, &buffer_used, &_property_count)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      *buffer_size = buffer_used;
      *buffer = realloc(*buffer, *buffer_size);
      if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                     *buffer_size, *buffer, &buffer_used, &_property_count)) {
        throw exceptions::msg_fmt("Failed to render event: {}",
                                  get_last_error_as_string());
      }
    } else {
      throw exceptions::msg_fmt("Failed to render event: {}",
                                get_last_error_as_string());
    }
  }
  for (unsigned debug = 0; debug < _property_count; ++debug) {
    EVT_VARIANT& val_debug = ((EVT_VARIANT*)*buffer)[debug];
    int ii = 0;
  }

  std::chrono::file_clock::duration d{
      ((EVT_VARIANT*)*buffer)[EvtSystemTimeCreated].FileTimeVal};

  std::chrono::file_clock::time_point t{d};

  /*std::string s = std::format("{:%Y-%m-%d %H:%M:%S}", t);
  std::cout << s << std::endl;*/

  _data = *buffer;
}

std::wstring_view event_data::get_provider() const {
  if (_property_count > EvtSystemProviderName &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemProviderName].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemProviderName]
        .StringVal;
  } else {
    return L"";
  }
}

uint16_t event_data::get_event_id() const {
  if (_property_count > EvtSystemEventID &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventID].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventID].UInt16Val;
  } else {
    return 0;
  }
}

uint8_t event_data::get_level() const {
  if (_property_count > EvtSystemLevel &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemLevel].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemLevel].ByteVal;
  } else {
    return 0;
  }
}

uint16_t event_data::get_task() const {
  if (_property_count > EvtSystemTask &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTask].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTask].UInt16Val;
  } else {
    return 0;
  }
}

int64_t event_data::get_keywords() const {
  if (_property_count > EvtSystemKeywords &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemKeywords].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemKeywords].Int64Val;
  } else {
    return 0;
  }
}

uint64_t event_data::get_time_created() const {
  if (_property_count > EvtSystemTimeCreated &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTimeCreated].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTimeCreated]
        .FileTimeVal;
  } else {
    return 0;
  }
}

uint64_t event_data::get_record_id() const {
  if (_property_count > EvtSystemEventRecordId &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventRecordId].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventRecordId]
        .UInt64Val;
  } else {
    return 0;
  }
}

std::wstring_view event_data::get_computer() const {
  if (_property_count > EvtSystemComputer &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemComputer].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemComputer].StringVal;
  } else {
    return L"";
  }
}

std::wstring_view event_data::get_channel() const {
  if (_property_count > EvtSystemChannel &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemChannel].Type !=
          EvtVarTypeNull) {
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
  } else if (filt->get_label() == "written") {
    filt->calc_duration();
    filt->change_threshold_to_abs();
    filt->set_checker_from_getter([](const testable& t) {
      FILETIME ft;
      SYSTEMTIME st;
      GetSystemTime(&st);
      SystemTimeToFileTime(&st, &ft);
      ULARGE_INTEGER caster{.LowPart = ft.dwLowDateTime,
                            .HighPart = ft.dwHighDateTime};
      return caster.QuadPart -
             static_cast<const event_data&>(t).get_time_created();
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
    uint8_t level = str_to_level(lpwcstr_to_acp(filt->get_value().c_str()));
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
    wstring_to_string(val, &cval);
    _values.insert(str_to_level(cval));
  }
}

template <filters::in_not rule>
event_filter::level_in<rule>::level_in(
    const filters::label_compare_to_string<wchar_t>& filt) {
  std::string cval;
  wstring_to_string(filt.get_value(), &cval);
  _values.insert(str_to_level(cval));
}

template <filters::in_not rule>
bool event_filter::level_in<rule>::operator()(const testable& t) const {
  uint8_t event_level = static_cast<const event_data&>(t).get_level();
  if constexpr (rule == filters::in_not::in) {
    return _values.find(event_level) != _values.end();
  } else {
    return _values.find(event_level) == _values.end();
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
event::event(const event_data& raw_data, e_status status, std::string&& message)
    : _event_id(raw_data.get_event_id()),
      _record_id(raw_data.get_record_id()),
      _time(std::chrono::file_clock::duration(raw_data.get_time_created())),
      _audit(raw_data.get_keywords()),
      _level(raw_data.get_level()),
      _status(status),
      _computer(lpwcstr_to_acp(raw_data.get_computer().data())),
      _channel(lpwcstr_to_acp(raw_data.get_channel().data())),
      _provider(lpwcstr_to_acp(raw_data.get_provider().data())),
      _message(message) {}

namespace com::centreon::agent::event_log {
std::ostream& operator<<(std::ostream& s, const event& evt) {
  s << "event_id:" << evt.event_id() << " time:" << evt.time()
    << " level:" << evt.level() << " record_id:" << evt.record_id()
    << " status:" << evt.status() << " channel:" << evt.channel()
    << " message:" << evt.message();
  return s;
}

}  // namespace com::centreon::agent::event_log
