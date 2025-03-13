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

#ifndef CENTREON_AGENT_CHECK_EVENT_LOG_DATA_HH
#define CENTREON_AGENT_CHECK_EVENT_LOG_DATA_HH

#include <winevt.h>

#include <boost/flyweight.hpp>
#include "com/centreon/common/absl_flyweight_factory.hh"

#include "check.hh"
#include "filter.hh"

namespace com::centreon::agent::event_log {

using time_point = std::chrono::system_clock::time_point;
using duration = std::chrono::system_clock::duration;

constexpr uint64_t _keywords_filter = 0x00FFFFFFFFFFFFFFL;
constexpr uint64_t _keywords_audit_success = 0x0020000000000000L;
constexpr uint64_t _keywords_audit_failure = 0x0010000000000000L;
constexpr uint64_t _keywords_audit_mask = 0x0030000000000000L;

/**
 * @brief raw event data constructed from EvtSubscribe returned handle
 * It will be converted to event class if it's allowed by filters
 */
class event_data : public testable {
  DWORD _property_count;
  void* _data;

 protected:
  /**
   * @brief Used only for tests
   */
  event_data() : _property_count(0) {}

 public:
  event_data(EVT_HANDLE render_context,
             EVT_HANDLE event_handle,
             void** buffer,
             DWORD* buffer_size);

  ~event_data() = default;

  static std::chrono::file_clock::time_point convert_to_tp(uint64_t file_time);

  // all getters are virtual in order to mock it in ut
  virtual std::wstring_view get_provider() const;
  virtual uint16_t get_event_id() const;
  virtual uint8_t get_level() const;
  virtual uint16_t get_task() const;
  virtual int64_t get_keywords() const;
  virtual uint64_t get_time_created() const;
  virtual uint64_t get_record_id() const;
  virtual std::wstring_view get_computer() const;
  virtual std::wstring_view get_channel() const;  // file
};

/**
 * @brief Event class constructed
 * from event_data and filtered by event_filter
 */
class event : public testable {
  uint16_t _event_id;
  uint64_t _record_id;
  int64_t _keywords;

  std::chrono::file_clock::time_point _time;

  uint64_t _audit;
  unsigned _level;

  struct computer_tag {};
  struct channel_tag {};
  struct provider_tag {};
  struct keyword_tag {};
  struct message_tag {};

  boost::flyweight<std::string,
                   common::absl_factory<true>,
                   boost::flyweights::tag<computer_tag>>
      _computer;
  boost::flyweight<std::string,
                   common::absl_factory<true>,
                   boost::flyweights::tag<channel_tag>>
      _channel;
  boost::flyweight<std::string,
                   common::absl_factory<true>,
                   boost::flyweights::tag<provider_tag>>
      _provider;
  boost::flyweight<std::string,
                   common::absl_factory<true>,
                   boost::flyweights::tag<keyword_tag>>
      _str_keywords;
  boost::flyweight<std::string,
                   common::absl_factory<true>,
                   boost::flyweights::tag<message_tag>>
      _message;

 public:
  event() = default;

  event(const event_data& raw_data,
        const std::chrono::file_clock::time_point& tp,
        std::string&& message);

  bool operator<(const event& other) const { return _time < other._time; }

  uint64_t get_record_id() const { return _record_id; }
  uint16_t get_event_id() const { return _event_id; }
  std::chrono::file_clock::time_point get_time() const { return _time; }
  uint64_t get_audit() const { return _audit; }
  unsigned get_level() const { return _level; }
  const std::string& get_computer() const { return _computer; }
  const std::string& get_channel() const { return _channel; }
  const std::string& get_provider() const { return _provider; }
  int64_t get_keywords() const { return _keywords; }
  const std::string& get_str_keywords() const { return _str_keywords; }
  const std::string& get_message() const { return _message; }
};

std::ostream& operator<<(std::ostream& s, const event& evt);

/**
 * @brief event_filter
 * It can filter both event_data and event objects
 * The choice is made by using a raw_data_tag or an event_tag in constructor
 * It relies on filter classes. It only provides checkers or getter
 */
class event_filter {
  filters::filter_combinator _filter;
  std::shared_ptr<spdlog::logger> _logger;
  duration _written_limit;

  /**
   * @brief level_in
   * It's a functor that will be called by filter::apply_checker
   * It will build the checker for each filter like "level in ('error',
   * 'warning')" or "level == 'error'"
   * It converts error or warning in numeric values
   */
  template <filters::in_not rule, typename data_type_tag>
  class level_in {
    std::set<uint8_t> _values;

   public:
    using char_type = data_type_tag::char_type;
    level_in(const filters::label_in<char_type>& filt);
    level_in(const filters::label_compare_to_string<char_type>& filt);

    bool operator()(const testable& t) const;
  };

 public:
  /**
   * @brief check_builder
   * It's a functor that will be called by filter::apply_checker
   * It will build the checker for each filter along filter label
   */
  template <typename data_type_tag>
  struct check_builder {
    using char_type = data_type_tag::char_type;
    check_builder() = default;
    check_builder(const check_builder&) = delete;
    check_builder& operator=(const checker_builder&) = delete;

    duration min_written{0};
    void operator()(filter* filt);
    void set_label_compare_to_value(filters::label_compare_to_value* filt);
    void set_label_compare_to_string(
        filters::label_compare_to_string<char_type>* filt) const;
    void set_label_in(filters::label_in<char_type>* filt) const;
  };

  struct raw_data_tag {
    using type = event_data;
    using char_type = wchar_t;
    static constexpr bool use_wchar = true;
    static constexpr std::wstring_view audit_success = L"auditsuccess";
    static constexpr std::wstring_view audit_failure = L"auditfailure";
  };
  struct event_tag {
    using type = event;
    using char_type = char;
    static constexpr bool use_wchar = false;
    static constexpr std::string_view audit_success = "auditsuccess";
    static constexpr std::string_view audit_failure = "auditfailure";
  };

  template <typename data_tag_type>
  event_filter(const data_tag_type& tag,
               const std::string_view& filter_str,
               const std::shared_ptr<spdlog::logger>& logger);

  bool allow(const testable& data) const { return _filter.check(data); }

  void dump(std::ostream& s) const { _filter.dump(s); }

  void visit(const visitor& visitr) const { _filter.visit(visitr); }

  duration get_written_limit() const { return _written_limit; }
};

/**
 * @brief Construct a new event filter object
 *
 * This constructor initializes an event_filter object with the given data tag
 * type, filter string, and logger. It parses the filter string to create a
 * filter and applies a checker to the filter. If the filter string cannot be
 * parsed, it throws an exception. It also sets the written limit based on the
 * minimum written duration found by the checker builder.
 *
 * @tparam data_tag_type The type of data tag used for the filter (raw_data_tag
 * or event_tag).
 * @param tag The data tag type used to specify the type of data being filtered.
 * @param filter_str The filter string used to create the filter.
 * @param logger A shared pointer to the logger used fo logging.
 *
 * @throws exceptions::msg_fmt If the filter string cannot be parsed.
 */
template <typename data_tag_type>
event_filter::event_filter(const data_tag_type& tag,
                           const std::string_view& filter_str,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _logger(logger), _written_limit{0} {
  if (!filter::create_filter(filter_str, logger, &_filter,
                             data_tag_type::use_wchar)) {
    throw exceptions::msg_fmt("fail to parse filter string: {}", filter_str);
  }
  try {
    check_builder<data_tag_type> builder;
    // we use this lambda to avoir copy of builder
    _filter.apply_checker([&builder](filter* filt) { builder(filt); });
    _written_limit = builder.min_written;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "wrong_value for {}: {}", filter_str, e.what());
    throw;
  }
}

inline std::ostream& operator<<(std::ostream& s, const event_filter& filt) {
  filt.dump(s);
  return s;
}

}  // namespace com::centreon::agent::event_log

#endif
