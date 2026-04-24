/**
 * Copyright 2026 Centreon
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

#ifndef CCCP_SCRIPT_CHILD_HH
#define CCCP_SCRIPT_CHILD_HH

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include "check_child.hh"
#include "src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

struct timeout_connector_mess_compare {
  bool operator()(const std::shared_ptr<ConnectorMess>& left,
                  const std::shared_ptr<ConnectorMess>& right) const {
    return left->execute().timeout() < right->execute().timeout();
  }
};

class script_child : public com::centreon::common::fork<false> {
  const std::string _script_path;
  const std::string _additional_code;
  std::filesystem::file_time_type _check_script_mtime;
  void* _check_script_handle = nullptr;
  protocol _protocol;
  std::string _global_error;
  using parent_read_handler =
      std::function<void(const std::string& /* _script_path*/,
                         const ConnectorMess&)>;
  parent_read_handler _parent_read_handler;
  using end_child_handler =
      std::function<void(const std::string& /* _script_path*/)>;
  end_child_handler _parent_end_child_handler;

  // child side (compiled script)
  asio::system_timer _minute_timer;
  std::unique_ptr<asio::writable_pipe> _child_stdout;
  std::unique_ptr<asio::readable_pipe> _child_stdin;
  absl::btree_multiset<std::shared_ptr<ConnectorMess>,
                       timeout_connector_mess_compare>
      _execute_queue;
  absl::flat_hash_map<int /*pid*/, std::shared_ptr<ConnectorMess>> _pending;

  using pid_to_check_child =
      absl::flat_hash_map<int, std::shared_ptr<check_child>>;
  pid_to_check_child _check_childs;

  // parent side (main process)
  void _compile_script(const std::string& loader_path);
  void _load_check_script();
  std::string _write_loader_to_disk(const std::string_view& additional_code);
  void _on_stdout_read(const std::string received) override;
  void _on_process_end() override;

  // child side
  void read_from_main_process_stdin();

  int _run(int stdin_fd, int stdout_fd, int stderr_fd) override;

  void _start_minute_timer();
  void _minute_timer_handler();

  void _on_stdin_receive(const boost::system::error_code& err,
                         const std::shared_ptr<ConnectorMess>& mess);

  void _on_execute_send_error(int pid,
                              const std::shared_ptr<ConnectorMess>& mess);

  void _from_child_script_receive(int pid, const ConnectorMess& received);
  void _on_child_script_end(int pid);

  void _send_to_main_process(const ConnectorMess& to_send);

 public:
  template <typename read_handler, typename end_handler>
  script_child(const std::shared_ptr<asio::io_context> io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string& script_path,
               read_handler&& readhandler,
               end_handler&& endhandler,
               const std::string& additional_code);

  script_child(const script_child&) = delete;
  script_child& operator=(const script_child&) = delete;

  ~script_child();

  std::shared_ptr<script_child> shared_from_this() {
    return std::static_pointer_cast<script_child>(
        com::centreon::common::child_process<false>::shared_from_this());
  }

  void write_mess_to_child_stdin(const ConnectorMess& to_child_mess);
};

template <typename read_handler, typename end_handler>
script_child::script_child(const std::shared_ptr<asio::io_context> io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           const std::string& script_path,
                           read_handler&& readhandler,
                           end_handler&& endhandler,
                           const std::string& additional_code)
    : com::centreon::common::fork<false>(io_context, logger),
      _script_path(script_path),
      _additional_code(additional_code),
      _parent_read_handler(readhandler),
      _parent_end_child_handler(endhandler),
      _minute_timer(*_io_context) {}

}  // namespace com::centreon::connector::perl
#endif
