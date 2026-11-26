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

#include "conf_helper.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros/grab_host.hh"
#include "com/centreon/engine/macros/grab_service.hh"
#include "com/centreon/engine/macros/process.hh"

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief check if a command line is allowed to be executed on this poller
 * results are cached in whitelist_cache until the whitelist instance changes
 *
 * @param cmd_line the command line to test
 * @param host_name name of the host on which the command would be executed
 * @param whitelist_cache cache of previous whitelist approvals
 * @return true the command is allowed by the whitelist
 * @return false the command is rejected by the whitelist
 */
static bool allowed_by_white_list(const std::string& cmd_line,
                                  const std::string& host_name,
                                  whitelist_cache& whitelist_cache) {
  configuration::whitelist& wchecker = configuration::whitelist::instance();

  if (wchecker.instance_id() != whitelist_cache.whitelist_instance_id) {
    whitelist_cache.whitelist_instance_id = wchecker.instance_id();
    whitelist_cache.data.clear();
  }

  auto cache_iter = whitelist_cache.data.find(cmd_line);
  if (cache_iter != whitelist_cache.data.end()) {
    return cache_iter->second;
  }
  bool allowed = wchecker.is_allowed_by_cma(cmd_line, host_name);

  whitelist_cache.data.emplace(cmd_line, allowed);
  return allowed;
};

/**
 * @brief build the event handler command line of a service
 * host and service macros are grabbed, then the raw command line of the
 * event handler command is expanded into cmdline
 * the result is rejected if not allowed by the whitelist
 *
 * @param event_handler_command name of the event handler command
 * @param macros global macros
 * @param serv the service concerned by the event handler
 * @param whitelist_cache cache of previous whitelist approvals
 * @param logger logger
 * @param cmdline output: the expanded event handler command line,
 * empty if the event handler cannot be used
 * @return true the event handler command line was built and allowed
 * @return false no event handler, unknown command or not allowed
 */
static bool _fill_service_event_handler(
    const std::string& event_handler_command,
    nagios_macros* macros,
    service& serv,
    whitelist_cache& whitelist_cache,
    const std::shared_ptr<spdlog::logger>& logger,
    std::string& cmdline) {
  cmdline.clear();
  if (event_handler_command.empty()) {
    return false;
  }
  std::shared_ptr<host> host = serv.get_host_ptr();
  if (host) {
    grab_host_macros_r(macros, host.get());
  }
  grab_service_macros_r(macros, &serv);

  macros->kept_macros.insert(
      {"SERVICESTATE", "SERVICESTATETYPE", "SERVICEATTEMPT"});
  auto cmd = commands::command::commands.find(event_handler_command);
  if (cmd == commands::command::commands.end()) {
    SPDLOG_LOGGER_ERROR(logger, "unknown event handler command {}",
                        event_handler_command);
    return false;
  }
  std::string raw_cmdline;
  get_raw_command_line_r(macros, cmd->second.get(), "", raw_cmdline, 0);
  process_macros_r(macros, raw_cmdline, cmdline,
                   STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
  if (!allowed_by_white_list(cmdline, host ? host->name() : "",
                             whitelist_cache)) {
    SPDLOG_LOGGER_ERROR(
        logger,
        "service {}: this command cannot be executed because of "
        "security restrictions on the poller. A whitelist has "
        "been defined, and it does not include this command.",
        serv.name());
    SPDLOG_LOGGER_DEBUG(
        logger, "service {}: event handler {} not allowed by whitelist",
        serv.name(), event_handler_command);
    cmdline.clear();
    return false;
  }
  return true;
}

/**
 * @brief build the event handler command line of an host
 * host macros are grabbed, then the raw command line of the event handler
 * command is expanded into cmdline
 * the result is rejected if not allowed by the whitelist
 *
 * @param event_handler_command name of the event handler command
 * @param macros global macros
 * @param host the host concerned by the event handler
 * @param whitelist_cache cache of previous whitelist approvals
 * @param logger logger
 * @param cmdline output: the expanded event handler command line,
 * empty if the event handler cannot be used
 * @return true the event handler command line was built and allowed
 * @return false no event handler, unknown command or not allowed
 */
static bool _fill_host_event_handler(
    const std::string& event_handler_command,
    nagios_macros* macros,
    host& host,
    whitelist_cache& whitelist_cache,
    const std::shared_ptr<spdlog::logger>& logger,
    std::string& cmdline) {
  cmdline.clear();
  if (event_handler_command.empty()) {
    return false;
  }
  grab_host_macros_r(macros, &host);
  macros->kept_macros.insert({"HOSTSTATE", "HOSTSTATETYPE", "HOSTATTEMPT"});
  auto cmd = commands::command::commands.find(event_handler_command);
  if (cmd == commands::command::commands.end()) {
    SPDLOG_LOGGER_ERROR(logger, "unknown event handler command {}",
                        event_handler_command);
    return false;
  }
  std::string raw_cmdline;
  get_raw_command_line_r(macros, cmd->second.get(), "", raw_cmdline, 0);
  process_macros_r(macros, raw_cmdline, cmdline,
                   STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
  if (!allowed_by_white_list(cmdline, host.name(), whitelist_cache)) {
    SPDLOG_LOGGER_ERROR(logger,
                        "host {}: this command cannot be executed because of "
                        "security restrictions on the poller. A whitelist has "
                        "been defined, and it does not include this command.",
                        host.name());
    SPDLOG_LOGGER_DEBUG(logger,
                        "host {}: event handler {} not allowed by whitelist",
                        host.name(), event_handler_command);
    cmdline.clear();
    return false;
  }
  return true;
}

/**
 * @brief extract opentelemetry commands from an host list
 * This function must be called from engine main thread, not grpc ones
 *
 * @param host_name name of the host supervised by the agent or telegraf
 * @param handler callback called on every opentelemetry command found
 * @param whitelist_cache cache of previous whitelist approvals
 * @param logger logger
 * @return success at least one opentelemetry command was found
 * @return unknown_host the host name was not found in the host list
 * @return no_cma_service no opentelemetry command was found
 */
e_get_otel_commands_ret get_otel_commands(
    const std::string& host_name,
    command_cb&& handler,
    whitelist_cache& whitelist_cache,
    const std::shared_ptr<spdlog::logger>& logger) {
  auto is_otl_command =
      [](const std::shared_ptr<commands::command>& to_test) -> bool {
    if (to_test) {
      if (to_test->get_type() == commands::command::e_type::otel)
        return true;
      if (to_test->get_type() == commands::command::e_type::forward) {
        return std::static_pointer_cast<commands::forward>(to_test)
                   ->get_sub_command()
                   ->get_type() == commands::command::e_type::otel;
      }
    }
    return false;
  };

  e_get_otel_commands_ret ret = e_get_otel_commands_ret::no_cma_service;

  auto hst_iter = host::hosts.find(host_name);
  if (hst_iter == host::hosts.end()) {
    SPDLOG_LOGGER_ERROR(
        logger,
        "Agent with host name '{}' not found in host list; unable to extract "
        "OpenTelemetry commands for this agent",
        host_name);
    return e_get_otel_commands_ret::unknown_host;
  }
  std::shared_ptr<host> hst = hst_iter->second;
  std::string cmd_line;
  std::string global_event_handler;
  std::string event_handler;
  nagios_macros* macros(get_global_macros());

  // host check use otl?
  if (is_otl_command(hst->get_check_command_ptr())) {
    cmd_line = hst->get_check_command_line(macros);

    if (pb_config.enable_event_handlers() &&
        !pb_config.global_host_event_handler().empty()) {
      _fill_host_event_handler(pb_config.global_host_event_handler(), macros,
                               *hst, whitelist_cache, logger,
                               global_event_handler);
    }

    if (hst->event_handler_enabled() && !hst->event_handler().empty()) {
      _fill_host_event_handler(hst->event_handler(), macros, *hst,
                               whitelist_cache, logger, event_handler);
    }
    clear_volatile_macros_r(macros);

    if (allowed_by_white_list(cmd_line, hst->name(), whitelist_cache)) {
      if (handler(hst->check_command(), cmd_line, "", hst->host_id(), 0,
                  hst->check_interval(), hst->retry_interval(),
                  hst->max_check_attempts(), hst->check_period(),
                  hst->get_timezone(), global_event_handler, event_handler,
                  logger)) {
        ret = e_get_otel_commands_ret::success;
      }
    } else {
      SPDLOG_LOGGER_ERROR(
          logger,
          "host {}: this command cannot be executed because of "
          "security restrictions on the poller. A whitelist has "
          "been defined, and it does not include this command.",
          host_name);
      SPDLOG_LOGGER_DEBUG(logger,
                          "host {}: command not allowed by whitelist {}",
                          host_name, cmd_line);
    }
  } else {
    SPDLOG_LOGGER_DEBUG(
        logger, "host {} doesn't use opentelemetry to do his check", host_name);
  }
  // services of host
  auto serv_iter = service::services_by_id.lower_bound({hst->host_id(), 0});
  for (; serv_iter != service::services_by_id.end() &&
         serv_iter->first.first == hst->host_id();
       ++serv_iter) {
    std::shared_ptr<service> serv = serv_iter->second;
    if (is_otl_command(serv->get_check_command_ptr())) {
      cmd_line = serv->get_check_command_line(macros);

      if (pb_config.enable_event_handlers() &&
          !pb_config.global_service_event_handler().empty()) {
        _fill_service_event_handler(pb_config.global_service_event_handler(),
                                    macros, *serv, whitelist_cache, logger,
                                    global_event_handler);
      } else {
        global_event_handler.clear();
      }

      if (serv->event_handler_enabled() && !serv->event_handler().empty()) {
        _fill_service_event_handler(serv->event_handler(), macros, *serv,
                                    whitelist_cache, logger, event_handler);
      } else {
        event_handler.clear();
      }

      clear_volatile_macros_r(macros);

      if (allowed_by_white_list(cmd_line, hst->name(), whitelist_cache)) {
        if (handler(serv->check_command(), cmd_line, serv->name(),
                    serv->host_id(), serv->service_id(), serv->check_interval(),
                    serv->retry_interval(), serv->max_check_attempts(),
                    serv->check_period(), serv->get_timezone(),
                    global_event_handler, event_handler, logger)) {
          ret = e_get_otel_commands_ret::success;
        }
      } else {
        SPDLOG_LOGGER_ERROR(
            logger,
            "service {}: this command cannot be executed because of "
            "security restrictions on the poller. A whitelist has "
            "been defined, and it does not include this command.",
            serv->name());

        SPDLOG_LOGGER_DEBUG(logger,
                            "service {}: command not allowed by whitelist {}",
                            serv->name(), cmd_line);
      }
    } else {
      SPDLOG_LOGGER_DEBUG(
          logger,
          "host {} service {} doesn't use opentelemetry to do his check",
          host_name, serv->name());
    }
  }
  return ret;
}
}  // namespace com::centreon::engine::modules::opentelemetry