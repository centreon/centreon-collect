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

#include "com/centreon/broker/event_script/connector.hh"
#include "com/centreon/broker/event_script/stream.hh"
#include "events.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::event_script;

static constexpr multiplexing::muxer_filter _event_script_mandatory_filter = {
    multiplexing::muxer_filter::zero_init()};

static constexpr multiplexing::muxer_filter _event_script_forbidden_filter = {
    multiplexing::muxer_filter::zero_init()};

/**
 *  Default constructor.
 */
connector::connector()
    : io::endpoint(false,
                   _event_script_mandatory_filter,
                   _event_script_forbidden_filter) {}

/**
 *  Set connection parameters.
 *
 */
void connector::connect_to(
    const std::string_view& script_path,
    const std::chrono::system_clock::duration& managed_event_ttl,
    const std::chrono::system_clock::duration& timeout) {
  _script_path = script_path;
  _managed_event_ttl = managed_event_ttl;
  _timeout = timeout;
}

/**
 * @brief Create an event_script stream
 *
 * @return An event_script connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  return std::make_shared<stream>(_script_path, _managed_event_ttl, _timeout);
}
