/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <rrd.h>

#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/rrd/factory.hh"

using namespace com::centreon::broker;

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
void broker_module_deinit() {
  // Decrement instance number.
  if (!--instances)
    // Deregister RRD layer.
    io::protocols::instance().unreg("RRD");
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
    // RRD module.
    logging::info(logging::high)
        << "RRD: module for Centreon Broker " << CENTREON_BROKER_VERSION;

    // Print RRDtool version.
    char const* rrdversion(rrd_strversion());
    logging::info(logging::high)
        << "RRD: using rrdtool " << (rrdversion ? rrdversion : "(unknown)");

    // Register RRD layer.
    io::protocols::instance().reg("RRD", std::make_shared<rrd::factory>(), 1,
                                  7);
  }
}
}
