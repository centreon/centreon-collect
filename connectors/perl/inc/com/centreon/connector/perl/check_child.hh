/*
** Copyright 2026 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCCP_CHECK_CHILD_HH
#define CCCP_CHECK_CHILD_HH

#include "com/centreon/common/process/fork.hh"
#include "com/centreon/connector/perl/protocol.hh"

#include "connectors/perl/src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

/**
 * @brief Forked worker that executes a single Perl check script on behalf of
 *        a script_child instance.
 *
 * The class sits on top of fork<false> and owns both sides of the
 * parent/child boundary:
 *
 *   - **Parent side** — receives protobuf results from the child's stdout
 *     pipe via _on_stdout_read(), notifies the owner through the registered
 *     callbacks, and tracks whether a check is currently in flight
 *     (_running).
 *
 *   - **Child side** — _run() drives a synchronous request-reply loop:
 *     read an execute request from the parent, invoke the compiled Perl
 *     subroutine through _check_script_handle, and write the result back.
 *
 * A check_child is considered *idle* when is_running() returns false;
 * script_child uses this flag to dispatch new requests without creating
 * unnecessary processes.
 */
class check_child : public com::centreon::common::fork<false, true> {
  struct load {
    size_t used_memory = 0;
    size_t nb_thread = 0;
    size_t nb_opened_fd = 0;
  };

  protocol _protocol;
  const std::string _script_path;

  // parent side
  bool _running = false;
  unsigned _execute_counter = 0;

  using parent_read_handler =
      std::function<void(int /* pid*/, const ConnectorMess&)>;
  parent_read_handler _parent_read_handler;
  using end_child_handler = std::function<void(int /* pid*/)>;
  end_child_handler _parent_end_child_handler;

  void _on_stdout_read(const boost::system::error_code& err,
                       const std::string received) override;
  void _on_process_end() override;

  // child side (that executes checks)

  void* _check_script_handle = nullptr;
  std::optional<load> _after_first_check_load;

  static load measure_load();

  // child side
  int _run(int stdin_fd, int stdout_fd, int stderr_fd) override;

 public:
  template <typename read_handler, typename end_handler>
  check_child(const std::shared_ptr<asio::io_context> io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              const std::string& script_path,
              void* check_script_handle,
              read_handler&& readhandler,
              end_handler&& endhandler);

  bool is_running() const { return _running; }

  unsigned execute_counter() const { return _execute_counter; }

  void execute(const ConnectorMess& stdin_mess);
};

template <typename read_handler, typename end_handler>
check_child::check_child(const std::shared_ptr<asio::io_context> io_context,
                         const std::shared_ptr<spdlog::logger>& logger,
                         const std::string& script_path,
                         void* check_script_handle,
                         read_handler&& readhandler,
                         end_handler&& endhandler)
    : com::centreon::common::fork<false, true>(io_context, logger),
      _script_path(script_path),
      _parent_read_handler(readhandler),
      _parent_end_child_handler(endhandler),
      _check_script_handle(check_script_handle) {}

}  // namespace com::centreon::connector::perl
#endif
