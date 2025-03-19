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

#include <absl/log/log_sink_registry.h>
#include <grpc/support/log.h>
#include <grpcpp/version_info.h>
#include "spdlog/common.h"

#include "log.hh"

std::shared_ptr<spdlog::logger> com::centreon::agent::g_logger;

using namespace com::centreon::agent;

/**
 * @brief after this version of grpc, logs are handled by absl
 *
 */
#if GRPC_CPP_VERSION_MAJOR > 1 || GRPC_CPP_VERSION_MINOR >= 66

/**
 * @brief abseil log sink used by grpc to log grpc layer's events to spdlog
 *
 */
class grpc_log_sink : public absl::LogSink {
 public:
  void Send(const absl::LogEntry& entry) override;
};

/**
 * @brief the method that do the job
 *
 * @param entry
 */
void grpc_log_sink::Send(const absl::LogEntry& entry) {
  auto logger = g_logger;
  if (!logger) {
    return;
  }
  if (!entry.text_message().empty()) {
    switch (entry.log_severity()) {
      case absl::LogSeverity::kInfo:
        logger->log(spdlog::source_loc{entry.source_basename().data(),
                                       entry.source_line(), ""},
                    spdlog::level::info, entry.text_message());
        break;
      case absl::LogSeverity::kWarning:
        logger->log(spdlog::source_loc(entry.source_basename().data(),
                                       entry.source_line(), ""),
                    spdlog::level::warn, entry.text_message());
        break;
      case absl::LogSeverity::kError:
        logger->log(spdlog::source_loc(entry.source_basename().data(),
                                       entry.source_line(), ""),
                    spdlog::level::err, entry.text_message());
        break;
      case absl::LogSeverity::kFatal:
        logger->log(spdlog::source_loc(entry.source_basename().data(),
                                       entry.source_line(), ""),
                    spdlog::level::critical, entry.text_message());
        break;
      default:
        break;
    }
  }
}

static grpc_log_sink* _grpc_log_sink = new grpc_log_sink();

void com::centreon::agent::set_grpc_logger() {
  absl::AddLogSink(_grpc_log_sink);
}

#else

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

#endif
