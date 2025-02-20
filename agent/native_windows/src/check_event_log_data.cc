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

#include <chrono>
#include "boost/algorithm/string/replace.hpp"
#include "windows_util.hh"

#include "filter.hh"
#pragma comment(lib, "wevtapi.lib")

#include "check_event_log_data.hh"
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
    : _id(raw_data.get_record_id()),
      _time(std::chrono::file_clock::duration(raw_data.get_time_created())),
      _audit(raw_data.get_keywords()),
      _level(raw_data.get_level()),
      _status(status),
      _computer(lpwcstr_to_acp(raw_data.get_computer().data())),
      _channel(lpwcstr_to_acp(raw_data.get_channel().data())),
      _provider(lpwcstr_to_acp(raw_data.get_provider().data())),
      _message(message) {}

namespace com::centreon::agent::check_event_log_detail {
std::ostream& operator<<(std::ostream& s, const event& evt) {
  s << "id:" << evt.id() << " time:" << evt.time() << " level:" << evt.level()
    << " status:" << evt.status() << " channel:" << evt.channel()
    << " message:" << evt.message();
  return s;
}

}  // namespace com::centreon::agent::check_event_log_detail

/***************************************************************************
 *                                                                       *
 *                          event_container                              *
 *                                                                       *
 ****************************************************************************/

event_container::event_container(const std::string_view& file,
                                 const std::string_view& primary_filter,
                                 const std::string_view& warning_filter,
                                 const std::string_view& critical_filter,
                                 duration scan_range,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : _scan_range(scan_range),
      _file(file.begin(), file.end()),
      _insertion_cpt(0),
      _nb_warning(0),
      _nb_critial(0),
      _render_context(nullptr),
      _subscription(nullptr),
      _read_event_buffer(malloc(4096)),
      _buffer_size(4096),
      _read_message_buffer(static_cast<LPWSTR>(malloc(4096 * sizeof(wchar_t)))),
      _message_buffer_size(4095),
      _logger(logger) {
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

  _render_context = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem);
  if (!_render_context) {
    throw exceptions::msg_fmt("fail to create render context: {}",
                              get_last_error_as_string());
  }
}

event_container::~event_container() {
  if (_subscription != nullptr) {
    EvtClose(_subscription);
  }
  if (_render_context != nullptr) {
    EvtClose(_render_context);
  }
  free(_read_event_buffer);
  free(_read_message_buffer);

  for (auto& to_close : _provider_metadata) {
    EvtClose(to_close.second);
  }
}

void event_container::start() {
  std::wstring query;
  query = std::format(
      L"Event/System[TimeCreated[timediff(@SystemTime) <= {}]]",
      std::chrono::duration_cast<std::chrono::milliseconds>(_scan_range)
          .count());

  _subscription = EvtSubscribe(nullptr, nullptr, _file.c_str(), query.c_str(),
                               nullptr, this, _subscription_callback,
                               EvtSubscribeStartAtOldestRecord);
  if (_subscription == nullptr) {
    throw exceptions::msg_fmt("Failed to subscribe to event log: {}",
                              get_last_error_as_string());
  }
}

DWORD WINAPI
event_container::_subscription_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                        PVOID p_context,
                                        EVT_HANDLE h_event) {
  event_container* me = reinterpret_cast<event_container*>(p_context);
  if (action == EvtSubscribeActionError) {
    SPDLOG_LOGGER_ERROR(me->_logger, "subscription_callback error");
    return ERROR_SUCCESS;
  }
  me->_on_event(h_event);
  return ERROR_SUCCESS;
}

LPWSTR event_container::_get_message_string(EVT_HANDLE h_metadata,
                                            EVT_HANDLE h_event) {
  DWORD buffer_used = 0;

  if (EvtFormatMessage(h_metadata, h_event, 0, 0, nullptr,
                       EvtFormatMessageEvent, _message_buffer_size,
                       _read_message_buffer, &buffer_used)) {
    _read_message_buffer[_message_buffer_size] = L'\0';
    return _read_message_buffer;
  }
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    _read_message_buffer = static_cast<LPWSTR>(
        malloc((buffer_used + 1) * sizeof(wchar_t)));  //+1 for \0
    _message_buffer_size = buffer_used;
  }
  if (EvtFormatMessage(h_metadata, h_event, 0, 0, nullptr,
                       EvtFormatMessageEvent, _message_buffer_size,
                       _read_message_buffer, &buffer_used)) {
    _read_message_buffer[_message_buffer_size] = L'\0';
    return _read_message_buffer;
  }
  SPDLOG_LOGGER_ERROR(_logger, "fail to get message");
  return nullptr;
}

void event_container::_on_event(EVT_HANDLE h_event) {
  try {
    absl::MutexLock l(&_events_m);
    event_data raw_event(_render_context, h_event, &_read_event_buffer,
                         &_buffer_size);

    if (!_primary_filter->allow(raw_event)) {
      return;
    }

    e_status event_status = e_status::ok;
    if (_critical_filter->allow(raw_event)) {
      event_status = e_status::critical;
    } else if (_warning_filter->allow(raw_event)) {
      event_status = e_status::warning;
    } else {
      return;
    }

    std::string message;
    if (_need_to_decode_message_content) {
      auto yet_open = _provider_metadata.find(raw_event.get_provider());
      if (yet_open == _provider_metadata.end()) {
        EVT_HANDLE new_publisher = EvtOpenPublisherMetadata(
            NULL, raw_event.get_provider().data(), NULL, 0, 0);
        if (!new_publisher) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "fail to get publisher metadata for provider: {}",
                              lpwcstr_to_acp(raw_event.get_provider().data()));
        } else {
          yet_open = _provider_metadata
                         .emplace(raw_event.get_provider(), new_publisher)
                         .first;
        }
      }
      if (yet_open != _provider_metadata.end()) {
        LPWSTR mess = _get_message_string(yet_open->second, h_event);
        if (mess) {
          message = lpwcstr_to_acp(mess);
        }
      }
    }

    boost::replace_all(message, "\r\n", " ");

    _events.emplace(raw_event, event_status, std::move(message));
    if (event_status == e_status::warning) {
      ++_nb_warning;
    } else {
      ++_nb_critical;
    }

    // every 10 events we
    if (!(++_insertion_cpt) % 10) {
      auto peremption = std::chrono::file_clock::now() - _scan_range;
      auto& time_index = _events.get<0>();
      while (!_events.empty()) {
        auto oldest = time_index.begin();
        if (oldest->time() > peremption) {
          break;
        }
        if (oldest->status() == e_status::warning) {
          --_nb_warning;
        } else {
          --nb_critical;
        }
        _events.erase(oldest);
      }
    }

  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to read event: {}", e.what());
  }
}
