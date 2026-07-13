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

#include "check_exec.hh"
#include "agent.grpc.pb.h"
#include "com/centreon/common/process/process.hh"
#include "config.hh"

using com::centreon::exceptions::msg_fmt;
using namespace com::centreon::agent;

/******************************************************************
 * check_exec
 ******************************************************************/

check_exec::check_exec(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    const Service& serv,
    const std::string& command_line,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat,
    const std::shared_ptr<common::crypto::aes256>& credentials_decrypt)
    : check(io_context,
            logger,
            first_start_expected,
            serv,
            cnf,
            std::move(handler),
            stat),
      _credentials_decrypt(credentials_decrypt) {
  _process_args =
      com::centreon::common::process<false>::parse_cmd_line(command_line);
  if (_credentials_decrypt) {
    _process_args->encrypt_args(*_credentials_decrypt);
    _process_args->clear_unencrypted_args();
  }
}

/**
 * @brief create and initialize a check_exec object (don't use
 * constructor)
 *
 * @tparam handler_type
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param serv
 * @param cnf   agent configuration
 * @param handler  completion handler
 * @return std::shared_ptr<check_exec>
 */
std::shared_ptr<check_exec> check_exec::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    const Service& serv,
    const std::string& command_line,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat,
    const std::shared_ptr<common::crypto::aes256>& credentials_decrypt) {
  std::shared_ptr<check_exec> ret = std::make_shared<check_exec>(
      io_context, logger, first_start_expected, serv, command_line, cnf,
      std::move(handler), stat, credentials_decrypt);
  return ret;
}

/**
 * @brief start a check, completion handler is always called asynchronously even
 * in case of failure
 *
 * @param timeout
 */
void check_exec::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }
  const duration effective_timeout = get_custom_timeout().value_or(timeout);

  try {
    auto proc = std::make_shared<com::centreon::common::process<false>>(
        _io_context, _logger, _process_args, true, false, nullptr);

    if (_credentials_decrypt) {
      _process_args->decrypt_args(*_credentials_decrypt);
    }
    // we add 100ms to time out in order to let check class manage timeout
    proc->start_process(
        [me = std::static_pointer_cast<check_exec>(shared_from_this()),
         running_index = _get_running_check_index()](
            const com::centreon::common::process<false>&, int exit_code,
            int exit_status, const std::string& std_out, const std::string&) {
          me->on_completion(running_index, exit_code, exit_status, std_out);
        },
        effective_timeout + std::chrono::milliseconds(100));
    _pid = proc->get_pid();
    if (_credentials_decrypt) {
      _process_args->clear_unencrypted_args();
    }
  } catch (const boost::system::system_error& e) {
    if (_credentials_decrypt) {
      _process_args->clear_unencrypted_args();
    }
    SPDLOG_LOGGER_ERROR(_logger, " serv {} fail to execute {}: {}",
                        get_service(), get_command_line(), e.code().message());
    asio::post(*_io_context,
               [me = check::shared_from_this(),
                start_check_index = _get_running_check_index(), e]() {
                 me->on_completion(
                     start_check_index, e_status::unknown,
                     std::list<com::centreon::common::perfdata>(),
                     {fmt::format("Fail to execute {} : {}",
                                  me->get_command_line(), e.code().message())});
               });
  } catch (const std::exception& e) {
    std::string output =
        fmt::format("Fail to execute {} : {}", get_command_line(), e.what());
    SPDLOG_LOGGER_ERROR(_logger, " serv {} {}", get_service(), output);
    asio::post(*_io_context, [me = check::shared_from_this(),
                              start_check_index = _get_running_check_index(),
                              output]() {
      me->on_completion(start_check_index, e_status::unknown,
                        std::list<com::centreon::common::perfdata>(), {output});
    });
  }
}

/**
 * @brief called on process completion
 *
 * @param running_index
 */
void check_exec::on_completion(unsigned running_index,
                               int exit_code,
                               int exit_status [[maybe_unused]],
                               const std::string& std_out) {
  if (running_index != _get_running_check_index()) {
    SPDLOG_LOGGER_ERROR(_logger, "running_index={}, running_index={}",
                        running_index, _get_running_check_index());
    return;
  }

  std::list<std::string> outputs;
  std::list<com::centreon::common::perfdata> perfs;

  std::string short_output;
  std::string long_output;
  std::string perf_data;
  // parse the output check
  common::parse_check_output(std_out, short_output, long_output, perf_data,
                             false, true);
  // prepare the output without perfdata
  outputs.push_front(short_output +
                     (long_output.empty() ? "" : "\n" + long_output));

  // parse perfdata
  if (!perf_data.empty()) {
    boost::trim(perf_data);
    perfs = com::centreon::common::perfdata::parse_perfdata(
        0, 0, perf_data.c_str(), _logger);
  }

  check::on_completion(running_index, exit_code, perfs, outputs);
}

/******************************************************************
 * check_dummy
 ******************************************************************/

check_dummy::check_dummy(const std::shared_ptr<asio::io_context>& io_context,
                         const std::shared_ptr<spdlog::logger>& logger,
                         time_point first_start_expected,
                         const Service& serv,
                         const std::string& output,
                         const engine_to_agent_request_ptr& cnf,
                         check::completion_handler&& handler,
                         const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            serv,
            cnf,
            std::move(handler),
            stat),
      _output(output) {}

/**
 * @brief create and initialize a check_dummy object (don't use constructor)
 *
 * @tparam handler_type
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param serv
 * @param cnf   agent configuration
 * @param handler  completion handler
 * @return std::shared_ptr<check_dummy>
 */
std::shared_ptr<check_dummy> check_dummy::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    const Service& serv,
    const std::string& output,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat) {
  std::shared_ptr<check_dummy> ret = std::make_shared<check_dummy>(
      io_context, logger, first_start_expected, serv, output, cnf,
      std::move(handler), stat);
  return ret;
}

/**
 * @brief start a check, completion handler is always called asynchronously even
 * in case of failure
 *
 * @param timeout
 */
void check_dummy::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  asio::post(*_io_context, [me = check::shared_from_this(),
                            start_check_index = _get_running_check_index(),
                            this]() {
    me->on_completion(
        start_check_index, e_status::critical,
        std::list<com::centreon::common::perfdata>(),
        {fmt::format("unable to execute native check {} , output error : {}",
                     me->get_command_line(), get_output())});
  });
}

/******************************************************************
 * check_custom
 ******************************************************************/
static std::string _build_custom_command_line(const Service& serv,
                                              const rapidjson::Value& args) {
  // args expected schema: {"name": "name_cmd", "ARG1": "args_cmd","ARG2":
  // "args_cmd"}
  std::string name;
  std::string cmd_args;
  if (!args.IsObject()) {
    throw msg_fmt("custom check arguments must be a JSON object for service {}",
                  serv.service_description());
  }
  if (args.HasMember("name") && args["name"].IsString()) {
    name = args["name"].GetString();
  } else {
    throw msg_fmt("custom check missing 'name' field for service {}",
                  serv.service_description());
  }

  // extract argument from json
  unsigned num_args = args.MemberCount() - 1;  // exclude "name"
  std::vector<std::string> arguments;
  arguments.reserve(num_args);

  // Collect ARGn values (ensure vector is sized correctly)
  arguments.reserve(num_args);
  for (unsigned idx = 0; idx < num_args; ++idx) {
    const std::string arg_key = fmt::format("ARG{}", idx + 1);
    if (args.HasMember(arg_key.c_str()) && args[arg_key.c_str()].IsString()) {
      arguments.push_back(args[arg_key.c_str()].GetString());
    } else {
      arguments.push_back("");
    }
  }

  const auto& custom_map = config::instance().get_custom_checks();
  if (custom_map.empty()) {
    throw msg_fmt("no custom checks defined in agent configuration");
  }

  auto it = custom_map.find(name);
  if (it == custom_map.end()) {
    throw msg_fmt("unknown custom check '{}' called by service '{}'", name,
                  serv.service_description());
  }

  // reconstruct the path with args
  std::string cmd_line = it->second;

  if (num_args > 0) {
    // replace $ARGn$ with {n} in the commandline
    for (unsigned idx = 0; idx < num_args; ++idx) {
      const std::string arg_key = fmt::format("$ARG{}$", idx + 1);
      size_t pos = cmd_line.find(arg_key);
      if (pos != std::string::npos) {
        cmd_line.replace(pos, arg_key.size(), arguments[idx]);
      }
    }
  }
  return cmd_line;
}

check_custom::check_custom(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    const Service& serv,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat,
    const std::shared_ptr<common::crypto::aes256>& credentials_decrypt)
    : check_exec(io_context,
                 logger,
                 first_start_expected,
                 serv,
                 _build_custom_command_line(serv, args),
                 cnf,
                 std::move(handler),
                 stat,
                 credentials_decrypt) {}
