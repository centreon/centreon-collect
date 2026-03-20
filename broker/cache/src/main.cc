/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "bbdo/neb.pb.h"

#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::cache;
using log_v2 = com::centreon::common::log_v2::log_v2;

// Load count.
static uint32_t instances(0);

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
char const* broker_module_version = CENTREON_BROKER_VERSION;

/**
 * @brief Return an array with modules needed for this one to work.
 *
 * @return An array of const char*
 */
const char* const* broker_module_parents() {
  constexpr static const char* retval[]{"10-neb.so", nullptr};
  return retval;
}

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    global_cache::unload();
  }
  return true;  // ok to be unloaded
}
/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration argument.
 */
void broker_module_init(void const*) {
  auto logger = log_v2::instance().get(log_v2::CORE);
  // Increment instance number.
  if (!instances++) {
    // Stats module.
    logger->info("cache: module for Centreon Broker {}",
                 CENTREON_BROKER_VERSION);

    global_cache::load(
        com::centreon::common::pool::io_context_ptr(),
        fmt::format("{}.cache.global",
                    com::centreon::broker::config::applier::state::instance()
                        .cache_dir()));
  }
}
}
