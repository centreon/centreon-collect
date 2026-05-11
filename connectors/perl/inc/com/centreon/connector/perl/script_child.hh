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

#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <boost/asio/io_context.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <memory>
#include "check_child.hh"
#include "src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

/**
 * @brief Comparator that orders ConnectorMess by ascending execute timeout.
 *
 * Used as the key comparator for the _execute_queue btree_multiset so that
 * requests closest to their deadline are dispatched first when an idle
 * check_child becomes available.
 */
struct timeout_connector_mess_compare {
  bool operator()(const std::shared_ptr<ConnectorMess>& left,
                  const std::shared_ptr<ConnectorMess>& right) const {
    return left->execute().timeout() < right->execute().timeout();
  }
};

/**
 * @brief Forked process that owns a compiled Perl interpreter and manages a
 *        pool of check_child workers for a single check script.
 *
 * The class inherits fork<false> and operates on two distinct sides:
 *
 *   - **Parent side** (main process after fork) — compiles the loader and the
 *     check script into the embedded Perl interpreter, then communicates with
 *     the child via protobuf messages over stdin/stdout pipes. Callbacks
 *     (_parent_read_handler, _parent_end_child_handler) propagate results and
 *     end-of-life events back to the owning policy.
 *
 *   - **Child side** (inside the forked process) — runs an asio event loop
 *     that dispatches incoming execute requests to idle check_child workers,
 *     queues excess requests ordered by timeout, and forwards results to the
 *     main process. A one-minute timer watches for script file modifications
 *     and triggers a reload by sending a have_to_terminate message.
 *
 * One script_child instance corresponds to exactly one Perl script file. The
 * owning policy creates a new instance whenever the script is updated or the
 * child exits unexpectedly.
 */
class script_child : public com::centreon::common::fork<false, true> {
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
      std::function<void(const std::string& /* _script_path*/, int /*pid*/)>;
  end_child_handler _parent_end_child_handler;

  // child side (compiled script)
  // caution after a fork parent _io_context bugs, so we create a new one
  // especially for child process
  std::shared_ptr<asio::io_context> _child_io_context;
  std::unique_ptr<asio::system_timer> _every_second_timer;
  std::unique_ptr<asio::writable_pipe> _child_stdout;
  std::unique_ptr<asio::readable_pipe> _child_stdin;
  absl::btree_multiset<std::shared_ptr<ConnectorMess>,
                       timeout_connector_mess_compare>
      _execute_queue;

  struct pending_execute {
    pending_execute() {}

    pending_execute(int in_pid, const std::shared_ptr<ConnectorMess> in_query)
        : pid(in_pid), query(in_query) {}

    int pid;
    std::shared_ptr<ConnectorMess> query;

    int64_t timeout() const { return query->execute().timeout(); }
  };

  using pending_cont = boost::multi_index::multi_index_container<
      pending_execute,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<
              BOOST_MULTI_INDEX_MEMBER(pending_execute, int, pid)>,
          boost::multi_index::ordered_non_unique<
              boost::multi_index::const_mem_fun<pending_execute,
                                                int64_t,
                                                &pending_execute::timeout>>>>;

  pending_cont _pending;

  using pid_to_check_child =
      absl::flat_hash_map<int, std::shared_ptr<check_child>>;
  pid_to_check_child _check_childs;

  using die_start_to_check_child =
      absl::btree_multimap<time_t, std::shared_ptr<check_child>>;
  die_start_to_check_child _die_start_to_check_child;

  // parent side (main process)
  void _compile_script(const std::string& loader_path);
  void _load_check_script();
  std::string _write_loader_to_disk(const std::string_view& additional_code);
  void _on_stdout_read(const boost::system::error_code& err,
                       const std::string received) override;
  void _on_process_end() override;

  // child side
  void read_from_main_process_stdin();

  int _run(int stdin_fd, int stdout_fd, int stderr_fd) override;

  void _start_every_second_timer();
  void _every_second_timer_handler();

  void _on_stdin_receive(const boost::system::error_code& err,
                         const std::shared_ptr<ConnectorMess>& mess);

  void _from_child_script_receive(int pid, const ConnectorMess& received);
  void _on_child_script_end(int pid);

  void _send_to_main_process(const ConnectorMess& to_send);

  void _clean_execute_queue();

  void _create_child_and_execute(const std::shared_ptr<ConnectorMess>& query);

  void _kill_check_child(bool erase_from_check_childs,
                         const std::shared_ptr<check_child>& to_kill);

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
        com::centreon::common::fork<false, true>::shared_from_this());
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
    : com::centreon::common::fork<false, true>(io_context, logger),
      _script_path(script_path),
      _additional_code(additional_code),
      _parent_read_handler(readhandler),
      _parent_end_child_handler(endhandler) {}

}  // namespace com::centreon::connector::perl
#endif
