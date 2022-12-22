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
 *
 */
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/namespace.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/http_client/http_connection.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_client;

static std::shared_ptr<asio::io_context> io_context(
    std::make_shared<asio::io_context>());

static asio::executor_work_guard<asio::io_context::executor_type> worker(
    asio::make_work_guard(*io_context));
static std::thread io_context_t;

constexpr unsigned port = 5796;

const asio::ip::tcp::endpoint test_endpoint(asio::ip::make_address("127.0.0.1"),
                                            port);

/*********************************************************************
 * http keepalive test
 *********************************************************************/
class dummy_connection : public connection_base {
 public:
  void set_state(int state) { _state = state; }

  dummy_connection(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const http_config::pointer& conf)
      : connection_base(io_context, logger, conf) {}

  void shutdown() override { _state = e_not_connected; }

  void connect(connect_callback_type&& callback) override {}

  void send(request_ptr request, send_callback_type&& callback) override {}
};

TEST(http_keepalive_test, ConnectionClose) {
  dummy_connection conn(io_context, log_v2::tcp(),
                        std::make_shared<http_config>(test_endpoint));
  response_ptr resp(std::make_shared<response_type>());
  resp->keep_alive(false);
  conn.set_state(connection_base::e_receive);
  conn.gest_keepalive(resp);
  ASSERT_EQ(conn.get_state(), connection_base::e_not_connected);
}

TEST(http_keepalive_test, KeepAliveWithoutTimeout) {
  auto conf = std::make_shared<http_config>(test_endpoint);
  dummy_connection conn(io_context, log_v2::tcp(), conf);
  response_ptr resp(std::make_shared<response_type>());
  resp->keep_alive(true);
  conn.set_state(connection_base::e_idle);
  conn.gest_keepalive(resp);
  time_point keepalive_end_expected =
      system_clock::now() + conf->get_default_http_keepalive_duration();
  ASSERT_EQ(conn.get_state(), connection_base::e_idle);
  ASSERT_LE(conn.get_keep_alive_end(),
            keepalive_end_expected + std::chrono::milliseconds(10));
  ASSERT_LE(keepalive_end_expected,
            conn.get_keep_alive_end() + std::chrono::milliseconds(10));
}

TEST(http_keepalive_test, KeepAliveWithTimeout) {
  auto conf = std::make_shared<http_config>(test_endpoint);
  dummy_connection conn(io_context, log_v2::tcp(), conf);
  response_ptr resp(std::make_shared<response_type>());
  resp->keep_alive(true);
  resp->set(boost::beast::http::field::keep_alive, "timeout=5, max=1000");
  conn.set_state(connection_base::e_idle);
  conn.gest_keepalive(resp);
  time_point keepalive_end_expected =
      system_clock::now() + std::chrono::seconds(5);
  ASSERT_EQ(conn.get_state(), connection_base::e_idle);
  ASSERT_LE(conn.get_keep_alive_end(),
            keepalive_end_expected + std::chrono::milliseconds(10));
  ASSERT_LE(keepalive_end_expected,
            conn.get_keep_alive_end() + std::chrono::milliseconds(10));
}

/*********************************************************************
 * connection test
 *********************************************************************/

class session_base : public std::enable_shared_from_this<session_base> {
 public:
  using pointer = std::shared_ptr<session_base>;
  virtual ~session_base() { SPDLOG_LOGGER_TRACE(log_v2::tcp(), "end session"); }

  virtual void on_receive(const boost::beast::error_code& err,
                          const request_ptr& request) = 0;

  virtual void send_response(const response_ptr&) = 0;
  virtual void shutdown() = 0;
  virtual void start() = 0;
};

using creator_fct = std::function<session_base::pointer(
    asio::ip::tcp::socket&&,
    const std::shared_ptr<boost::asio::ssl::context>& /*null if no crypted*/)>;

static creator_fct creator;

class http_session : public session_base {
  boost::beast::tcp_stream _stream;
  boost::beast::flat_buffer _buffer;

  void start_recv() {
    request_ptr request(std::make_shared<request_type>());
    boost::beast::http::async_read(
        _stream, _buffer, *request,
        [me = shared_from_this(), request](const boost::beast::error_code& err,
                                           std::size_t bytes_received) {
          if (err) {
            SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail recv {}", err.message());
            me->shutdown();
          } else {
            SPDLOG_LOGGER_TRACE(log_v2::tcp(), "recv {} bytes", bytes_received);
            me->on_receive(err, request);
            me->start_recv();
          }
        });
  }

 public:
  http_session(boost::asio::ip::tcp::socket&& socket,
               const std::shared_ptr<boost::asio::ssl::context>&)
      : _stream(std::move(socket)) {}

  std::shared_ptr<http_session> shared_from_this() {
    return std::static_pointer_cast<http_session>(
        session_base::shared_from_this());
  }

  void start() override {
    SPDLOG_LOGGER_TRACE(log_v2::tcp(), "start a session");
    start_recv();
  }

  void send_response(const response_ptr& resp) override {
    SPDLOG_LOGGER_TRACE(log_v2::tcp(), "send response");
    boost::beast::http::async_write(
        _stream, *resp,
        [me = shared_from_this(), resp](const boost::beast::error_code& err,
                                        std::size_t bytes_sent) {
          if (err) {
            SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail send response");
            me->shutdown();
          }
        });
  }

  void shutdown() override {
    _stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both);
    _stream.close();
  }
};

class https_session : public session_base {};

class listener : public std::enable_shared_from_this<listener> {
 protected:
  std::shared_ptr<boost::asio::ssl::context> _ssl_context;
  boost::asio::ip::tcp::acceptor _acceptor;

  void do_accept() {
    // The new connection gets its own strand
    _acceptor.async_accept(boost::asio::make_strand(*io_context),
                           boost::beast::bind_front_handler(
                               &listener::on_accept, shared_from_this()));
  }

  void on_accept(const boost::beast::error_code& ec,
                 boost::asio::ip::tcp::socket socket) {
    if (ec) {
      SPDLOG_LOGGER_ERROR(log_v2::tcp(), "fail accept");
      return;
    }
    SPDLOG_LOGGER_DEBUG(log_v2::tcp(), "accept a connection");
    auto session = creator(std::move(socket), _ssl_context);
    session->start();
    do_accept();
  }

 public:
  using pointer = std::shared_ptr<listener>;

  listener(unsigned port)
      : _ssl_context(std::make_shared<boost::asio::ssl::context>(
            boost::asio::ssl::context::tlsv12)),
        _acceptor(*io_context, test_endpoint, true) {}

  void start() { do_accept(); }

  void shutdown() { _acceptor.close(); }
};

/**
 * @brief the template parameter indicate if test is played on https versus http
 *
 */
class http_test : public ::testing::TestWithParam<bool> {
 protected:
  static listener::pointer _listener;

 public:
  static void SetUpTestSuite() {
    std::thread t([]() {
      do {
        io_context->run();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (!io_context->stopped());
    });
    io_context_t.swap(t);
    log_v2::tcp()->set_level(spdlog::level::trace);
    _listener = std::make_shared<listener>(port);
    _listener->start();
  };
  static void TearDownTestSuite() {
    _listener->shutdown();
    _listener.reset();
    worker.reset();
    io_context->stop();
    io_context_t.join();
  }
  void SetUp() override {}

  void TearDown() override {}
};

listener::pointer http_test::_listener;

// simple exchange with no keepalive
template <class base_class>
class answer_no_keep_alive : public base_class {
 public:
  answer_no_keep_alive(
      boost::asio::ip::tcp::socket&& socket,
      const std::shared_ptr<boost::asio::ssl::context>& ssl_context)
      : base_class(std::move(socket), ssl_context) {}

  void on_receive(const boost::beast::error_code& err,
                  const request_ptr& request) {
    if (!err) {
      ASSERT_EQ(request->body(), "hello server");
      response_ptr resp(std::make_shared<response_type>(
          boost::beast::http::status::ok, request->version()));
      resp->keep_alive(false);
      resp->body() = "hello client";
      resp->content_length(resp->body().length());
      base_class::send_response(resp);
    }
  }
};

TEST_P(http_test, connect_send_answer_without_keepalive) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer conf(
      std::make_shared<http_config>(test_endpoint, GetParam()));

  if (GetParam()) {  // crypted
  } else {
    creator = [](asio::ip::tcp::socket&& socket,
                 const std::shared_ptr<boost::asio::ssl::context>& ctx)
        -> session_base::pointer {
      return std::make_shared<answer_no_keep_alive<http_session>>(
          std::move(socket), ctx);
    };
  }

  auto client = http_connection::load(io_context, log_v2::tcp(), conf);
  request_ptr request(std::make_shared<request_type>());
  request->method(boost::beast::http::verb::put);
  request->target("/");
  request->body() = "hello server";
  request->content_length(request->body().length());
  std::promise<std::tuple<boost::beast::error_code, std::string, response_ptr>>
      p;
  auto f(p.get_future());
  time_point send_begin = system_clock::now();

  client->connect([&p, client, request](const boost::beast::error_code& err,
                                        const std::string& detail) {
    if (err) {
      p.set_value(std::make_tuple(err, detail, response_ptr()));
    } else {
      client->send(request, [&p](const boost::beast::error_code& err,
                                 const std::string& detail,
                                 const response_ptr& response) mutable {
        p.set_value(std::make_tuple(err, detail, response));
      });
    }
  });

  auto completion = f.get();
  time_point send_end = system_clock::now();
  ASSERT_LT((send_end - send_begin), std::chrono::milliseconds(100));
  ASSERT_FALSE(std::get<0>(completion));
  ASSERT_TRUE(std::get<1>(completion).empty());
  ASSERT_EQ(std::get<2>(completion)->body(), "hello client");
  ASSERT_EQ(std::get<2>(completion)->keep_alive(), false);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(client->get_state(), connection_base::e_not_connected);
}

// simple exchange with  keepalive
template <class base_class>
class answer_keep_alive : public base_class {
  unsigned _counter;

 public:
  answer_keep_alive(
      boost::asio::ip::tcp::socket&& socket,
      const std::shared_ptr<boost::asio::ssl::context>& ssl_context)
      : base_class(std::move(socket), ssl_context), _counter(0) {}

  void on_receive(const boost::beast::error_code& err,
                  const request_ptr& request) {
    if (!err) {
      ASSERT_EQ(request->body(), "hello server");
      response_ptr resp(std::make_shared<response_type>(
          boost::beast::http::status::ok, request->version()));
      resp->keep_alive(true);
      resp->body() = fmt::format("hello client {}", _counter++);
      resp->content_length(resp->body().length());
      base_class::send_response(resp);
    }
  }
};

TEST_P(http_test, connect_send_answer_with_keepalive) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer conf(
      std::make_shared<http_config>(test_endpoint, GetParam()));

  if (GetParam()) {  // crypted
  } else {
    creator = [](asio::ip::tcp::socket&& socket,
                 const std::shared_ptr<boost::asio::ssl::context>& ctx)
        -> session_base::pointer {
      return std::make_shared<answer_keep_alive<http_session>>(
          std::move(socket), ctx);
    };
  }

  auto client = http_connection::load(io_context, log_v2::tcp(), conf);
  request_ptr request(std::make_shared<request_type>());
  request->method(boost::beast::http::verb::put);
  request->target("/");
  request->body() = "hello server";
  request->content_length(request->body().length());
  std::promise<std::tuple<boost::beast::error_code, std::string, response_ptr>>
      p;
  auto f(p.get_future());
  time_point send_begin = system_clock::now();

  client->connect([&p, client, request](const boost::beast::error_code& err,
                                        const std::string& detail) {
    if (err) {
      p.set_value(std::make_tuple(err, detail, response_ptr()));
    } else {
      client->send(request, [&p](const boost::beast::error_code& err,
                                 const std::string& detail,
                                 const response_ptr& response) mutable {
        p.set_value(std::make_tuple(err, detail, response));
      });
    }
  });

  auto completion = f.get();
  time_point send_end = system_clock::now();
  ASSERT_LT((send_end - send_begin), std::chrono::milliseconds(100));
  ASSERT_FALSE(std::get<0>(completion));
  ASSERT_TRUE(std::get<1>(completion).empty());
  ASSERT_EQ(std::get<2>(completion)->body(), "hello client 0");
  ASSERT_EQ(std::get<2>(completion)->keep_alive(), true);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(client->get_state(), connection_base::e_idle);

  std::promise<std::tuple<boost::beast::error_code, std::string, response_ptr>>
      p2;
  auto f2(p2.get_future());
  client->send(request, [&p2](const boost::beast::error_code& err,
                              const std::string& detail,
                              const response_ptr& response) mutable {
    p2.set_value(std::make_tuple(err, detail, response));
  });

  auto completion2 = f2.get();
  ASSERT_FALSE(std::get<0>(completion2));
  ASSERT_TRUE(std::get<1>(completion2).empty());
  ASSERT_EQ(std::get<2>(completion2)->body(), "hello client 1");
  ASSERT_EQ(std::get<2>(completion2)->keep_alive(), true);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(client->get_state(), connection_base::e_idle);
}

INSTANTIATE_TEST_SUITE_P(http_connection, http_test, testing::Values(false));