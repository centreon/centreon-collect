/**
 * Copyright 2020-2021 Centreon
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
#ifndef CENTREON_BROKER_TCP_INC_COM_CENTREON_BROKER_TCP_TCP_ASYNC_HH_
#define CENTREON_BROKER_TCP_INC_COM_CENTREON_BROKER_TCP_TCP_ASYNC_HH_

#include "com/centreon/broker/tcp/tcp_config.hh"
#include "com/centreon/broker/tcp/tcp_connection.hh"
#include "com/centreon/common/pool.hh"

namespace com::centreon::broker {
namespace tcp {

/**
 * @brief The tcp_async class.
 *
 * This class does several little things in background to establish, shutdown,
 * clear connections.
 *
 * It is a unique instance. Before using it, we have to initialize it, it is not
 * done automatically because it depends on the pool. To initialize it, use
 * tcp_async::load() and to reset it use tcp_async::unload(). Be careful you
 * have to initialize the pool before!
 *
 * TCP acceptors do their job here with essentially two methods:
 * * start_acceptor()
 * * stop_acceptor()
 *
 * When an acceptor gets a connection, this one is stored in a multimap named
 * _acceptor_available_con.
 *
 * Each time, broker gets one of those available connections, they also are
 * removed from this multimap.
 *
 * If a connection stays a too long time in this multimap, it is probably
 * a dead connection (usually, the connector asks for a connection, the acceptor
 * accepts it, but this is done asynchronously. Then the connector starts to
 * negotiate and sometimes, the acceptor does not have time at this instant, so
 * the negotiation fails. The connector throws away the connection but not the
 * acceptor because it does not know about this negotiation attempt).
 *
 * Then each time an acceptor is started, a timer is started, it asynchronously
 * waits for 10s, and then looks if there are not used connections established
 * for more than 4s. In that case, it removes them.
 */
class tcp_async {
  static std::shared_ptr<tcp_async> _instance;
  /* The acceptors open by this tcp_async */
  std::list<std::shared_ptr<asio::ip::tcp::acceptor>> _acceptor;

  /* Connections opened by acceptors not already got by streams */
  absl::btree_multimap<asio::ip::tcp::acceptor*,
                       std::pair<tcp_connection::pointer, time_t>>
      _acceptor_available_con;
  mutable std::mutex _acceptor_available_con_m;
  mutable std::condition_variable _acceptor_available_con_cv;

  std::unique_ptr<asio::steady_timer> _timer;
  std::atomic_bool _clear_available_con_running;

  std::shared_ptr<spdlog::logger> _logger;

  tcp_async();

  void _clear_available_con(boost::system::error_code ec);

  static void _set_sock_opt(asio::ip::tcp::socket& sock,
                            const tcp_config::pointer& conf,
                            const std::shared_ptr<spdlog::logger>& logger);

 public:
  static void load();
  static void unload();
  static tcp_async& instance();
  void stop_timer();

  tcp_async(const tcp_async&) = delete;
  tcp_async& operator=(const tcp_async&) = delete;
  std::shared_ptr<asio::ip::tcp::acceptor> create_acceptor(
      const tcp_config::pointer& conf);
  void start_acceptor(const std::shared_ptr<asio::ip::tcp::acceptor>& acceptor,
                      const tcp_config::pointer& conf);
  void stop_acceptor(std::shared_ptr<asio::ip::tcp::acceptor> acceptor);

  std::shared_ptr<tcp_connection> create_connection(
      const tcp_config::pointer& conf);
  void remove_acceptor(std::shared_ptr<asio::ip::tcp::acceptor> acceptor);
  void handle_accept(std::shared_ptr<asio::ip::tcp::acceptor> acceptor,
                     tcp_connection::pointer new_connection,
                     const boost::system::error_code& error,
                     const tcp_config::pointer& conf);
  tcp_connection::pointer get_connection(
      const std::shared_ptr<asio::ip::tcp::acceptor>& acceptor,
      uint32_t timeout_s);
  bool contains_available_acceptor_connections(
      asio::ip::tcp::acceptor* acceptor) const;
};
}  // namespace tcp

}  // namespace com::centreon::broker
#endif  // CENTREON_BROKER_TCP_INC_COM_CENTREON_BROKER_TCP_TCP_ASYNC_HH_
