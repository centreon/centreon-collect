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
#include <absl/functional/function_ref.h>
#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/defines.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/commands/forward.hh"
#include "com/centreon/engine/utils.hh"

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief in this struct we store results of whitelist approvals
 *
 */
struct whitelist_cache {
  using cache = absl::flat_hash_map<std::string, bool>;
  uint whitelist_instance_id = 0;
  cache data;
};

enum class e_get_otel_commands_ret { success, unknown_host, no_cma_service };

using command_cb =
    absl::FunctionRef<bool(const std::string& /* command name */,
                           const std::string& /* command line */,
                           const std::string& /* service name */,
                           uint64_t /* host id */,
                           uint64_t /* service id */,
                           uint32_t /* check interval*/,
                           uint32_t /* retry interval */,
                           uint32_t /* max checks attempts */,
                           const std::string& /* check period */,
                           const std::string& /* tz */,
                           const std::string& /*global event handler */,
                           const std::string& /* event handler */,
                           const std::shared_ptr<spdlog::logger>&)>;

e_get_otel_commands_ret get_otel_commands(
    const std::string& host_name,
    command_cb&& handler,
    whitelist_cache& whitelist_cache,
    const std::shared_ptr<spdlog::logger>& logger);

}  // namespace com::centreon::engine::modules::opentelemetry
#endif
