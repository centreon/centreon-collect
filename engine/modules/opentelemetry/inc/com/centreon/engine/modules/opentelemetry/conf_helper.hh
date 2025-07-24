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
#ifndef CCE_MOD_CONF_HELPER_OPENTELEMETRY_HH
#define CCE_MOD_CONF_HELPER_OPENTELEMETRY_HH

#include <absl/container/flat_hash_map.h>
#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/commands/forward.hh"

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief in this struct we store results of whitelist approvals
 *
 */
struct whitelist_cache {
  using cache = absl::flat_hash_map<std::string, bool>;
  uint whitelist_instance_id;
  cache data;
};

/**
 * @brief extract opentelemetry commands from an host list
 * This function must be called from engine main thread, not grpc ones
 *
 * @tparam command_handler callback called on every opentelemetry command found
 * @param host_name name of the host supervised by the agent or telegraf
 * @param handler
 * @return true at least one opentelemetry command was found
 * @return false
 */
template <class command_handler>
bool get_otel_commands(const std::string& host_name,
                       command_handler&& handler,
                       whitelist_cache& whitelist_cache,
                       const std::shared_ptr<spdlog::logger>& logger) {
  auto use_otl_command = [](const checkable& to_test) -> bool {
    if (to_test.get_check_command_ptr()) {
      if (to_test.get_check_command_ptr()->get_type() ==
          commands::command::e_type::otel)
        return true;
      if (to_test.get_check_command_ptr()->get_type() ==
          commands::command::e_type::forward) {
        return std::static_pointer_cast<commands::forward>(
                   to_test.get_check_command_ptr())
                   ->get_sub_command()
                   ->get_type() == commands::command::e_type::otel;
      }
    }
    return false;
  };

  configuration::whitelist& wchecker = configuration::whitelist::instance();

  auto allowed_by_white_list = [&](const std::string& cmd_line) {
    auto cache_iter = whitelist_cache.data.find(cmd_line);
    if (cache_iter != whitelist_cache.data.end()) {
      return cache_iter->second;
    }
    bool allowed = wchecker.is_allowed_by_cma(cmd_line, host_name);

    whitelist_cache.data.emplace(cmd_line, allowed);
    return allowed;
  };

  if (wchecker.instance_id() != whitelist_cache.whitelist_instance_id) {
    whitelist_cache.whitelist_instance_id = wchecker.instance_id();
    whitelist_cache.data.clear();
  }

  bool ret = false;

  auto hst_iter = host::hosts.find(host_name);
  if (hst_iter == host::hosts.end()) {
    SPDLOG_LOGGER_ERROR(logger, "unknown host:{}", host_name);
    return false;
  }
  std::shared_ptr<host> hst = hst_iter->second;
  std::string cmd_line;
  // host check use otl?
  if (use_otl_command(*hst)) {
    nagios_macros* macros(get_global_macros());
    cmd_line = hst->get_check_command_line(macros);
    clear_volatile_macros_r(macros);

    if (allowed_by_white_list(cmd_line)) {
      ret |= handler(hst->check_command(), cmd_line, "", hst->check_interval(),
                     logger);
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
    if (use_otl_command(*serv)) {
      nagios_macros* macros(get_global_macros());
      cmd_line = serv->get_check_command_line(macros);
      clear_volatile_macros_r(macros);

      if (allowed_by_white_list(cmd_line)) {
        ret |= handler(serv->check_command(), cmd_line, serv->name(),
                       serv->check_interval(), logger);
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
#endif
