/**
 * Copyright 2011 - 2021 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/tcp/connector.hh"

#include <fmt/format.h>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/tcp/stream.hh"
#include "com/centreon/broker/tcp/tcp_async.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tcp;

/**
 * @brief Constructor of the connector that will connect to the given host at
 * the given port. read_timeout is a duration in seconds or -1 if no limit.
 *
 * @param host The host to connect to.
 * @param port The port used for the connection.
 * @param read_timeout The read timeout in seconds or -1 if no duration.
 */
connector::connector(const tcp_config::pointer& conf)
    : io::limit_endpoint(false, {}), _conf(conf) {}

/**
 *  Destructor.
 */
connector::~connector() {}

/**
 * @brief Connect to the remote host.
 *
 * @return The TCP connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  // Launch connection process.
  log_v2::tcp()->info("TCP: connecting to {}:{}", _conf->get_host(),
                      _conf->get_port());
  try {
    return limit_endpoint::open();
  } catch (const std::exception& e) {
    log_v2::tcp()->debug(
        "Unable to establish the connection to {}:{} (attempt {}): {}",
        _conf->get_host(), _conf->get_port(), _is_ready_count, e.what());
    return nullptr;
  }
}

/**
 * @brief create a stream from attributes
 *
 * @return std::shared_ptr<stream>
 */
std::shared_ptr<io::stream> connector::create_stream() {
  return std::make_shared<stream>(_conf);
}
