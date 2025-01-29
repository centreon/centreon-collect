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

#include <grpc/support/log.h>

#include "log.hh"

std::shared_ptr<spdlog::logger> com::centreon::agent::g_logger;

using namespace com::centreon::agent;

/**
 * @brief this function is passed to grpc in order to log grpc layer's events to
 * spdlog
 *
 * @param args grpc logging params
 */
template <spdlog::level::level_enum grpc_level>
static void grpc_logger(gpr_log_func_args* args) {
  if (!args->message)
    return;

  const char* start = args->message;
  if constexpr (grpc_level > spdlog::level::debug) {
    start = strstr(args->message, "}: ");
    if (!start)
      start = args->message;
    else {
      start += 3;
    }
  }
  switch (args->severity) {
    case GPR_LOG_SEVERITY_DEBUG:
      if constexpr (grpc_level == spdlog::level::trace ||
                    grpc_level == spdlog::level::debug) {
        SPDLOG_LOGGER_DEBUG(g_logger, "({}:{}) {}", args->file, args->line,
                            start);
      }
      break;
    case GPR_LOG_SEVERITY_INFO:
      if constexpr (grpc_level == spdlog::level::trace ||
                    grpc_level == spdlog::level::debug ||
                    grpc_level == spdlog::level::info) {
        SPDLOG_LOGGER_INFO(g_logger, "{}", start);
      }
      break;
    case GPR_LOG_SEVERITY_ERROR:
      SPDLOG_LOGGER_ERROR(g_logger, "{}", start);
      break;
  }
}

/**
 * @brief Set the grpc logger hook
 *
 */
void com::centreon::agent::set_grpc_logger() {
  switch (g_logger->level()) {
    case spdlog::level::trace:
      gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);
      gpr_set_log_function(grpc_logger<spdlog::level::trace>);
      break;
    case spdlog::level::debug:
      gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);
      gpr_set_log_function(grpc_logger<spdlog::level::debug>);
      break;
    case spdlog::level::info:
      gpr_set_log_verbosity(GPR_LOG_SEVERITY_INFO);
      gpr_set_log_function(grpc_logger<spdlog::level::info>);
      break;
    case spdlog::level::warn:
    case spdlog::level::err:
    case spdlog::level::critical:
      gpr_set_log_verbosity(GPR_LOG_SEVERITY_ERROR);
      gpr_set_log_function(grpc_logger<spdlog::level::err>);
      break;
    default:
      break;
  }
}
