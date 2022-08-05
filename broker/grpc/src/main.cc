/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/broker/grpc/factory.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;

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
  if (!--instances)
    // Unregister TCP protocol.
    io::protocols::instance().unreg("GRPC");
  return false;  // don't unload library because grpc layers could run in other
                 // threads
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg) {
  (void)arg;

  // Increment instance number.
  if (!instances++) {
    // TCP module.
    SPDLOG_LOGGER_INFO(log_v2::grpc(), "Module for Centreon Broker {}",
                       CENTREON_BROKER_VERSION);

    // Register TCP protocol.
    io::protocols::instance().reg(
        "GRPC", std::make_shared<com::centreon::broker::grpc::factory>(), 1, 4);
  }
}
}
