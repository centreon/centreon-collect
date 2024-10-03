/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include <spdlog/sinks/stdout_color_sinks.h>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "http_client.hh"
#include "http_connection.hh"
#include "http_server.hh"
#include "https_connection.hh"

using namespace com::centreon::common::http;
namespace beast = boost::beast;

extern std::shared_ptr<asio::io_context> g_io_context;

constexpr unsigned port = 5796;

const asio::ip::tcp::endpoint test_endpoint(asio::ip::make_address("127.0.0.1"),
                                            port);

const asio::ip::tcp::endpoint listen_endpoint(asio::ip::make_address("0.0.0.0"),
                                              port);

static void load_server_certificate(boost::asio::ssl::context& ctx,
                                    const http_config::pointer&) {
  /*
      The certificate was generated from bash on Ubuntu (OpenSSL 1.1.1f) using:

      openssl dhparam -out dh.pem 2048
      openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 10000 -out
     cert.pem -subj "/C=US/ST=CA/L=Los Angeles/O=Beast/CN=www.example.com"
  */

  std::string const cert =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIDlTCCAn2gAwIBAgIUOLxr3q7Wd/pto1+2MsW4fdRheCIwDQYJKoZIhvcNAQEL\n"
      "BQAwWjELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMRQwEgYDVQQHDAtMb3MgQW5n\n"
      "ZWxlczEOMAwGA1UECgwFQmVhc3QxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAe\n"
      "Fw0yMTA3MDYwMTQ5MjVaFw00ODExMjEwMTQ5MjVaMFoxCzAJBgNVBAYTAlVTMQsw\n"
      "CQYDVQQIDAJDQTEUMBIGA1UEBwwLTG9zIEFuZ2VsZXMxDjAMBgNVBAoMBUJlYXN0\n"
      "MRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IB\n"
      "DwAwggEKAoIBAQCz0GwgnxSBhygxBdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOT\n"
      "Lppr5MKxqQHEpYdyDYGD1noBoz4TiIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05\n"
      "P41c2F9pBEnUwUxIUG1Cb6AN0cZWF/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATm\n"
      "Wm9SJc2lsjWptbyIN6hFXLYPXTwnYzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7\n"
      "rV+AiGTxUU35V0AzpJlmvct5aJV/5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4\n"
      "lIzJ8yxzOzSStBPxvdrOobSSNlRZIlE7gnyNAgMBAAGjUzBRMB0GA1UdDgQWBBR+\n"
      "dYtY9zmFSw9GYpEXC1iJKHC0/jAfBgNVHSMEGDAWgBR+dYtY9zmFSw9GYpEXC1iJ\n"
      "KHC0/jAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBzKrsiYywl\n"
      "RKeB2LbddgSf7ahiQMXCZpAjZeJikIoEmx+AmjQk1bam+M7WfpRAMnCKooU+Utp5\n"
      "TwtijjnJydkZHFR6UH6oCWm8RsUVxruao/B0UFRlD8q+ZxGd4fGTdLg/ztmA+9oC\n"
      "EmrcQNdz/KIxJj/fRB3j9GM4lkdaIju47V998Z619E/6pt7GWcAySm1faPB0X4fL\n"
      "FJ6iYR2r/kJLoppPqL0EE49uwyYQ1dKhXS2hk+IIfA9mBn8eAFb/0435A2fXutds\n"
      "qhvwIOmAObCzcoKkz3sChbk4ToUTqbC0TmFAXI5Upz1wnADzjpbJrpegCA3pmvhT\n"
      "7356drqnCGY9\n"
      "-----END CERTIFICATE-----\n";

  std::string const key =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCz0GwgnxSBhygx\n"
      "BdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOTLppr5MKxqQHEpYdyDYGD1noBoz4T\n"
      "iIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05P41c2F9pBEnUwUxIUG1Cb6AN0cZW\n"
      "F/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATmWm9SJc2lsjWptbyIN6hFXLYPXTwn\n"
      "YzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7rV+AiGTxUU35V0AzpJlmvct5aJV/\n"
      "5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4lIzJ8yxzOzSStBPxvdrOobSSNlRZ\n"
      "IlE7gnyNAgMBAAECggEAY0RorQmldGx9D7M+XYOPjsWLs1px0cXFwGA20kCgVEp1\n"
      "kleBeHt93JqJsTKwOzN2tswl9/ZrnIPWPUpcbBlB40ggjzQk5k4jBY50Nk2jsxuV\n"
      "9A9qzrP7AoqhAYTQjZe42SMtbkPZhEeOyvCqxBAi6csLhcv4eB4+In0kQo7dfvLs\n"
      "Xu/3WhSsuAWqdD9EGnhD3n+hVTtgiasRe9318/3R9DzP+IokoQGOtXm+1dsfP0mV\n"
      "8XGzQHBpUtJNn0yi6SC4kGEQuKkX33zORlSnZgT5VBLofNgra0THd7x3atOx1lbr\n"
      "V0QizvCdBa6j6FwhOQwW8UwgOCnUbWXl/Xn4OaofMQKBgQDdRXSMyys7qUMe4SYM\n"
      "Mdawj+rjv0Hg98/xORuXKEISh2snJGKEwV7L0vCn468n+sM19z62Axz+lvOUH8Qr\n"
      "hLkBNqJvtIP+b0ljRjem78K4a4qIqUlpejpRLw6a/+44L76pMJXrYg3zdBfwzfwu\n"
      "b9NXdwHzWoNuj4v36teGP6xOUwKBgQDQCT52XX96NseNC6HeK5BgWYYjjxmhksHi\n"
      "stjzPJKySWXZqJpHfXI8qpOd0Sd1FHB+q1s3hand9c+Rxs762OXlqA9Q4i+4qEYZ\n"
      "qhyRkTsl+2BhgzxmoqGd5gsVT7KV8XqtuHWLmetNEi+7+mGSFf2iNFnonKlvT1JX\n"
      "4OQZC7ntnwKBgH/ORFmmaFxXkfteFLnqd5UYK5ZMvGKTALrWP4d5q2BEc7HyJC2F\n"
      "+5lDR9nRezRedS7QlppPBgpPanXeO1LfoHSA+CYJYEwwP3Vl83Mq/Y/EHgp9rXeN\n"
      "L+4AfjEtLo2pljjnZVDGHETIg6OFdunjkXDtvmSvnUbZBwG11bMnSAEdAoGBAKFw\n"
      "qwJb6FNFM3JnNoQctnuuvYPWxwM1yjRMqkOIHCczAlD4oFEeLoqZrNhpuP8Ij4wd\n"
      "GjpqBbpzyVLNP043B6FC3C/edz4Lh+resjDczVPaUZ8aosLbLiREoxE0udfWf2dU\n"
      "oBNnrMwwcs6jrRga7Kr1iVgUSwBQRAxiP2CYUv7tAoGBAKdPdekPNP/rCnHkKIkj\n"
      "o13pr+LJ8t+15vVzZNHwPHUWiYXFhG8Ivx7rqLQSPGcuPhNss3bg1RJiZAUvF6fd\n"
      "e6QS4EZM9dhhlO2FmPQCJMrRVDXaV+9TcJZXCbclQnzzBus9pwZZyw4Anxo0vmir\n"
      "nOMOU6XI4lO9Xge/QDEN4Y2R\n"
      "-----END PRIVATE KEY-----\n";

  std::string const dh =
      "-----BEGIN DH PARAMETERS-----\n"
      "MIIBCAKCAQEArzQc5mpm0Fs8yahDeySj31JZlwEphUdZ9StM2D8+Fo7TMduGtSi+\n"
      "/HRWVwHcTFAgrxVdm+dl474mOUqqaz4MpzIb6+6OVfWHbQJmXPepZKyu4LgUPvY/\n"
      "4q3/iDMjIS0fLOu/bLuObwU5ccZmDgfhmz1GanRlTQOiYRty3FiOATWZBRh6uv4u\n"
      "tff4A9Bm3V9tLx9S6djq31w31Gl7OQhryodW28kc16t9TvO1BzcV3HjRPwpe701X\n"
      "oEEZdnZWANkkpR/m/pfgdmGPU66S2sXMHgsliViQWpDCYeehrvFRHEdR9NV+XJfC\n"
      "QMUk26jPTIVTLfXmmwU0u8vUkpR7LQKkwwIBAg==\n"
      "-----END DH PARAMETERS-----\n";

  ctx.set_password_callback(
      [](std::size_t, boost::asio::ssl::context_base::password_purpose) {
        return "test";
      });

  ctx.set_options(boost::asio::ssl::context::default_workarounds |
                  boost::asio::ssl::context::no_sslv2 |
                  boost::asio::ssl::context::single_dh_use);

  ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

  ctx.use_private_key(boost::asio::buffer(key.data(), key.size()),
                      boost::asio::ssl::context::file_format::pem);

  ctx.use_tmp_dh(boost::asio::buffer(dh.data(), dh.size()));
}

static std::shared_ptr<spdlog::logger> logger =
    spdlog::stdout_color_mt("http_server_test");

static std::atomic_uint session_cpt(0);

template <class connection_class>
class session_test : public connection_class {
  std::atomic_uint _request_cpt;

  void wait_for_request();

  void answer_to_request(const std::shared_ptr<request_type>& request);

 public:
  session_test(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf,
               const ssl_ctx_initializer& ssl_initializer)
      : connection_class(io_context, logger, conf, ssl_initializer) {
    session_cpt.fetch_add(1);
  }

  ~session_test() { session_cpt.fetch_sub(1); }

  std::shared_ptr<session_test> shared_from_this() {
    return std::static_pointer_cast<session_test>(
        connection_class::shared_from_this());
  }

  void on_accept() override;
};

template <class connection_class>
void session_test<connection_class>::on_accept() {
  connection_class::_on_accept(
      [me = shared_from_this()](const boost::beast::error_code& err,
                                const std::string&) {
        if (!err)
          me->wait_for_request();
      });
}

template <class connection_class>
void session_test<connection_class>::wait_for_request() {
  connection_class::receive_request(
      [me = shared_from_this()](const boost::beast::error_code& err,
                                const std::string& detail [[maybe_unused]],
                                const std::shared_ptr<request_type>& request) {
        if (err) {
          SPDLOG_LOGGER_DEBUG(me->_logger,
                              "fail to receive request from {}: {}", me->_peer,
                              err.what());
          return;
        }
        me->answer_to_request(request);
      });
}

template <class connection_class>
void session_test<connection_class>::answer_to_request(
    const std::shared_ptr<request_type>& request) {
  response_ptr resp(std::make_shared<response_type>());
  resp->version(request->version());
  resp->body() = request->body();
  resp->content_length(resp->body().length());

  connection_class::answer(
      resp, [me = shared_from_this(), resp](const boost::beast::error_code& err,
                                            const std::string& detail) {
        if (err) {
          SPDLOG_LOGGER_ERROR(me->_logger, "fail to answer to client {} {}",
                              err.message(), detail);
          return;
        }
        me->wait_for_request();
      });
}

class http_server_test : public ::testing::TestWithParam<bool> {
 public:
  server::pointer _server;

  static void SetUpTestSuite() { logger->set_level(spdlog::level::debug); };
  void TearDown() override {
    if (_server) {
      _server->shutdown();
      _server.reset();
    }
    // let some time to all connections die
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  http_config::pointer create_conf() {
    if (GetParam()) {
      return std::make_shared<http_config>(
          test_endpoint, "localhost", true, std::chrono::seconds(10),
          std::chrono::seconds(10), std::chrono::seconds(10), 30,
          std::chrono::seconds(10), 5, std::chrono::hours(1), 10);

    } else {
      return std::make_shared<http_config>(test_endpoint, "localhost", false);
    }
  }
  http_config::pointer create_server_conf() {
    if (GetParam()) {
      return std::make_shared<http_config>(
          listen_endpoint, "localhost", true, std::chrono::seconds(10),
          std::chrono::seconds(10), std::chrono::seconds(10), 30,
          std::chrono::seconds(10), 5, std::chrono::hours(1), 10,
          asio::ssl::context_base::sslv23_server);

    } else {
      return std::make_shared<http_config>(
          listen_endpoint, "localhost", false, std::chrono::seconds(10),
          std::chrono::seconds(10), std::chrono::seconds(10), 30,
          std::chrono::seconds(10), 5, std::chrono::hours(1), 10);
    }
  }
};

TEST_P(http_server_test, many_request_by_connection) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer client_conf = create_conf();
  http_config::pointer server_conf = create_server_conf();

  connection_creator server_creator, client_creator;

  if (GetParam()) {  // crypted
    server_creator = [server_conf]() {
      return std::make_shared<session_test<https_connection>>(
          g_io_context, logger, server_conf, load_server_certificate);
    };
    client_creator = [client_conf]() {
      return https_connection::load(g_io_context, logger, client_conf);
    };
  } else {
    server_creator = [server_conf]() {
      return std::make_shared<session_test<http_connection>>(
          g_io_context, logger, server_conf, nullptr);
    };
    client_creator = [client_conf]() {
      return http_connection::load(g_io_context, logger, client_conf);
    };
  }

  _server = server::load(g_io_context, logger, server_conf,
                         std::move(server_creator));

  client::pointer client =
      client::load(g_io_context, logger, client_conf, client_creator);

  std::condition_variable cond;
  std::mutex cond_m;
  std::atomic_uint resp_cpt(0);

  for (unsigned send_cpt = 0; send_cpt < 200; ++send_cpt) {
    request_ptr req =
        std::make_shared<request_base>(beast::http::verb::get, "", "/");
    req->body() = fmt::format("hello server {}", send_cpt);
    req->content_length(req->body().length());

    client->send(
        req, [&cond, req, &resp_cpt](const beast::error_code& err,
                                     const std::string& detail [[maybe_unused]],
                                     const response_ptr& response) mutable {
          ASSERT_FALSE(err);
          ASSERT_EQ(req->body(), response->body());
          if (resp_cpt.fetch_add(1) == 199)
            cond.notify_one();
        });
  }
  std::unique_lock l(cond_m);
  cond.wait(l);
  SPDLOG_LOGGER_INFO(logger, "shutdown client");
  client->shutdown();
}

TEST_P(http_server_test, many_request_and_many_connection) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer client_conf = std::make_shared<http_config>(
      test_endpoint, "localhost", GetParam(), std::chrono::seconds(10),
      std::chrono::seconds(10), std::chrono::seconds(10), 30,
      std::chrono::seconds(10), 5, std::chrono::hours(1), 200);

  http_config::pointer server_conf = create_server_conf();

  connection_creator server_creator, client_creator;

  if (GetParam()) {  // crypted
    server_creator = [server_conf]() {
      return std::make_shared<session_test<https_connection>>(
          g_io_context, logger, server_conf, load_server_certificate);
    };
    client_creator = [client_conf]() {
      return https_connection::load(g_io_context, logger, client_conf);
    };
  } else {
    server_creator = [server_conf]() {
      return std::make_shared<session_test<http_connection>>(
          g_io_context, logger, server_conf, nullptr);
    };
    client_creator = [client_conf]() {
      return http_connection::load(g_io_context, logger, client_conf);
    };
  }

  _server = server::load(g_io_context, logger, server_conf,
                         std::move(server_creator));

  client::pointer client =
      client::load(g_io_context, logger, client_conf, client_creator);

  std::condition_variable cond;
  std::mutex cond_m;

  std::atomic_uint resp_cpt(0);
  for (unsigned send_cpt = 0; send_cpt < 1000; ++send_cpt) {
    request_ptr req =
        std::make_shared<request_base>(beast::http::verb::get, "", "/");
    req->body() = fmt::format("hello server {}", send_cpt);
    req->content_length(req->body().length());

    client->send(
        req, [&cond, req, &resp_cpt](const beast::error_code& err,
                                     const std::string& detail [[maybe_unused]],
                                     const response_ptr& response) mutable {
          ASSERT_FALSE(err);
          ASSERT_EQ(req->body(), response->body());
          if (resp_cpt.fetch_add(1) == 999)
            cond.notify_one();
        });
  }
  std::unique_lock l(cond_m);
  cond.wait(l);

  client->shutdown();
}

TEST_P(http_server_test, only_connect_to_timeout) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer client_conf = std::make_shared<http_config>(
      test_endpoint, "localhost", GetParam(), std::chrono::seconds(10),
      std::chrono::seconds(10), std::chrono::seconds(1000), 30,
      std::chrono::seconds(10), 5, std::chrono::hours(1), 200);

  http_config::pointer server_conf;

  if (GetParam()) {
    server_conf = std::make_shared<http_config>(
        listen_endpoint, "localhost", true, std::chrono::seconds(10),
        std::chrono::seconds(10), std::chrono::seconds(1), 30,
        std::chrono::seconds(10), 5, std::chrono::hours(1), 10,
        asio::ssl::context_base::sslv23_server);

  } else {
    server_conf = std::make_shared<http_config>(
        listen_endpoint, "localhost", false, std::chrono::seconds(10),
        std::chrono::seconds(10), std::chrono::seconds(1), 30,
        std::chrono::seconds(10), 5, std::chrono::hours(1), 10);
  }

  connection_creator server_creator, client_creator;

  if (GetParam()) {  // crypted
    server_creator = [server_conf]() {
      return std::make_shared<session_test<https_connection>>(
          g_io_context, logger, server_conf, load_server_certificate);
    };
    client_creator = [client_conf]() {
      return https_connection::load(g_io_context, logger, client_conf);
    };
  } else {
    server_creator = [server_conf]() {
      return std::make_shared<session_test<http_connection>>(
          g_io_context, logger, server_conf, nullptr);
    };
    client_creator = [client_conf]() {
      return http_connection::load(g_io_context, logger, client_conf);
    };
  }

  _server = server::load(g_io_context, logger, server_conf,
                         std::move(server_creator));

  auto lazy_connection = client_creator();

  std::chrono::system_clock::time_point expected_shutdown =
      std::chrono::system_clock::now() + server_conf->get_receive_timeout();

  std::condition_variable cond;
  std::mutex cond_m;
  lazy_connection->connect(
      [lazy_connection, &cond](const boost::beast::error_code& err,
                               const std::string&) {
        ASSERT_FALSE(err);
        lazy_connection->receive_request(
            [&cond](const boost::beast::error_code& err, const std::string&,
                    const std::shared_ptr<request_type>&) {
              ASSERT_TRUE(err);
              cond.notify_one();
            });
      });

  std::unique_lock l(cond_m);
  cond.wait(l);
  std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
  // let some time to session to destroy
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(session_cpt.load(), 1);
  ASSERT_LE(expected_shutdown, end);
  ASSERT_GT(expected_shutdown + std::chrono::milliseconds(100), end);
}

INSTANTIATE_TEST_SUITE_P(http_server_test,
                         http_server_test,
                         testing::Values(false, true));
