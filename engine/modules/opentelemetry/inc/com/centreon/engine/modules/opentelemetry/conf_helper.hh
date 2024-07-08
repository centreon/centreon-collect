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

#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/commands/forward.hh"

namespace com::centreon::engine::modules::opentelemetry {

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

    ret |= handler(hst->check_command(), hst->get_check_command_line(macros),
                   "", logger);
    clear_volatile_macros_r(macros);
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
      ret |=
          handler(serv->check_command(), serv->get_check_command_line(macros),
                  serv->name(), logger);
      clear_volatile_macros_r(macros);
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
