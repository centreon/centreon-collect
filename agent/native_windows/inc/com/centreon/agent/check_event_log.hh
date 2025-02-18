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

#ifndef CENTREON_AGENT_CHECK_EVENT_LOG_HH
#define CENTREON_AGENT_CHECK_EVENT_LOG_HH

#include <winevt.h>

#include <boost/flyweight.hpp>
#include <string_view>

#include "check.hh"
#include "filter.hh"

namespace com::centreon::agent {

namespace check_event_log_detail {

using time_point = std::chrono::time_point<std::chrono::system_clock>;

constexpr uint64_t _keywords_filter = 0x00FFFFFFFFFFFFFFL;
constexpr uint64_t _keywords_audit_success = 0x0020000000000000L;
constexpr uint64_t _keywords_audit_failure = 0x0010000000000000L;

class event_data : public testable {
  std::unique_ptr<uint8_t[]> _data;
  DWORD _property_count;

 public:
  event_data(EVT_HANDLE render_context, EVT_HANDLE event_handle);

  std::wstring_view get_provider() const;
  uint16_t get_event_id() const;
  uint8_t get_level() const;
  uint16_t get_task() const;
  int64_t get_keywords() const;
  uint64_t get_time_created() const;
  uint64_t get_record_id() const;
  std::wstring_view get_computer() const;
  std::wstring_view get_channel() const;  // file
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
};

class event {
  uint64_t _id;

  time_point _time;

  uint64_t _audit;
  unsigned _level;

  e_status _status;

  struct computer_tag {};
  struct file_tag {};
  struct source_tag {};
  struct keyword_tag {};
  struct message_tag {};

  boost::flyweight<std::string, boost::flyweights::tag<computer_tag>> _computer;
  boost::flyweight<std::string, boost::flyweights::tag<file_tag>> _file;
  boost::flyweight<std::string, boost::flyweights::tag<source_tag>> _source;
  boost::flyweight<std::string, boost::flyweights::tag<keyword_tag>> _keyword;
  boost::flyweight<std::string, boost::flyweights::tag<message_tag>> _message;

 public:
  event(EVT_HANDLE render_context, EVT_HANDLE event_handle);
};

struct event_container {};

}  // namespace check_event_log_detail

class check_event_log : public check {
 public:
  check_event_log(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  time_point first_start_expected,
                  duration check_interval,
                  const std::string& serv,
                  const std::string& cmd_name,
                  const std::string& cmd_line,
                  const rapidjson::Value& args,
                  const engine_to_agent_request_ptr& cnf,
                  check::completion_handler&& handler,
                  const checks_statistics::pointer& stat);

  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  e_status compute(uint64_t ms_uptime,
                   std::string* output,
                   common::perfdata* perfs);
};

}  // namespace com::centreon::agent

#endif
