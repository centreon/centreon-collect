/**
* Copyright 2017 Centreon
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

#include "com/centreon/broker/generator/dummy.hh"
#include "com/centreon/broker/generator/factory.hh"
#include "com/centreon/broker/generator/internal.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

// Load count.
namespace {
uint32_t instances(0);
char const* generator_module("generator");
}  // namespace

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
    // Deregister storage layer.
    io::protocols::instance().unreg(generator_module);
    // Deregister generator events.
    io::events::instance().unregister_category(io::generator);
  }
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg  Configuration object.
 */
void broker_module_init(void const* arg) {
  (void)arg;

  // Increment instance number.
  if (!instances++) {
    // generator module.
    log_v2::core()->info("generator: module for Centreon Broker {}",
                         CENTREON_BROKER_VERSION);

    // Register storage layer.
    io::protocols::instance().reg(generator_module,
                                  std::make_shared<generator::factory>(), 1, 7);

    io::events& e(io::events::instance());

    // Register bam events.
    e.register_event(io::generator, generator::de_dummy,
                     io::event_info("dummy", &generator::dummy::operations,
                                    generator::dummy::entries));
  }
}
}
