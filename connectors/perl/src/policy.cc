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
#include <absl/strings/ascii.h>

#include "com/centreon/common/process/process_args.hh"
#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/connector/perl/script_child.hh"
#include "src/perl_connector.pb.h"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;

/**
 *  Default constructor.
 */
policy::policy(const shared_io_context& io_context,
               const std::string& additional_loader_code)
    : _reporter(reporter::create(io_context)),
      _io_context(io_context),
      _additional_loader_code(additional_loader_code) {}

void policy::create(const shared_io_context& io_context,
                    const std::string& additional_loader_code,
                    const std::string& test_cmd_file) {
  std::shared_ptr<policy> ret(new policy(io_context, additional_loader_code));
  ret->_start(test_cmd_file);
}

void policy::_start(const std::string& test_cmd_file) {
  orders::parser::create(_io_context, shared_from_this(), test_cmd_file);
}

/**
 *  Called if stdin is closed.
 */
void policy::on_eof() {
  log::core()->info("stdin is closed");
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
    log::core()->info("error occurred while parsing stdin");
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
void policy::on_execute(
    uint64_t cmd_id,
    const time_point& timeout,
    const std::shared_ptr<com::centreon::connector::orders::options>& opt) {
  // first extract executable
  std::string script_path = *opt;
  size_t first_space = script_path.find(' ');
  if (first_space != std::string_view::npos) {
    script_path = script_path.substr(0, first_space);
  }
  absl::StripAsciiWhitespace(&script_path);

  auto script = _scripts.find(script_path);
  if (script == _scripts.end()) {
    script =
        _scripts
            .emplace(
                script_path,
                std::make_shared<script_child>(
                    _io_context, log::core(), script_path,
                    [weak_me = weak_from_this()](const std::string& script_path,
                                                 const ConnectorMess& mess) {
                      auto me = weak_me.lock();
                      if (me) {
                        std::static_pointer_cast<policy>(me)
                            ->_from_script_child(script_path, mess);
                      }
                    },
                    [weak_me =
                         weak_from_this()](const std::string& script_path) {
                      auto me = weak_me.lock();
                      if (me) {
                        std::static_pointer_cast<policy>(me)->_scripts.erase(
                            script_path);
                      }
                    },
                    _additional_loader_code))
            .first;
    script->second->do_fork(false);
  }

  ConnectorMess order;
  auto execute = order.mutable_execute();
  execute->set_cmd_id(cmd_id);
  com::centreon::common::process_args cmd_line(*opt);
  for (const auto& arg : cmd_line.get_args()) {
    execute->add_args(arg);
  }
  script->second->write_mess_to_child_stdin(order);
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  log::core()->info("quit request received");
}

/**
 *  Version request was received.
 */
void policy::on_version() {
  // Report version 1.0.
  log::core()->info(
      "monitoring engine requested protocol version, sending 1.0");
  _reporter->send_version(1, 0);
}

void policy::_from_script_child(const std::string& script_path,
                                const ConnectorMess& from_script_mess) {}