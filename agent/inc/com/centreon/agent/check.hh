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

#ifndef CENTREON_AGENT_CHECK_HH
#define CENTREON_AGENT_CHECK_HH

#include "agent.pb.h"
#include "com/centreon/common/perfdata.hh"

namespace com::centreon::agent {

using engine_to_agent_request_ptr =
    std::shared_ptr<com::centreon::agent::MessageToAgent>;

using time_point = std::chrono::system_clock::time_point;
using duration = std::chrono::system_clock::duration;

class checks_statistics {
  struct check_stat {
    std::string cmd_name;
    duration last_check_interval;
    duration last_check_duration;
  };

  using statistic_container = multi_index::multi_index_container<
      check_stat,
      multi_index::indexed_by<
          multi_index::hashed_unique<
              BOOST_MULTI_INDEX_MEMBER(check_stat, std::string, cmd_name)>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              check_stat,
              duration,
              last_check_interval)>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              check_stat,
              duration,
              last_check_duration)>>>;

  statistic_container _stats;

 public:
  using pointer = std::shared_ptr<checks_statistics>;

  void add_interval_stat(const std::string& cmd_name,
                         const duration& check_interval);

  void add_duration_stat(const std::string& cmd_name,
                         const duration& check_interval);

  const auto& get_ordered_by_interval() const { return _stats.get<1>(); }
  const auto& get_ordered_by_duration() const { return _stats.get<2>(); }

  size_t size() const { return _stats.size(); }
};

/**
 * @brief nagios status values
 *
 */
enum e_status : unsigned { ok = 0, warning = 1, critical = 2, unknown = 3 };

/**
 * @brief in order to have a non derive scheduling, we use this class to iterate
 * time to time in case of we want to schedule an event every 30s for example
 *
 */
class time_step {
  time_point _start_point;
  duration _step;
  uint64_t _step_index = 0;

 public:
  /**
   * @brief Construct a new time step object
   *
   * @param start_point this time_point is the first time_point of the sequence
   * @param step value() will return start_point + step * step_index
   */
  time_step(time_point start_point, duration step)
      : _start_point(start_point), _step(step) {}

  time_step() : _start_point(), _step() {}

  /**
   * @brief increment time of one duration (one step)
   *
   * @return time_step&
   */
  time_step& operator++() {
    ++_step_index;
    return *this;
  }

  /**
   * @brief set _step_index to the first step after or equal to now
   *
   */
  void increment_to_after_now() {
    time_point now = std::chrono::system_clock::now();
    _step_index =
        (now - _start_point + _step - std::chrono::microseconds(1)) / _step;
  }

  /**
   * @brief set _step_index to the first step after or equal to min_tp
   *
   */
  void increment_to_after_min(time_point min_tp) {
    _step_index =
        (min_tp - _start_point + _step - std::chrono::microseconds(1)) / _step;
  }

  time_point value() const { return _start_point + _step_index * _step; }

  uint64_t get_step_index() const { return _step_index; }

  duration get_step() const { return _step; }
};

/**
 * @brief base class for check
 * start_expected is set by scheduler and increased by check_period on each
 * check
 *
 */
class check : public std::enable_shared_from_this<check> {
 public:
  using completion_handler = std::function<void(
      const std::shared_ptr<check>& caller,
      int status,
      const std::list<com::centreon::common::perfdata>& perfdata,
      const std::list<std::string>& outputs)>;

 private:
  //_start_expected is set on construction on config receive
  // it's updated on check_start and added of multiple of check_interval
  // (check_period / nb_check) on check completion
  time_step _start_expected;
  const std::string& _service;
  const std::string& _command_name;
  const std::string& _command_line;
  // by owning a reference to the original request, we can get only reference to
  // host, service and command_line
  // on completion, this pointer is compared to the current config pointer.
  // if not equal result is not processed
  engine_to_agent_request_ptr _conf;

  asio::system_timer _time_out_timer;

  void _start_timeout_timer(const duration& timeout);

  bool _running_check = false;
  // this index is used and incremented by on_completion to insure that
  // async on_completion is called by the actual asynchronous check
  unsigned _running_check_index = 0;
  completion_handler _completion_handler;

  // statistics used by check_health
  time_point _last_start;
  checks_statistics::pointer _stat;

 protected:
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  unsigned _get_running_check_index() const { return _running_check_index; }
  const completion_handler& _get_completion_handler() const {
    return _completion_handler;
  }

  virtual void _timeout_timer_handler(const boost::system::error_code& err,
                                      unsigned start_check_index);

  bool _start_check(const duration& timeout);

  virtual void _on_timeout(){};

 public:
  using pointer = std::shared_ptr<check>;

  static const std::array<std::string_view, 4> status_label;

  check(const std::shared_ptr<asio::io_context>& io_context,
        const std::shared_ptr<spdlog::logger>& logger,
        time_point first_start_expected,
        duration check_interval,
        const std::string& serv,
        const std::string& command_name,
        const std::string& cmd_line,
        const engine_to_agent_request_ptr& cnf,
        completion_handler&& handler,
        const checks_statistics::pointer& stat);

  virtual ~check() = default;

  struct pointer_start_compare {
    bool operator()(const check::pointer& left,
                    const check::pointer& right) const {
      return left->_start_expected.value() < right->_start_expected.value();
    }
  };

  void increment_start_expected_to_after_min_timepoint(time_point min_tp) {
    _start_expected.increment_to_after_min(min_tp);
  }

  void add_check_interval_to_start_expected() { ++_start_expected; }

  time_point get_start_expected() const { return _start_expected.value(); }

  const time_step& get_raw_start_expected() const { return _start_expected; }

  const std::string& get_service() const { return _service; }

  const std::string& get_command_name() const { return _command_name; }

  const std::string& get_command_line() const { return _command_line; }

  const engine_to_agent_request_ptr& get_conf() const { return _conf; }

  const time_point& get_last_start() const { return _last_start; }

  void on_completion(unsigned start_check_index,
                     unsigned status,
                     const std::list<com::centreon::common::perfdata>& perfdata,
                     const std::list<std::string>& outputs);

  virtual void start_check(const duration& timeout) = 0;

  static std::optional<double> get_double(const std::string& cmd_name,
                                          const char* field_name,
                                          const rapidjson::Value& val,
                                          bool must_be_positive);

  static std::optional<bool> get_bool(const std::string& cmd_name,
                                      const char* field_name,
                                      const rapidjson::Value& val);

  const checks_statistics& get_stats() const { return *_stat; }
};

}  // namespace com::centreon::agent

#endif
