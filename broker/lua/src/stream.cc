/**
 * Copyright 2017-2024 Centreon
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
#include "com/centreon/broker/lua/stream.hh"

#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/lua/luabinding.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::misc;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::lua;

using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] addr                    Address to connect to
 *  @param[in] port                    port
 */
stream::stream(const std::string& lua_script,
               const std::map<std::string, misc::variant>& conf_params,
               const std::shared_ptr<persistent_cache>& cache)
    : io::stream("lua"),
      _luabinding(lua_script, conf_params, _cache),
      _logger{cache->logger()},
      _cache{cache} {}

stream::~stream() noexcept {
  _logger->trace("lua::stream destructor {}", static_cast<void*>(this));
}

/**
 *  Read from the connector.
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method will throw.
 */
bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;
  d.reset();
  throw exceptions::shutdown("cannot read from lua generic connector");
}

/**
 *  Write an event.
 *
 *  @param[in] data Event pointer.
 *
 *  @return Number of events acknowledged.
 */
int stream::write(std::shared_ptr<io::data> const& data) {
  assert(data);

  // Give data to cache.
  _cache.write(data);

  return _luabinding.write(data);
}

/**
 *  Events have been transmitted to the Lua connector, several of them have
 *  been treated and can now be acknowledged by broker. This function returns
 *  how many are in that case.
 *
 * @return The number of events to ack.
 */
int32_t stream::flush() {
  int32_t retval = 0;
  if (_luabinding.has_flush()) {
    retval = _luabinding.flush();
    _logger->debug("stream: flush {} events acknowledged", retval);
  }
  return retval;
}

/**
 * @brief Stops the stream and flushes data.
 *
 * @return The number of acknowledged events.
 */
int32_t stream::stop() {
  _logger->trace("lua::stream stop {}", static_cast<void*>(this));
  return _luabinding.stop();
}
