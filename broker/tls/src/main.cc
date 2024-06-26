/**
 * Copyright 2013 Centreon
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

#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/tls/factory.hh"
#include "com/centreon/broker/tls/internal.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

// Load count.
static uint32_t instances(0);

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
char const* broker_module_version = CENTREON_BROKER_VERSION;

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    // Unregister TLS layer.
    io::protocols::instance().unreg("TLS");

    // Cleanup.
    tls::destroy();
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

  auto logger = log_v2::instance().get(log_v2::TLS);
  // Increment instance number.
  if (!instances++) {
    // TLS module.
    logger->info("TLS: module for Centreon Broker {}", CENTREON_BROKER_VERSION);

    // Initialization.
    tls::initialize();

    // Register TLS layer.
    io::protocols::instance().reg("TLS", std::make_shared<tls::factory>(), 5,
                                  5);
  }
}
}
