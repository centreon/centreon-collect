/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_COMMANDS_CONNECTOR_HH
#define CCE_COMMANDS_CONNECTOR_HH

#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>
#include <boost/asio/system_context.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <memory>
#include "com/centreon/broker/timestamp.hh"
#include "com/centreon/common/process/process.hh"
#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/timestamp.hh"

namespace com::centreon::engine {
namespace commands {
class connector;
}
}  // namespace com::centreon::engine

typedef std::unordered_map<
    std::string,
    std::shared_ptr<com::centreon::engine::commands::connector>>
    connector_map;

namespace com::centreon::engine {

namespace commands {
/**
 *  @class connector commands/connector.hh
 *  @brief Command is a specific implementation of commands::command.
 *
 *  connector is a specific implementation of commands::command that
 *  is more efficient than a raw command. A connector is an external software
 *  that launches checks and returns when available their result. For example,
 *  we have a perl connector. Since, it centralizes various checks, it compiles
 *  them while reading and then they are are already compiled and can be
 *  executed faster.
 *
 *  A connector usually executes many checks, whereas a commands::raw works
 *  with only one check. This is a significant difference.
 *
 *  To exchange with scripts executed by the connector, we use specific commands
 *  to the connector. Those internal functions all begins with _recv_query_ or
 *  _send_query_.
 *
 *  The connector is connected to an external program named connector and also
 *  has an internal thread used to restart the connector if needed. So, we have
 *  several variables to control all that:
 *  * _is_running: the connector is up and running.
 *  * _try_to_restart: This boolean tells if the connector is stopped, if we
 *    should start it again. Usually it is the case but when we want to stop
 *    everything, we don't want.
 *  * _thread_running: The thread is running, so it is possible to restart the
 *    the connector.
 *  * _thread_action: This enum asks the thread to start/stop the connector.
 *  * _thread_cv: Here is the condition variable used in pair with
 *    _thread_action.
 *
 */
class connector : public command {
  struct query_info {
    query_info() {}

    query_info(uint64_t commandid,
               std::string processedcmd,
               timestamp starttime,
               uint32_t time_out,
               bool waitingresult)
        : command_id(commandid),
          processed_cmd(processedcmd),
          start_time(starttime),
          timeout(time_out),
          abs_timeout(starttime + time_out),
          waiting_result((waitingresult)) {}

    uint64_t command_id;
    std::string processed_cmd;
    timestamp start_time;
    uint32_t timeout;
    timestamp abs_timeout;
    bool waiting_result;
  };

  std::string _data_available ABSL_GUARDED_BY(_lock);

  using active_queries = boost::multi_index::multi_index_container<
      query_info,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<
              BOOST_MULTI_INDEX_MEMBER(query_info, uint64_t, command_id)>,
          boost::multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(query_info, timestamp, abs_timeout)>>>;

  active_queries _queries ABSL_GUARDED_BY(_lock);

  enum class e_state { not_started, starting, running };

  e_state _state ABSL_GUARDED_BY(_lock);

  std::shared_ptr<common::process<true>> _process ABSL_GUARDED_BY(_lock);

  absl::flat_hash_map<uint64_t, result> _results ABSL_GUARDED_BY(_results_m);

  mutable absl::Mutex _results_m;
  std::shared_ptr<spdlog::logger> _logger;
  std::shared_ptr<asio::io_context> _io_context;
  asio::system_timer _second_timer ABSL_GUARDED_BY(_lock);

  void _connector_close();
  void _connector_start_nolock() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_lock);
  void _recv_query_error(const std::string_view& data);
  void _recv_query_execute(const std::string_view& data);
  void _recv_query_quit(const std::string_view&);
  void _recv_query_version(const std::string_view& data);
  void _send_query_execute_nolock(std::string const& cmdline,
                                  uint64_t command_id,
                                  timestamp const& start,
                                  uint32_t timeout)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_lock);
  void _send_query_quit_nolock() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_lock);
  void _send_query_version_nolock() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_lock);
  void _on_stdout_recv(const std::shared_ptr<common::process<true>>& caller,
                       const std::string_view& data);
  void _on_stderr_recv(const std::shared_ptr<common::process<true>>& caller,
                       const std::string_view& data);
  void _on_process_end(const common::process<true>& caller);

  void _second_timer_start();
  void _second_timer_handler();

  connector(std::string const& connector_name,
            std::string const& connector_line,
            const std::shared_ptr<asio::io_context>& io_context,
            command_listener* listener = nullptr);

 public:
  static std::shared_ptr<connector> load(
      std::string const& connector_name,
      std::string const& connector_line,
      const std::shared_ptr<asio::io_context>& io_context,
      command_listener* listener = nullptr);

  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;

  ~connector();

  std::shared_ptr<connector> shared_from_this() {
    return std::static_pointer_cast<connector>(command::shared_from_this());
  }

  uint64_t run(std::string const& processed_cmd,
               nagios_macros& macros,
               uint32_t timeout,
               const check_result::pointer& to_push_to_checker,
               const notifier* caller = nullptr) override;
  void run(std::string const& processed_cmd,
           nagios_macros& macros,
           uint32_t timeout,
           result& res) override;
  void set_command_line(std::string const& command_line) override;

  static connector_map connectors;
  void stop_connector();
};
}  // namespace commands

}  // namespace com::centreon::engine

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::commands::connector const& obj);

#endif  // !CCE_COMMANDS_CONNECTOR_HH
