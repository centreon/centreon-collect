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
#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/numbers.h>
#include <re2/re2.h>
#include <spdlog/spdlog.h>
#include <memory>

#include "com/centreon/common/process/process_args.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/connector/perl/script_child.hh"
#include "common/inc/com/centreon/common/file_system.hh"
#include "connectors/precomp_inc/precomp.hh"
#include "src/perl_connector.pb.h"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;

policy::check_child_stat::check_child_stat(
    const std::shared_ptr<script_child>& prent,
    const Result& res)
    : parent(prent),
      check_child_pid(res.pid()),
      last_used(time(nullptr)),
      footprint(res.afterlastcheck().used_memory(),
                res.afterlastcheck().nb_opened_fd(),
                res.afterlastcheck().nb_thread()) {}

/**
 * @brief This function evaluate free memory by parsing /proc/meminfo
 * It's a weak function on order to be override in tests
 *
 */
__attribute__((weak)) int64_t get_free_memory() {
  std::string mem_info = common::read_file_content("/proc/meminfo");
  static re2::RE2 mem_parser("MemAvailable:\\s+(\\d+)");
  int64_t free_mem = 0;
  if (re2::RE2::PartialMatch(mem_info, mem_parser, &free_mem)) {
    return free_mem * 1024;
  }
  return 0;
}

/**
 *  Default constructor.
 */
policy::policy(const shared_io_context& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const config& conf,
               char* argv0,
               int stdin_fd,
               int stdout_fd,
               bool stop_io_context_on_quit)
    : _reporter(reporter::create(io_context, stdout_fd)),
      _io_context(io_context),
      _logger(logger),
      _stdin_fd(stdin_fd),
      _every_second_timer(*io_context),
      _config(conf),
      _argv0(argv0),
      _stop_io_context_on_quit(stop_io_context_on_quit) {
  SPDLOG_LOGGER_DEBUG(logger, "Create policy {:p}",
                      static_cast<const void*>(this));
}

policy::~policy() {
  SPDLOG_LOGGER_DEBUG(_logger, "Delete policy {:p}",
                      static_cast<const void*>(this));
}

void policy::create(const shared_io_context& io_context,
                    const std::shared_ptr<spdlog::logger>& logger,
                    const config& conf,
                    char* argv0,
                    int stdin_fd,
                    int stdout_fd,
                    bool stop_io_context_on_quit) {
  std::shared_ptr<policy> ret(new policy(io_context, logger, conf, argv0,
                                         stdin_fd, stdout_fd,
                                         stop_io_context_on_quit));
  ret->_start();
}

void policy::_start() {
  orders::parser::create(_io_context, shared_from_this(),
                         _config.test_file_path(), _stdin_fd);
  _start_every_second_timer();
}

/**
 *  Called if stdin is closed.
 */
void policy::on_eof() {
  SPDLOG_LOGGER_INFO(_logger, "stdin is closed");
  on_quit();
}

/**
 *  Called if an error occured on stdin.
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] msg    Associated message.
 */
void policy::on_error(uint64_t cmd_id, const std::string& msg) {
  if (cmd_id) {
    result r;
    r.set_command_id(cmd_id);
    r.set_executed(false);
    r.set_error(msg);
    _reporter->send_result(r);
  } else {
    SPDLOG_LOGGER_INFO(_logger, "error occurred while parsing stdin");
    on_quit();
  }
}

/**
 *  Execution command received.
 *
 *  @param[in] cmd_id  Command ID.
 *  @param[in] timeout Time the command has to execute.
 *  @param[in] cmd     Command to execute.
 */
void policy::on_execute(uint64_t cmd_id,
                        const time_point& timeout,
                        const std::string& cmdline) {
  // first extract executable
  std::string script_path = cmdline;
  size_t first_space = script_path.find(' ');
  if (first_space != std::string_view::npos) {
    script_path = script_path.substr(0, first_space);
  }
  absl::StripAsciiWhitespace(&script_path);

  auto& script_path_index = _scripts.get<0>();
  auto script = script_path_index.find(script_path);
  if (script == script_path_index.end()) {
    // script_child process close _stdin_fd. If we don't do that, stdin_fd
    // closed event will never happen as stdin_fd will remain opened in
    // script_child process
    script =
        script_path_index
            .emplace(std::make_shared<script_child>(
                _io_context, _logger, script_path,
                [weak_me = weak_from_this()](
                    const std::shared_ptr<script_child>& script_chld,
                    const ConnectorMess& mess) {
                  auto me = weak_me.lock();
                  if (me) {
                    std::static_pointer_cast<policy>(me)->_from_script_child(
                        script_chld, mess);
                  }
                },
                [weak_me = weak_from_this()](
                    const std::shared_ptr<script_child>& script_chld) {
                  auto me = weak_me.lock();
                  if (me) {
                    std::static_pointer_cast<policy>(me)->_on_script_child_end(
                        script_chld);
                  }
                },
                _config, _argv0, _stdin_fd))
            .first;
    (*script)->do_fork(false);
  }

  bool no_child_create = _free_memory(*script) < _config.min_free_memory();
  if (_check_child_stats.size() + _scripts.size() >= _config.max_child()) {
    no_child_create = true;
  }

  _pending_queries.emplace(cmd_id, script_path, timeout, *script);
  (*script)->write_mess_to_child_stdin(
      _create_execute(cmd_id, timeout, cmdline, no_child_create));
}

ConnectorMess policy::_create_execute(uint64_t cmd_id,
                                      const time_point& timeout,
                                      const std::string& cmdline,
                                      bool no_child_create) {
  ConnectorMess order;
  auto execute = order.mutable_execute();
  execute->set_cmd_id(cmd_id);
  com::centreon::common::process_args cmd_line(cmdline);
  execute->set_max_execute(_config.child_max_reuse_script());
  execute->set_percent_max_memory_increased(
      _config.child_max_memory_increase_percent());
  execute->set_percent_max_open_fd_increased(
      _config.child_max_fd_increase_percent());
  execute->set_max_thread(_config.child_max_thread());
  uint32_t specific_limit;
  for (auto arg_iter = cmd_line.get_args().begin();
       arg_iter != cmd_line.get_args().end(); ++arg_iter) {
    if (*arg_iter == "child-max-memory-increase-percent") {
      ++arg_iter;
      if (arg_iter != cmd_line.get_args().end()) {
        if (!absl::SimpleAtoi(*arg_iter, &specific_limit)) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "child-max-memory-increase-percent needs a "
                              "numeric argument instead of {}",
                              *arg_iter);
        } else {
          execute->set_percent_max_memory_increased(specific_limit);
        }
      }
    } else if (*arg_iter == "child-max-fd-increase-percent") {
      ++arg_iter;
      if (arg_iter != cmd_line.get_args().end()) {
        if (!absl::SimpleAtoi(*arg_iter, &specific_limit)) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "child-max-fd-increase-percent needs a "
                              "numeric argument instead of {}",
                              *arg_iter);
        } else {
          execute->set_percent_max_open_fd_increased(specific_limit);
        }
      }
    } else if (*arg_iter == "child-max-thread") {
      ++arg_iter;
      if (arg_iter != cmd_line.get_args().end()) {
        if (!absl::SimpleAtoi(*arg_iter, &specific_limit)) {
          SPDLOG_LOGGER_ERROR(
              _logger,
              "child-max-thread needs a numeric argument instead of {}",
              *arg_iter);
        } else {
          execute->set_max_thread(specific_limit);
        }
      }
    } else if (*arg_iter == "child-max-reuse-script") {
      ++arg_iter;
      if (arg_iter != cmd_line.get_args().end()) {
        if (!absl::SimpleAtoi(*arg_iter, &specific_limit)) {
          SPDLOG_LOGGER_ERROR(
              _logger,
              "child-max-reuse-script needs a numeric argument instead of {}",
              *arg_iter);
        } else {
          execute->set_max_execute(specific_limit);
        }
      }
    } else {
      execute->add_args(*arg_iter);
    }
  }
  execute->set_timeout(std::chrono::system_clock::to_time_t(timeout));

  execute->set_no_child_create(no_child_create);

  return order;
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  SPDLOG_LOGGER_INFO(_logger, "quit request received");
  _every_second_timer.cancel();
  // stop all scripts
  for (auto script : _scripts) {
    ConnectorMess terminate;
    terminate.mutable_terminate()->set_pid(script->get_pid());
    script->write_mess_to_child_stdin(terminate);
  }
  if (_stop_io_context_on_quit) {
    _io_context->stop();
  }
}

/**
 *  Version request was received.
 */
void policy::on_version() {
  // Report version 1.0.
  SPDLOG_LOGGER_INFO(
      _logger, "monitoring engine requested protocol version, sending 1.0");
  _reporter->send_version(1, 0);
}

void policy::_from_script_child(std::shared_ptr<script_child> script_chld,
                                const ConnectorMess& from_script_mess) {
  if (from_script_mess.has_have_to_terminate()) {
    auto& script_index = _scripts.get<1>();
    auto has_been = script_index.find(script_chld);
    if (has_been != script_index.end()) {
      _dying_scripts.emplace(*has_been);
      script_index.erase(has_been);
    }
    // export script_child failure to all pending checks
    auto& script_pid_index = _pending_queries.get<2>();
    auto check_errors = script_pid_index.equal_range(script_chld);
    for (; check_errors.first != check_errors.second; ++check_errors.first) {
      result res(check_errors.first->cmd_id, -1, "",
                 from_script_mess.have_to_terminate().error());
      res.set_executed(false);
      _reporter->send_result(res);
    }
    script_pid_index.erase(script_chld);

  } else if (from_script_mess.has_result()) {
    const auto& res = from_script_mess.result();
    check_child_stat new_stat(script_chld, res);
    auto res_insert = _check_child_stats.insert(new_stat);
    if (!res_insert.second) {
      _check_child_stats.modify(
          res_insert.first,
          [&new_stat](check_child_stat& to_update) { to_update = new_stat; });
    }
    _free_memory({});
    _reporter->send_result(
        result(res.cmd_id(), res.status(), res.stdout(), res.stderr()));
    _pending_queries.get<0>().erase(res.cmd_id());
  } else if (from_script_mess.has_child_end()) {
    _check_child_stats.get<1>().erase(from_script_mess.child_end().pid());
  }
}

void policy::_on_script_child_end(std::shared_ptr<script_child> script_chld) {
  int pid = script_chld->get_pid();
  SPDLOG_LOGGER_INFO(_logger, "end of script child pid={} {}", pid,
                     script_chld->get_script_path());
  _check_child_stats.get<0>().erase(script_chld);
  if (_dying_scripts.erase(script_chld) <= 0) {
    auto& script_index = _scripts.get<1>();
    auto bad_terminate = script_index.find(script_chld);
    if (bad_terminate != script_index.end()) {
      SPDLOG_LOGGER_ERROR(_logger, "end of script child pid={} {}", pid,
                          script_chld->get_script_path());
      script_index.erase(bad_terminate);
    }
  }
  auto& script_pid_index = _pending_queries.get<2>();
  auto check_errors = script_pid_index.equal_range(script_chld);
  for (; check_errors.first != check_errors.second; ++check_errors.first) {
    result res(check_errors.first->cmd_id, -1, "",
               fmt::format("script child {} has died",
                           script_chld->get_script_path()));
    res.set_executed(false);
    _reporter->send_result(res);
  }
  script_pid_index.erase(script_chld);
}

void policy::_start_every_second_timer() {
  _every_second_timer.expires_after(std::chrono::seconds(1));
  _every_second_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_every_second_timer_handler(err);
      });
}

void policy::_every_second_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }

  // timeout requests?
  if (!_pending_queries.empty()) {
    auto& timeout_index = _pending_queries.get<1>();
    auto limit = timeout_index.lower_bound(std::chrono::system_clock::now());
    for (auto timeout_request = timeout_index.begin(); timeout_request != limit;
         ++timeout_request) {
      _reporter->send_result(
          {timeout_request->cmd_id, 3, "", "(Process Timeout)"});
    }
    timeout_index.erase(timeout_index.begin(), limit);
  }

  // can we delete has been script child?
  auto& script_index = _pending_queries.get<2>();
  for (auto to_test = _dying_scripts.begin();
       to_test != _dying_scripts.end();) {
    if (script_index.find(*to_test) ==
        script_index.end()) {  // no pending query => kill
      (*to_test)->kill();
      _dying_scripts.erase(to_test++);
    } else {
      ++to_test;
    }
  }

  _start_every_second_timer();
}

/**
 * @brief remove check child if needed
 *
 * @param who_need_memory script child that may want to create a check child
 * (optional)
 * @return size_t amount of free memory now or after some check childs have been
 * died
 */
size_t policy::_free_memory(
    const std::shared_ptr<script_child>& who_need_memory) {
  size_t free_memory = get_free_memory();
  if (!free_memory) {
    SPDLOG_LOGGER_ERROR(_logger, "can't get free memory");
    return 0;
  }

  // idle check_child for this check?
  if (who_need_memory) {
    auto working_check_child =
        _pending_queries.get<2>().equal_range(who_need_memory);

    auto total_check_child =
        _check_child_stats.get<0>().equal_range(who_need_memory);
    if (std::distance(total_check_child.first, total_check_child.second) <=
        std::distance(working_check_child.first, working_check_child.second)) {
      // all check child busy => calculate memory used by another child
      unsigned average_memory_used_by_this_script = 0;
      unsigned nb_check_child_script = 0;

      for (; total_check_child.first != total_check_child.second;
           ++total_check_child.first, ++nb_check_child_script) {
        average_memory_used_by_this_script +=
            std::get<0>(total_check_child.first->footprint);
      }
      if (nb_check_child_script > 0) {
        free_memory -=
            average_memory_used_by_this_script / nb_check_child_script;
      }
    }
  }
  // need to sweep memory or too many childs
  if (free_memory < _config.min_free_memory()) {
    free_memory += _remove_heaviest_check_child();
  }
  if (_check_child_stats.size() + _scripts.size() >= _config.max_child()) {
    _remove_oldest_check_child();
  }
  return free_memory;
}

/**
 * @brief remove a check child
 *
 * @return size_t memory used by deleted check child
 */
size_t policy::_remove_heaviest_check_child() {
  /*When we used this function, we have a problem of out of memory.
  So we begin with check_child with max footprint. It selected one is not
  running and is not the only one child of a script, we send a Terminate query
  with immediate flag. If all are running, we send a Terminate query without
  immediate flag
  */

  auto& script_child_index = _check_child_stats.get<0>();
  auto& footprint_index = _check_child_stats.get<3>();
  absl::flat_hash_set<std::shared_ptr<script_child>> forbidden, round_forbidden;
  ConnectorMess child_script_terminate;
  auto term = child_script_terminate.mutable_terminate();
  std::shared_ptr<script_child> selected;
  unsigned round;
  size_t freed = 0;
  // round 0 search idle
  // round 1 search heaviest and delay kill
  for (round = 0; round < 2 && !selected; ++round) {
    for (auto load_iter = footprint_index.rbegin();
         !selected && load_iter != footprint_index.rend(); ++load_iter) {
      if (forbidden.contains(load_iter->parent) ||
          round_forbidden.contains(load_iter->parent)) {
        continue;
      }
      unsigned nb_check_child = script_child_index.count(load_iter->parent);
      if (nb_check_child < 2) {
        // only one check child for this script => don't touch
        forbidden.insert(load_iter->parent);
        continue;
      }
      // all busy for this script
      if (_pending_queries.get<2>().count(load_iter->parent) >=
          nb_check_child) {
        if (round == 1) {
          selected = load_iter->parent;
          // we give to script child list of check child reverse ordered by
          // footprint
          for (; load_iter != footprint_index.rend() &&
                 load_iter->parent == selected;
               ++load_iter) {
            term->add_other_pids(load_iter->check_child_pid);
          }
          freed = std::get<0>(load_iter->footprint);
        } else {
          round_forbidden.emplace(
              load_iter->parent);  // no need to re test this script child
        }
      } else {
        selected = load_iter->parent;  // we tell to script child to terminate
                                       // the first inactive check child
        // we give to script child list of check child reverse ordered by
        // footprint
        for (; load_iter != footprint_index.rend() &&
               load_iter->parent == selected;
             ++load_iter) {
          term->add_other_pids(load_iter->check_child_pid);
        }
        freed = std::get<0>(load_iter->footprint);
      }
    }
    round_forbidden.clear();
  }
  // if all busy, no kill wait pending queries
  term->set_immediate(round == 0);
  if (selected) {
    selected->write_mess_to_child_stdin(child_script_terminate);
  }
  return freed;
}

void policy::_remove_oldest_check_child() {
  auto& older_index = _check_child_stats.get<2>();
  ConnectorMess child_script_terminate;
  auto term = child_script_terminate.mutable_terminate();
  if (!older_index.empty()) {
    term->add_other_pids(older_index.begin()->check_child_pid);
    older_index.begin()->parent->write_mess_to_child_stdin(
        child_script_terminate);
  }
}
