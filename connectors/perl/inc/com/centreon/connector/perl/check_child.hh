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
class check_child : public com::centreon::common::fork<false> {
  protocol _protocol;

  // parent side
  bool _running = false;

  using parent_read_handler =
      std::function<void(int /* pid*/, const ConnectorMess&)>;
  parent_read_handler _parent_read_handler;
  using end_child_handler = std::function<void(int /* pid*/)>;
  end_child_handler _parent_end_child_handler;

  void _on_stdout_read(const std::string received) override;
  void _on_process_end() override;

  // child side (that executes checks)
  struct load {
    size_t used_memory = 0;
    size_t nb_thread = 0;
    size_t nb_opened_fd = 0;
  };

  void* _check_script_handle = nullptr;
  load _after_first_check_load;

  static load measure_load();

  // child side
  int _run(int stdin_fd, int stdout_fd, int stderr_fd) override;

 public:
  template <typename read_handler, typename end_handler>
  check_child(const std::shared_ptr<asio::io_context> io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              void* check_script_handle,
              read_handler&& readhandler,
              end_handler&& endhandler);

  template <typename read_handler>
  check_child(const std::shared_ptr<asio::io_context> io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              void* check_script_handle)
      : com::centreon::common::fork<false>(io_context, logger),
        _check_script_handle(check_script_handle) {}

  bool is_running() const { return _running; }

  template <typename handler_type>
  void execute(const ConnectorMess& stdin_mess, handler_type&& handler);
};

template <typename read_handler, typename end_handler>
check_child::check_child(const std::shared_ptr<asio::io_context> io_context,
                         const std::shared_ptr<spdlog::logger>& logger,
                         void* check_script_handle,
                         read_handler&& readhandler,
                         end_handler&& endhandler)
    : com::centreon::common::fork<false>(io_context, logger),
      _parent_read_handler(readhandler),
      _parent_end_child_handler(endhandler),
      _check_script_handle(check_script_handle) {}

template <typename handler_type>
void check_child::execute(const ConnectorMess& stdin_mess,
                          handler_type&& handler) {
  _protocol.async_send(_stdin_pipe, stdin_mess, std::move(handler));
}

}  // namespace com::centreon::connector::perl
#endif
