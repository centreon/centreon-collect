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

class event_filter {
  filters::filter_combinator _filter;
  std::shared_ptr<spdlog::logger> _logger;

  struct check_builder {
    void operator()(filter* filt) const;
    void set_label_compare_to_value(
        filters::label_compare_to_value* filt) const;
    void set_label_compare_to_string(
        filters::label_compare_to_string<wchar_t>* filt) const;
    void set_label_in(filters::label_in<wchar_t>* filt) const;
  };

  template <filters::in_not rule>
  class level_in {
    std::set<uint8_t> _values;

   public:
    level_in(const filters::label_in<wchar_t>& filt);
    level_in(const filters::label_compare_to_string<wchar_t>& filt);

    bool operator()(const testable& t) const;
  };

 public:
  event_filter(const std::string_view& filter_str,
               const std::shared_ptr<spdlog::logger>& logger);

  bool allow(event_data& data) const { return _filter.check(data); }

  void dump(std::ostream& s) const { _filter.dump(s); }

  void visit(const visitor& visitr) const { _filter.visit(visitr); }
};

inline std::ostream& operator<<(std::ostream& s, const event_filter& filt) {
  filt.dump(s);
  return s;
}

class event {
  uint64_t _id;

  std::chrono::file_clock::time_point _time;

  uint64_t _audit;
  unsigned _level;

  e_status _status;

  struct computer_tag {};
  struct channel_tag {};
  struct provider_tag {};
  struct keyword_tag {};
  struct message_tag {};

  boost::flyweight<std::string,
                   boost::flyweights::hashed_factory<>,
                   boost::flyweights::tag<computer_tag>>
      _computer;
  boost::flyweight<std::string,
                   boost::flyweights::hashed_factory<>,
                   boost::flyweights::tag<channel_tag>>
      _channel;
  boost::flyweight<std::string,
                   boost::flyweights::hashed_factory<>,
                   boost::flyweights::tag<provider_tag>>
      _provider;
  boost::flyweight<std::string,
                   boost::flyweights::hashed_factory<>,
                   boost::flyweights::tag<keyword_tag>>
      _keyword;
  boost::flyweight<std::string,
                   boost::flyweights::hashed_factory<>,
                   boost::flyweights::tag<message_tag>>
      _message;

 public:
  event(const event_data& raw_data, e_status status, std::string&& message);

  bool operator<(const event& other) const { return _time < other._time; }

  uint64_t id() const { return _id; }
  std::chrono::file_clock::time_point time() const { return _time; }
  uint64_t audit() const { return _audit; }
  unsigned level() const { return _level; }
  e_status status() const { return _status; }
  const std::string& computer() const { return _computer; }
  const std::string& channel() const { return _channel; }
  const std::string& provider() const { return _provider; }
  const std::string& keyword() const { return _keyword; }
  const std::string& message() const { return _message; }
};

std::ostream& operator<<(std::ostream& s, const event& evt);

}  // namespace com::centreon::agent::event_log

#endif
