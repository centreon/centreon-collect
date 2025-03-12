/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/tcp/acceptor.hh"

#include <fmt/format.h>

#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/tcp/stream.hh"
#include "com/centreon/broker/tcp/tcp_async.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tcp;
using log_v2 = com::centreon::common::log_v2::log_v2;

static constexpr multiplexing::muxer_filter _tcp_stream_filter =
    multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init());

static constexpr multiplexing::muxer_filter _tcp_forbidden_filter =
    multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())
        .add_category(io::local);

/**
 * @brief Acceptor constructor. It needs the port used to listen and a read
 * timeout duration given in seconds that can be -1 if no timeout is wanted.
 *
 * @param port A port.
 * @param read_timeout A duration in seconds.
 */
acceptor::acceptor(const tcp_config::pointer& conf)
    : io::endpoint(true, _tcp_stream_filter, _tcp_forbidden_filter),
      _conf(conf),
      _logger(log_v2::instance().get(log_v2::TCP)) {}

/**
 *  Destructor.
 */
acceptor::~acceptor() noexcept {
  _logger->trace("acceptor destroyed");
  if (_acceptor) {
    tcp_async::instance().stop_acceptor(_acceptor);
  }
}

/**
 *  Add a child to this acceptor.
 *
 *  @param[in] child  Child name.
 */
void acceptor::add_child(std::string const& child) {
  std::lock_guard<std::mutex> lock(_childrenm);
  _children.insert(child);
}

/**
 *  Start connection acception.
 *
 */
std::shared_ptr<io::stream> acceptor::open() {
  if (!_acceptor) {
    _acceptor = tcp_async::instance().create_acceptor(_conf);
    tcp_async::instance().start_acceptor(_acceptor, _conf);
  }

  /* Timeout in seconds during get_connection */
  const uint32_t timeout_s = 3;
  auto conn = tcp_async::instance().get_connection(_acceptor, timeout_s);
  if (conn) {
    assert(conn->port());
    _logger->info("acceptor gets a new connection from {}", conn->peer());
    add_child(conn->peer());
    return std::make_shared<stream>(conn, _conf, _logger);
  }
  return nullptr;
}

bool acceptor::is_ready() const {
  return tcp_async::instance().contains_available_acceptor_connections(
      _acceptor.get());
}

/**
 *  Remove child of this socket.
 *
 *  @param[in] child  Child to remove.
 */
void acceptor::remove_child(std::string const& child) {
  std::lock_guard<std::mutex> lock(_childrenm);
  _children.erase(child);
}

/**
 *  Get statistics about this TCP acceptor.
 *
 *  @param[out] tree Buffer in which statistics will be written.
 */
void acceptor::stats(nlohmann::json& tree) {
  std::lock_guard<std::mutex> children_lock(_childrenm);
  tree["peers"] =
      fmt::format("{}: {}", _children.size(), fmt::join(_children, ", "));
}
