/**
 * Copyright 2011-2024 Centreon
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

#ifndef CCE_COMMANDS_COMMAND_HH
#define CCE_COMMANDS_COMMAND_HH

#include "com/centreon/engine/commands/command_listener.hh"
#include "com/centreon/engine/macros/defines.hh"

namespace com::centreon::engine::commands {
class command;
}  // namespace com::centreon::engine::commands

typedef std::unordered_map<
    std::string,
    std::shared_ptr<com::centreon::engine::commands::command> >
    command_map;

namespace com::centreon::engine::commands {

/**
 *  @class command command.hh
 *  @brief Execute command and send the result.
 *
 *  Command execute a command line with their arguments and
 *  notify listener at the end of the command.
 */
class command {
 public:
  enum class e_type { exec, forward, raw, connector, otel };
  const e_type _type;

 protected:
  static uint64_t get_uniq_id();

  std::mutex _lock;
  std::string _command_line;
  command_listener* _listener;
  std::string _name;

  /**
   * @brief the goal of this structure is to ensure that checks shared by
   * anomalydetection and service are not called to often
   *
   */
  struct last_call {
    using pointer = std::shared_ptr<last_call>;
    time_t launch_time;
    std::shared_ptr<result> res;

    last_call() : launch_time(0) {}
  };

  /**
   * @brief this map is used to group service and anomalydetection in order to
   * insure that both don't call check too often
   *
   */
  using caller_to_last_call_map =
      boost::container::flat_map<const void*, last_call::pointer>;
  caller_to_last_call_map _result_cache;

  using cmdid_to_last_call_map =
      boost::container::flat_map<uint64_t, last_call::pointer>;
  cmdid_to_last_call_map _current;

  bool gest_call_interval(uint64_t command_id,
                          const check_result::pointer& to_push_to_checker,
                          const void* caller);
  void update_result_cache(uint64_t command_id, const result& res);

 public:
  using pointer = std::shared_ptr<command>;

  command(const std::string& name,
          const std::string& command_line,
          command_listener* listener = nullptr,
          e_type cmd_type = e_type::exec);
  virtual ~command() noexcept;
  command(const command&) = delete;
  command& operator=(const command&) = delete;
  bool operator==(const command& right) const noexcept;
  bool operator!=(const command& right) const noexcept;
  virtual const std::string& get_command_line() const noexcept;
  virtual const std::string& get_name() const noexcept;
  e_type get_type() const { return _type; }
  virtual std::string process_cmd(nagios_macros* macros) const;
  virtual uint64_t run(const std::string& processed_cmd,
                       nagios_macros& macors,
                       uint32_t timeout,
                       const check_result::pointer& to_push_to_checker,
                       const void* caller = nullptr) = 0;
  virtual void run(const std::string& process_cmd,
                   nagios_macros& macros,
                   uint32_t timeout,
                   result& res) = 0;

  /**
   * @brief connector and host serv extractor share a list of host serv which is
   * updated by this method and unregister_host_serv
   * This method add an entry in this list
   * Command is the only thing that hosts and service knows.
   * So we use it to update host serv list used by host serv extractors
   *
   * @param host
   * @param service_description empty for host command
   */
  virtual void register_host_serv(const std::string& host [[maybe_unused]],
                                  const std::string& service_description
                                  [[maybe_unused]]) {}

  /**
   * @brief Remove an entry for host serv list shared between this connector and
   * host serv extractor
   *
   * @param host
   * @param service_description empty for host command
   */
  virtual void unregister_host_serv(const std::string& host [[maybe_unused]],
                                    const std::string& service_description
                                    [[maybe_unused]]) {}

  template <typename caller_iterator>
  void add_caller_group(caller_iterator begin, caller_iterator end);
  void remove_caller(void* caller);

  virtual void set_command_line(const std::string& command_line);
  void set_listener(command_listener* listener) noexcept;
  static command_map commands;
};

template <typename caller_iterator>
void command::add_caller_group(caller_iterator begin, caller_iterator end) {
  last_call::pointer obj{std::make_shared<last_call>()};

  std::unique_lock<std::mutex> l(_lock);
  for (; begin != end; ++begin) {
    const void* caller = static_cast<const void*>(*begin);
    _result_cache[caller] = obj;
  }
}

std::ostream& operator<<(std::ostream& s, const command& cmd);

inline std::ostream& operator<<(std::ostream& s, const command::pointer& cmd) {
  s << *cmd;
  return s;
}

}  // namespace com::centreon::engine::commands

namespace fmt {
template <>
struct formatter<com::centreon::engine::commands::command> : ostream_formatter {
};
template <>
struct formatter<com::centreon::engine::commands::command::pointer>
    : ostream_formatter {};
}  // namespace fmt

#endif  // !CCE_COMMANDS_COMMAND_HH
