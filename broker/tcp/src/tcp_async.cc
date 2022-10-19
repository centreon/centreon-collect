/*
** Copyright 2020-2021 Centreon
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
#include "com/centreon/broker/tcp/tcp_async.hh"

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::tcp;

/**
 * @brief this option set the interval in seconds between two keepalive sent
 *
 */
using tcp_keep_alive_interval =
    asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPINTVL>;

/**
 * @brief this option set the delay after the first keepalive will be sent
 *
 */
using tcp_keep_alive_idle =
    asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPIDLE>;
/**
 * @brief this option set the maximum of not answered keepalive paquets
 *
 */
using tcp_keep_alive_cnt =
    asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPCNT>;
/**
 * This option takes an unsigned int as an argument.  When the value is greater
 * than 0, it specifies the maximum amount of time in milliseconds that
 * transmitted data may remain unacknowledged before TCP will forcibly close the
 * corresponding connection and return ETIMEDOUT to the ap‐ plication.  If the
 * option value is specified as 0, TCP will use the system default.
 *
 */
using tcp_user_timeout =
    asio::detail::socket_option::integer<IPPROTO_TCP, TCP_USER_TIMEOUT>;

tcp_async* tcp_async::_instance{nullptr};

/**
 * @brief Return the tcp_async singleton.
 *
 * @return A tcp_async singleton.
 */
tcp_async& tcp_async::instance() {
  assert(tcp_async::_instance);
  return *_instance;
}

/**
 * @brief Static function to initialize the tcp_sync object. It must be
 * executed before using the tcp_sync object and must be started after the
 * pool initialization.
 */
void tcp_async::load() {
  if (_instance == nullptr)
    _instance = new tcp_async();
  else
    log_v2::tcp()->error("tcp_async instance already started.");
}

/**
 * @brief This is the way to stop the tcp_sync instance. To call before the
 * pool unload since tcp_sync is heavily based on it.
 */
void tcp_async::unload() {
  if (_instance) {
    delete _instance;
    _instance = nullptr;
  }
}

/**
 * @brief Default constructor. Don't use it, it is private. Instead, call the
 * tcp_async::load() function to initialize it and then, use the instance()
 * method.
 */
tcp_async::tcp_async()
    : _strand{pool::instance().io_context()},
      _clear_available_con_running(false) {}

/**
 * @brief Stop the timer that clears available connections.
 */
void tcp_async::stop_timer() {
  log_v2::tcp()->trace("tcp_async::stop_timer");
  if (_clear_available_con_running) {
    std::promise<bool> p;
    std::future<bool> f(p.get_future());
    _clear_available_con_running = false;
    asio::post(_timer->get_executor(), [this, &p] {
      _timer->cancel();
      p.set_value(true);
    });
    f.get();
  }
  if (_timer)
    _timer.reset();
}

/**
 * @brief The destructor of tcp_async. You don't have to use it, instead, use
 * the unload() function.
 */
tcp_async::~tcp_async() noexcept {
  stop_timer();
  /* Before destroying the strand, we have to wait it is really empty. We post
   * a last action and wait it is over. */
  std::promise<bool> p;
  std::future<bool> f{p.get_future()};
  _strand.post([&p] { p.set_value(true); });
  f.get();
}

/**
 * @brief If the acceptor given in parameter has established a connection.
 * This method returns it. Otherwise, it returns an empty connection.
 *
 * @param acceptor The acceptor we want a connection to.
 *
 * @return A shared_ptr to a connection or nothing.
 */
tcp_connection::pointer tcp_async::get_connection(
    const std::shared_ptr<asio::ip::tcp::acceptor>& acceptor,
    uint32_t timeout_s) {
  auto end = std::chrono::system_clock::now() + std::chrono::seconds{timeout_s};
  do {
    std::promise<tcp_connection::pointer> p;
    std::future<tcp_connection::pointer> f{p.get_future()};
    _strand.post([&p, a = acceptor.get(), this] {
      auto found = _acceptor_available_con.find(a);
      if (found != _acceptor_available_con.end()) {
        tcp_connection::pointer retval = std::move(found->second.first);
        _acceptor_available_con.erase(found);
        p.set_value(retval);
      } else
        p.set_value(nullptr);
    });
    auto retval = f.get();
    if (retval)
      return retval;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while (std::chrono::system_clock::now() < end);

  return nullptr;
}

bool tcp_async::contains_available_acceptor_connections(
    asio::ip::tcp::acceptor* acceptor) const {
  std::promise<bool> p;
  std::future<bool> f{p.get_future()};
  _strand.post([&p, &acceptor, this] {
    p.set_value(_acceptor_available_con.find(acceptor) !=
                _acceptor_available_con.end());
  });

  return f.get();
}

/**
 * @brief Create an ASIO acceptor listening on the given port. Once it is
 * operational, it begins to accept connections.
 *
 * @param port The port to listen on.
 *
 * @return The created acceptor as a shared_ptr.
 */
std::shared_ptr<asio::ip::tcp::acceptor> tcp_async::create_acceptor(
    const tcp_config::pointer& conf) {
  asio::ip::tcp::endpoint listen_endpoint;
  if (conf->get_host().empty())
    listen_endpoint =
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), conf->get_port());
  else {
    asio::ip::tcp::resolver::query query(conf->get_host(),
                                         std::to_string(conf->get_port()));
    asio::ip::tcp::resolver resolver(pool::io_context());
    asio::error_code ec;
    asio::ip::tcp::resolver::iterator it = resolver.resolve(query, ec), end;
    if (ec) {
      log_v2::tcp()->error("TCP: error while resolving '{}' name: {}",
                           conf->get_host(), ec.message());
      listen_endpoint =
          asio::ip::tcp::endpoint(asio::ip::tcp::v4(), conf->get_port());
    } else {
      for (; it != end; ++it) {
        listen_endpoint = *it;
        log_v2::tcp()->info("TCP: {} gives address {}", conf->get_host(),
                            listen_endpoint.address().to_string());
        if (listen_endpoint.address().is_v4())
          break;
      }
    }
  }
  auto retval{std::make_shared<asio::ip::tcp::acceptor>(pool::io_context(),
                                                        listen_endpoint)};

  asio::ip::tcp::acceptor::reuse_address option(true);
  retval->set_option(option);
  return retval;
}

/**
 * @brief this function works
 *
 * @param ec
 */
void tcp_async::_clear_available_con(asio::error_code ec) {
  if (ec)
    log_v2::core()->info("Available connections cleaning: {}", ec.message());
  else {
    log_v2::core()->debug("Available connections cleaning");
    std::time_t now = std::time(nullptr);
    _strand.post([now, this] {
      for (auto it = _acceptor_available_con.begin();
           it != _acceptor_available_con.end();) {
        if (now >= it->second.second + 10) {
          log_v2::tcp()->debug("Destroying connection to '{}'",
                               it->second.first->peer());
          it = _acceptor_available_con.erase(it);
        } else
          ++it;
      }
      if (!_acceptor_available_con.empty()) {
        _timer->expires_after(std::chrono::seconds(10));
        _timer->async_wait(std::bind(&tcp_async::_clear_available_con, this,
                                     std::placeholders::_1));
      } else
        _clear_available_con_running = false;
    });
  }
}

/**
 * @brief Starts the acceptor given in parameter. To accept the acceptor needs
 * the IO Context to be running. A timer is started/restarted so that in 10s
 * not used connections will be erased.
 *
 * @param acceptor The acceptor that you want it to accept.
 */
void tcp_async::start_acceptor(
    const std::shared_ptr<asio::ip::tcp::acceptor>& acceptor,
    const tcp_config::pointer& conf) {
  log_v2::tcp()->trace("Start acceptor");
  if (!_timer)
    _timer =
        std::make_unique<asio::steady_timer>(pool::instance().io_context());

  if (!_clear_available_con_running)
    _clear_available_con_running = true;

  log_v2::tcp()->debug("Reschedule available connections cleaning in 10s");
  _timer->expires_after(std::chrono::seconds(10));
  _timer->async_wait(
      std::bind(&tcp_async::_clear_available_con, this, std::placeholders::_1));

  tcp_connection::pointer new_connection =
      std::make_shared<tcp_connection>(pool::io_context());

  log_v2::tcp()->debug("Waiting for a connection");
  acceptor->async_accept(
      new_connection->socket(),
      [this, acceptor, new_connection, conf](const asio::error_code& ec) {
        handle_accept(acceptor, new_connection, ec, conf);
      });
}

/**
 * @brief Stop the acceptor.
 *
 * @param acceptor The acceptor to stop.
 */
void tcp_async::stop_acceptor(
    std::shared_ptr<asio::ip::tcp::acceptor> acceptor) {
  std::error_code ec;
  acceptor->cancel(ec);
  if (ec)
    log_v2::tcp()->warn("Error while cancelling acceptor: {}", ec.message());
  acceptor->close(ec);
  if (ec)
    log_v2::tcp()->warn("Error while closing acceptor: {}", ec.message());
}

/**
 * @brief The handler called after an async_accept.
 *
 * @param acceptor The acceptor accepting a connection.
 * @param new_connection The established connection.
 * @param ec An error code if any.
 */
void tcp_async::handle_accept(std::shared_ptr<asio::ip::tcp::acceptor> acceptor,
                              tcp_connection::pointer new_connection,
                              const asio::error_code& ec,
                              const tcp_config::pointer& conf) {
  /* If we got a connection, we store it */
  if (!ec) {
    asio::error_code ecc;
    new_connection->update_peer(ecc);
    if (ecc)
      log_v2::tcp()->error(
          "tcp acceptor handling connection: unable to get peer endpoint: {}",
          ecc.message());
    else {
      std::time_t now = std::time(nullptr);
      asio::ip::tcp::socket& sock = new_connection->socket();
      try {
        _set_sock_opt(sock, conf);
        _strand.post([new_connection, now, acceptor, this] {
          _acceptor_available_con.insert(std::make_pair(
              acceptor.get(), std::make_pair(new_connection, now)));
        });
      } catch (const std::exception& e) {
        log_v2::tcp()->error(
            "fail to activate keepalive on accepted connection");
      }
      start_acceptor(acceptor, conf);
    }
  } else
    log_v2::tcp()->info("TCP acceptor interrupted: {}", ec.message());
}

/**
 * @brief Creates a connection to the given host on the given port.
 *
 * @param host The host to connect to.
 * @param port The port to use for the connection.
 *
 * @return A shared_ptr to the connection or an empty shared_ptr.
 */
tcp_connection::pointer tcp_async::create_connection(
    const tcp_config::pointer& conf) {
  log_v2::tcp()->trace("create connection to host {}:{}", conf->get_host(),
                       conf->get_port());
  tcp_connection::pointer conn = std::make_shared<tcp_connection>(
      pool::io_context(), conf->get_host(), conf->get_port());
  asio::ip::tcp::socket& sock = conn->socket();

  asio::ip::tcp::resolver resolver(pool::io_context());
  asio::ip::tcp::resolver::query query(conf->get_host(),
                                       std::to_string(conf->get_port()));
  asio::ip::tcp::resolver::iterator it = resolver.resolve(query), end;

  std::error_code err{std::make_error_code(std::errc::host_unreachable)};

  // it can resolve multiple addresses like ipv4 or ipv6
  // We need to try all to find the first available socket
  for (; err && it != end; ++it) {
    sock.connect(*it, err);

    if (err)
      sock.close();
  }

  /* Connection refused */
  if (err.value() == 111) {
    log_v2::tcp()->error("TCP: Connection refused to {}:{}", conf->get_host(),
                         conf->get_port());
    throw std::system_error(err);
  } else if (err) {
    log_v2::tcp()->error("TCP: could not connect to {}:{}", conf->get_host(),
                         conf->get_port());
    throw msg_fmt(err.message());
  } else {
    _set_sock_opt(sock, conf);
    return conn;
  }
}

void tcp_async::_set_sock_opt(asio::ip::tcp::socket& sock,
                              const tcp_config::pointer& conf) {
  asio::socket_base::keep_alive option1(true);
  tcp_keep_alive_cnt option2(conf->get_keepalive_count());
  tcp_keep_alive_idle option3(conf->get_second_keepalive_interval());
  tcp_keep_alive_interval option4(conf->get_second_keepalive_interval());
  tcp_user_timeout option5(1000 * (conf->get_second_keepalive_interval() *
                                   (conf->get_keepalive_count() + 1)));
  asio::error_code err;
  sock.set_option(option1, err);
  if (err) {
    SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail to set keepalive option {}",
                        err.message());
  } else {
    sock.set_option(option2, err);
    if (err) {
      SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail to set keepalive cnt {}",
                          err.message());
    }
    sock.set_option(option3, err);
    if (err) {
      SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail to set keepalive idle {}",
                          err.message());
    }
    sock.set_option(option4, err);
    if (err) {
      SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail to set keepalive interval {}",
                          err.message());
    }
  }
  sock.set_option(option5, err);
  if (err) {
    SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail to set keepalive option");
  }
}