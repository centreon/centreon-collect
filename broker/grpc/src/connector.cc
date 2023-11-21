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

#include "grpc_stream.pb.h"

#include "com/centreon/broker/grpc/connector.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief Constructor of the connector that will connect to the given host at
 * the given port. read_timeout is a duration in seconds or -1 if no limit.
 *
 * @param host The host to connect to.
 * @param port The port used for the connection.
 */
connector::connector(const grpc_config::pointer& conf)
    : io::limit_endpoint(false, {}), _conf(conf) {}

/**
 * @brief open a new connection
 *
 * @return std::unique_ptr<io::stream>
 */
std::shared_ptr<io::stream> connector::open() {
  auto logger = log_v2::instance().get(log_v2::GRPC);
  SPDLOG_LOGGER_INFO(logger, "Connecting to {}", _conf->get_hostport());
  try {
    return limit_endpoint::open();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(
        logger, "Unable to establish the connection to {} (attempt {}): {}",
        _conf->get_hostport(), _is_ready_count, e.what());
    return nullptr;
  }
}

/**
 * @brief create a stream from attributes
 *
 * @return std::unique_ptr<io::stream>
 */
std::shared_ptr<io::stream> connector::create_stream() {
  return std::make_shared<stream>(_conf);
}
