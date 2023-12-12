/**
 * Copyright 2011-2013 Merethis
 * Copyright 2023      Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_COMMANDS_COMMAND_HH
#define CCE_COMMANDS_COMMAND_HH

#include <boost/container/flat_map.hpp>

#include "com/centreon/engine/commands/command_listener.hh"
#include "com/centreon/engine/commands/result.hh"
#include "com/centreon/engine/macros/defines.hh"

namespace com::centreon::engine {
namespace commands {
class command;
}
}  // namespace com::centreon::engine

typedef std::unordered_map<
    std::string,
    std::shared_ptr<com::centreon::engine::commands::command>>
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
          command_listener* listener = nullptr);
  virtual ~command() noexcept;
  command(const command&) = delete;
  command& operator=(const command&) = delete;
  bool operator==(const command& right) const noexcept;
  bool operator!=(const command& right) const noexcept;
  virtual const std::string& get_command_line() const noexcept;
  virtual const std::string& get_name() const noexcept;
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
