/**
 * Copyright 2013, 2021 Centreon
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

#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
#include "bbdo/bbdo/version_response.hh"
#include "broker/core/bbdo/factory.hh"
#include "com/centreon/broker/io/protocols.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bbdo;

/**
 *  @brief BBDO initialization routine.
 *
 *  Initialize BBDO mappings and register BBDO protocol.
 */
void bbdo::load() {
  // Register BBDO category.
  io::events& e(io::events::instance());

  // Register BBDO events. Be careful, on Broker, these events are initialized
  // in core/src/io/events, method events::events().
  e.register_event(make_type(io::bbdo, bbdo::de_version_response),
                   "version_response", &version_response::operations,
                   version_response::entries);
  e.register_event(make_type(io::bbdo, bbdo::de_welcome), "welcome",
                   &bbdo::pb_welcome::operations);
  e.register_event(make_type(io::bbdo, bbdo::de_ack), "ack", &ack::operations,
                   ack::entries);
  e.register_event(make_type(io::bbdo, bbdo::de_stop), "stop",
                   &stop::operations, stop::entries);
  e.register_event(make_type(io::bbdo, bbdo::de_pb_ack), "Ack",
                   &bbdo::pb_ack::operations);
  e.register_event(make_type(io::bbdo, bbdo::de_pb_stop), "Stop",
                   &bbdo::pb_stop::operations);
  e.register_event(make_type(io::local, local::de_pb_stop), "LocStop",
                   &local::pb_stop::operations);
  e.register_event(make_type(io::bbdo, bbdo::de_pb_diff_state), "DiffState",
                   &bbdo::pb_diff_state::operations);

  // Register BBDO protocol.
  io::protocols::instance().reg("BBDO", std::make_shared<bbdo::factory>(), 7,
                                7);
}

/**
 *  @brief BBDO cleanup routine.
 *
 *  Delete BBDO mappings and unregister BBDO protocol.
 */
void bbdo::unload() {
  // Unregister protocol.
  io::protocols::instance().unreg("BBDO");

  // Unregister category
  io::events::instance().unregister_category(io::bbdo);
}
