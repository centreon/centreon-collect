/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
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

//#include "com/centreon/broker/config/state.hh"
//#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/namespace.hh"

// using namespace com::centreon::broker;

// Load count.
static uint32_t instances = 0;

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
const char* broker_module_version = CENTREON_BROKER_VERSION;

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration argument.
 */
void broker_module_init(const void* arg) {
  // Increment instance number.
  if (!instances++) {
    // Stats module.
    //    log_v2::config()->info("stats: module for Centreon Broker {}",
    //                        CENTREON_BROKER_VERSION);
  }
}

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
  }
  return true;  // ok to be unloaded
}
}
