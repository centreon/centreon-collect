/**
 * Copyright 2011-2012,2015 Centreon
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

#include "com/centreon/broker/simu/connector.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/simu/stream.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::simu;
using com::centreon::common::log_v2::log_v2;

/**
 *  Default constructor.
 */
connector::connector()
    : io::endpoint(
          false,
          multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init()),
          multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())
              .add_category(io::local)) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
connector::connector(connector const& other)
    : io::endpoint(other),
      _lua_script(other._lua_script),
      _conf_params(other._conf_params) {}

/**
 *  Destructor.
 */
connector::~connector() {}

/**
 *  Set connection parameters.
 *
 *  @param[in] lua_script              The Lua script to load
 *  @param[in] cfg_params              A hash table containing the user
 *                                     parameters
 */
void connector::connect_to(
    std::string const& lua_script,
    std::map<std::string, misc::variant> const& cfg_params) {
  _conf_params = cfg_params;
  _lua_script = lua_script;
}

/**
 *  Connect to the lua connector.
 *
 *  @return a lua connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  return std::make_shared<stream>(_lua_script, _conf_params,
                                  log_v2::instance().get(log_v2::LUA));
}
