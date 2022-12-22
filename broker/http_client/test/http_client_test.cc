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
#include <boost/container/flat_set.hpp>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/namespace.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/http_client/http_client.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_client;

static std::shared_ptr<asio::io_context> io_context(
    std::make_shared<asio::io_context>());
static asio::executor_work_guard<asio::io_context::executor_type> worker(
    asio::make_work_guard(*io_context));
static std::thread io_context_t;

const asio::ip::tcp::endpoint test_endpoint(asio::ip::make_address("127.0.0.1"),
                                            1234);

class http_client_test : public testing::Test {
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
  };
  static void TearDownTestSuite() {
    worker.reset();
    io_context->stop();
    io_context_t.join();
  }
};

class connection_ok : public connection_base {
  unsigned _connect_counter;
  unsigned _request_counter;

 public:
  connection_ok(const std::shared_ptr<asio::io_context>& io_context,
                const std::shared_ptr<spdlog::logger>& logger,
                const http_config::pointer& conf)
      : connection_base(io_context, logger, conf),
        _connect_counter(0),
        _request_counter(0) {}

  unsigned get_connect_counter() const { return _connect_counter; }
  unsigned get_request_counter() const { return _request_counter; }

  void shutdown() override { _state = e_not_connected; }

  void connect(connect_callback_type&& callback) override {
    _state = e_idle;
    ++_connect_counter;
    _io_context->post([cb = std::move(callback)]() { cb({}, {}); });
  }

  void send(request_ptr request, send_callback_type&& callback) override {
    if (_state != e_idle) {
      _io_context->post([cb = std::move(callback)]() {
        cb(std::make_error_code(std::errc::invalid_argument), "bad state", {});
      });
    } else {
      _keep_alive_end = system_clock::now() + std::chrono::hours(1);
      _io_context->post([cb = std::move(callback)]() {
        auto resp = std::make_shared<response_type>();
        resp->keep_alive(true);

        cb({}, "", resp);
      });
    }
    ++_request_counter;
  }
};

TEST_F(http_client_test, many_request_use_all_connection) {
  std::vector<std::shared_ptr<connection_ok>> conns;
  client::pointer clt = client::load(
      io_context, log_v2::tcp(), std::make_shared<http_config>(test_endpoint),
      [&conns](const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf) {
        auto dummy_conn =
            std::make_shared<connection_ok>(io_context, logger, conf);
        conns.push_back(dummy_conn);
        return dummy_conn;
      });

  request_ptr request(std::make_shared<request_type>());

  std::promise<void> p;
  std::future<void> f = p.get_future();
  for (unsigned sent_cpt = 0; sent_cpt < 10; ++sent_cpt) {
    clt->send(request, [sent_cpt, &p](const boost::beast::error_code&,
                                      const std::string&, const response_ptr&) {
      if (sent_cpt == 9) {
        p.set_value();
      }
    });
  }
  f.get();

  unsigned nb_connected =
      std::count_if(conns.begin(), conns.end(),
                    [](const std::shared_ptr<connection_ok>& to_test) {
                      return to_test->get_connect_counter() == 1;
                    });

  auto max_request = std::max_element(
      conns.begin(), conns.end(),
      [](const std::shared_ptr<connection_ok>& left,
         const std::shared_ptr<connection_ok>& right) {
        return left->get_request_counter() < right->get_request_counter();
      });
  ASSERT_EQ(nb_connected, 10);
  ASSERT_EQ((*max_request)->get_request_counter(), 1);
}

TEST_F(http_client_test, recycle_connection) {
  std::vector<std::shared_ptr<connection_ok>> conns;
  client::pointer clt = client::load(
      io_context, log_v2::tcp(), std::make_shared<http_config>(test_endpoint),
      [&conns](const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf) {
        auto dummy_conn =
            std::make_shared<connection_ok>(io_context, logger, conf);
        conns.push_back(dummy_conn);
        return dummy_conn;
      });

  struct sender {
    request_ptr request = std::make_shared<request_type>();
    unsigned sent_cpt = 0;
    std::promise<void> p;
    void send(client::pointer clt) {
      clt->send(request, [&, clt](const boost::beast::error_code&,
                                  const std::string&, const response_ptr&) {
        if (++sent_cpt == 10) {
          p.set_value();
        } else {
          send(clt);
        }
      });
    }
  };

  sender runner;
  std::future<void> f = runner.p.get_future();
  runner.send(clt);

  f.get();

  unsigned nb_connected =
      std::count_if(conns.begin(), conns.end(),
                    [](const std::shared_ptr<connection_ok>& to_test) {
                      return to_test->get_connect_counter() == 1;
                    });

  auto max_request = std::max_element(
      conns.begin(), conns.end(),
      [](const std::shared_ptr<connection_ok>& left,
         const std::shared_ptr<connection_ok>& right) {
        return left->get_request_counter() < right->get_request_counter();
      });
  ASSERT_EQ(nb_connected, 1);
  ASSERT_EQ((*max_request)->get_request_counter(), 10);
}