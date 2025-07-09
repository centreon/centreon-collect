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

  _data = *buffer;
}

/**
 * @brief Get the provider of the event
 * @return std::wstring_view The provider
 */
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

/**
 * @brief Get the event id
 * @return uint16_t The event id
 */
uint16_t event_data::get_event_id() const {
  if (_property_count > EvtSystemEventID &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventID].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemEventID].UInt16Val;
  } else {
    return 0;
  }
}

/**
 * @brief Get the level of the event (error, warning....)
 * @return uint8_t The level
 */
uint8_t event_data::get_level() const {
  if (_property_count > EvtSystemLevel &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemLevel].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemLevel].ByteVal;
  } else {
    return 0;
  }
}

/**
 * @brief Get the task of the event
 * @return uint16_t The task
 */
uint16_t event_data::get_task() const {
  if (_property_count > EvtSystemTask &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTask].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemTask].UInt16Val;
  } else {
    return 0;
  }
}

/**
 * @brief Get the keywords of the event
 * @return int64_t The keywords values are mask like _keywords_audit_success
 */
int64_t event_data::get_keywords() const {
  if (_property_count > EvtSystemKeywords &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemKeywords].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemKeywords].Int64Val;
  } else {
    return 0;
  }
}

/**
 * @brief Get the time the event was created
 * @return uint64_t The time in file time format
 */
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

/**
 * @brief Get the record id of the event
 * @return uint64_t The record id
 */
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

/**
 * @brief Get the computer name
 * @return std::wstring_view The computer name
 */
std::wstring_view event_data::get_computer() const {
  if (_property_count > EvtSystemComputer &&
      reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemComputer].Type !=
          EvtVarTypeNull) {
    return reinterpret_cast<EVT_VARIANT*>(_data)[EvtSystemComputer].StringVal;
  } else {
    return L"";
  }
}

/**
 * @brief Get the channel of the event
 * @return std::wstring_view The channel
 */
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
 *                          event_filter::check_builder                  *
 *                                                                       *
 ****************************************************************************/

/**
 * @brief functor that set the checker for each filter along filter label
 * @param filt The filter
 */
template <typename data_tag_type>
void event_filter::check_builder<data_tag_type>::operator()(filter* filt) {
  if (filt->get_type() == filter::filter_type::label_compare_to_value) {
    set_label_compare_to_value(
        static_cast<filters::label_compare_to_value*>(filt));
  } else if (filt->get_type() == filter::filter_type::label_compare_to_string) {
    set_label_compare_to_string(static_cast<filters::label_compare_to_string<
                                    typename data_tag_type::char_type>*>(filt));
  } else {
    set_label_in(
        static_cast<filters::label_in<typename data_tag_type::char_type>*>(
            filt));
  }
}

/**
 * @brief Set the checker for a label_in filter
 * @param filt The filter
 */
template <typename data_tag_type>
void event_filter::check_builder<data_tag_type>::set_label_compare_to_value(
    filters::label_compare_to_value* filt) {
  if (filt->get_label() == "event_id") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_event_id();
    });
  } else if (filt->get_label() == "level") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_level();
    });
  } else if (filt->get_label() == "written") {
    // we may have written > -60m
    filt->calc_duration();
    filt->change_threshold_to_abs();
    // we calculate min_written because we recalculate event status that are
    // older than min_written
    if ((filt->get_comparison() ==
             filters::label_compare_to_value::comparison::less_than ||
         filt->get_comparison() ==
             filters::label_compare_to_value::comparison::less_than_or_equal) &&
        (!min_written.count() || min_written.count() > filt->get_value())) {
      min_written =
          std::chrono::seconds(static_cast<unsigned>(filt->get_value()));
    }

    if constexpr (std::is_same_v<data_tag_type, raw_data_tag>) {
      filt->set_checker_from_getter([](const testable& t) {
        FILETIME ft;
        SYSTEMTIME st;
        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &ft);
        ULARGE_INTEGER caster{.LowPart = ft.dwLowDateTime,
                              .HighPart = ft.dwHighDateTime};
        return (caster.QuadPart -
                static_cast<const event_data&>(t).get_time_created()) /
               10000000ULL;
      });
    } else {
      filt->set_checker_from_getter([](const testable& t) {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::file_clock::now() -
                   static_cast<const event&>(t).get_time())
            .count();
      });
    }
  }
}

/**
 * @brief Set the checker for a label_compare_to_string filter
 * @param filt The filter
 */
template <typename data_tag_type>
void event_filter::check_builder<data_tag_type>::set_label_compare_to_string(
    filters::label_compare_to_string<typename data_tag_type::char_type>* filt)
    const {
  if (filt->get_label() == "provider") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_provider();
    });
  } else if (filt->get_label() == "level") {
    uint8_t level;
    if constexpr (std::is_same_v<data_tag_type, raw_data_tag>) {
      level = str_to_level(lpwcstr_to_acp(filt->get_value().c_str()));
    } else {
      level = str_to_level(filt->get_value());
    }
    if (filt->get_comparison() == filters::string_comparison::equal) {
      filt->set_checker([level](const testable& t) {
        return static_cast<const data_tag_type::type&>(t).get_level() == level;
      });
    } else {
      filt->set_checker([level](const testable& t) {
        return static_cast<const data_tag_type::type&>(t).get_level() != level;
      });
    }
  } else if (filt->get_label() == "keywords") {
    if (filt->get_value() == data_tag_type::audit_success) {
      if (filt->get_comparison() == filters::string_comparison::equal) {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const data_tag_type::type&>(t).get_keywords() &
                  _keywords_audit_mask) == _keywords_audit_success;
        });
      } else {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const data_tag_type::type&>(t).get_keywords() &
                  _keywords_audit_mask) != _keywords_audit_success;
        });
      }
    } else if (filt->get_value() == data_tag_type::audit_failure) {
      if (filt->get_comparison() == filters::string_comparison::equal) {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const data_tag_type::type&>(t).get_keywords() &
                  _keywords_audit_mask) == _keywords_audit_failure;
        });
      } else {
        filt->set_checker([](const testable& t) -> bool {
          return (static_cast<const data_tag_type::type&>(t).get_keywords() &
                  _keywords_audit_mask) != _keywords_audit_failure;
        });
      }
    } else {
      throw std::invalid_argument(
          "only auditFailure and auditSuccess keywords are allowed");
    }
  } else if (filt->get_label() == "computer") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_computer();
    });
  } else if (filt->get_label() == "channel") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_channel();
    });
  } else {
    throw exceptions::msg_fmt("unknown filter label {}", filt->get_label());
  }
}

/**
 * @brief Set the checker for a label_in filter
 * @param filt The filter
 */
template <typename data_tag_type>
void event_filter::check_builder<data_tag_type>::set_label_in(
    filters::label_in<typename data_tag_type::char_type>* filt) const {
  if (filt->get_label() == "provider") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_provider();
    });
  } else if (filt->get_label() == "event_id") {
    filt->set_checker_from_number_getter([](const testable& t) {
      return static_cast<uint32_t>(
          static_cast<const data_tag_type::type&>(t).get_event_id());
    });
  } else if (filt->get_label() == "level") {
    if (filt->get_rule() == filters::in_not::in) {
      filt->set_checker(level_in<filters::in_not::in, data_tag_type>(*filt));
    } else {
      filt->set_checker(
          level_in<filters::in_not::not_in, data_tag_type>(*filt));
    }
  } else if (filt->get_label() == "keywords") {
    uint64_t mask = 0;
    if (filt->get_values().contains(data_tag_type::audit_success)) {
      mask = _keywords_audit_success;
    }
    if (filt->get_values().contains(data_tag_type::audit_failure)) {
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
      return static_cast<const data_tag_type::type&>(t).get_computer();
    });
  } else if (filt->get_label() == "channel") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const data_tag_type::type&>(t).get_channel();
    });
  } else {
    throw exceptions::msg_fmt("unknown filter label {}", filt->get_label());
  }
}

/***************************************************************************
 *                                                                       *
 *                          event_filter::level_in                       *
 *                                                                       *
 ****************************************************************************/

/**
 * @brief Construct a new level_in object
 * @param filt The filter
 */
template <filters::in_not rule, typename data_type_tag>
event_filter::level_in<rule, data_type_tag>::level_in(
    const filters::label_in<typename data_type_tag::char_type>& filt) {
  if constexpr (data_type_tag::use_wchar) {
    std::string cval;
    for (const auto& val : filt.get_values()) {
      wstring_to_string(val, &cval);
      _values.insert(str_to_level(cval));
    }
  } else {
    for (const auto& val : filt.get_values()) {
      _values.insert(str_to_level(val));
    }
  }
}

/**
 * @brief Construct a new level_in object
 * @param filt The filter
 */
template <filters::in_not rule, typename data_type_tag>
event_filter::level_in<rule, data_type_tag>::level_in(
    const filters::label_compare_to_string<typename data_type_tag::char_type>&
        filt) {
  if constexpr (data_type_tag::use_wchar) {
    std::string cval;
    wstring_to_string(filt.get_value(), &cval);
    _values.insert(str_to_level(cval));
  } else {
    _values.insert(str_to_level(filt.get_value()));
  }
}

/**
 * @brief the functor checker
 */
template <filters::in_not rule, typename data_type_tag>
bool event_filter::level_in<rule, data_type_tag>::operator()(
    const testable& t) const {
  uint8_t event_level = static_cast<const data_type_tag::type&>(t).get_level();
  if constexpr (rule == filters::in_not::in) {
    return _values.find(event_level) != _values.end();
  } else {
    return _values.find(event_level) == _values.end();
  }
}

/***************************************************************************
 *                                                                       *
 *                          event                                        *
 *                                                                       *
 ****************************************************************************/

/**
 * @brief Constructs an event object.
 *
 * @param raw_data The raw event data containing various event attributes.
 * @param tp The time point representing the event time.
 * @param message The message associated with the event.
 *
 * This constructor initializes an event object with the provided raw event
 * data, time point, and message. It extracts and converts various attributes
 * from the raw event data, such as event ID, record ID, keywords, level,
 * computer, channel, and provider. It also constructs a string representation
 * of the keywords based on the audit success and failure flags.
 */
event::event(const event_data& raw_data,
             const std::chrono::file_clock::time_point& tp,
             std::string&& message)
    : _event_id(raw_data.get_event_id()),
      _record_id(raw_data.get_record_id()),
      _keywords(raw_data.get_keywords()),
      _time(tp),
      _audit(raw_data.get_keywords()),
      _level(raw_data.get_level()),
      _computer(lpwcstr_to_acp(raw_data.get_computer().data())),
      _channel(lpwcstr_to_acp(raw_data.get_channel().data())),
      _provider(lpwcstr_to_acp(raw_data.get_provider().data())),
      _message(message) {
  std::string keywords;
  if (raw_data.get_keywords() & _keywords_audit_success) {
    keywords = "audit_success";
  }
  if (raw_data.get_keywords() & _keywords_audit_failure) {
    if (!keywords.empty()) {
      keywords.push_back('|');
    }
    keywords += "audit_failure";
  }
  _str_keywords = std::move(keywords);
}

namespace com::centreon::agent::event_log {
std::ostream& operator<<(std::ostream& s, const event& evt) {
  s << "event_id:" << evt.get_event_id() << " time:" << evt.get_time()
    << " level:" << evt.get_level() << " record_id:" << evt.get_record_id()
    << " channel:" << evt.get_channel() << " message:" << evt.get_message();
  return s;
}

template class event_filter::check_builder<event_filter::event_tag>;
template class event_filter::check_builder<event_filter::raw_data_tag>;

}  // namespace com::centreon::agent::event_log
