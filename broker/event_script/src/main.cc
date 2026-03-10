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

#include "com/centreon/broker/event_script/factory.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

// Load count.
static uint32_t instances = 0;

#pragma GCC visibility push(default)
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
    // Deregister storage layer.
    io::protocols::instance().unreg("event_script");
  }
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg) {
  (void)arg;

  auto logger = log_v2::instance().get(log_v2::EVENT_SCRIPT);
  // Increment instance number.
  if (!instances++) {
    // Storage module.
    logger->info("event_script: module for Centreon Broker {}",
                 CENTREON_BROKER_VERSION);
    // Register storage layer.
    io::protocols::instance().reg(
        "event_script", std::make_shared<event_script::factory>(), 1, 7);
  }
}
}
#pragma GCC visibility pop
