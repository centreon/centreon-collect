/**
* Copyright 2022 Centreon
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
#include "com/centreon/broker/io/limit_endpoint.hh"

using namespace com::centreon::broker::io;

/**
 * @brief Connect to the remote host.
 *
 * @return The connection object.
 */
std::shared_ptr<stream> limit_endpoint::open() {
  // Launch connection process.
  try {
    std::shared_ptr<stream> retval = create_stream();
    _is_ready_count = 0;
    return retval;
  } catch (const std::exception& e) {
    if (_is_ready_count < 30)
      _is_ready_count++;
    return nullptr;
  }
}

/**
 * @brief Return true when it is time to attempt a new connection. The idea is
 * to increase the duration between two calls each time this function is called
 * without connection between. So if now server is available, we should not
 * try to connect too often, but if the connection failed one time, it should
 * be fast to connect again.
 *
 * @return a boolean.
 */
bool limit_endpoint::is_ready() const {
  time_t now;
  std::time(&now);
  if (now - _is_ready_now > (1 << _is_ready_count)) {
    _is_ready_now = now;
    return true;
  }
  return false;
}
