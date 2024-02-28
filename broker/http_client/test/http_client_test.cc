/**
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
#include <com/centreon/common/defer.hh>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/common/defer.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/http_client/http_client.hh"

using namespace com::centreon::broker;
using namespace com::centreon::common;
using namespace com::centreon::broker::http_client;

extern std::shared_ptr<asio::io_context> g_io_context;

const asio::ip::tcp::endpoint test_endpoint(asio::ip::make_address("127.0.0.1"),
                                            1234);

class http_client_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    srand(time(nullptr));
    pool::load(g_io_context, 1);
    log_v2::tcp()->set_level(spdlog::level::info);
  };
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
    com::centreon::common::defer(
        _io_context, std::chrono::milliseconds(5),
        [me = shared_from_this(), cb = std::move(callback)]() { cb({}, {}); });
  }

  void send(request_ptr request, send_callback_type&& callback) override {
    if (_state != e_idle) {
      _io_context->post([cb = std::move(callback)]() {
        cb(std::make_error_code(std::errc::invalid_argument), "bad state", {});
      });
    } else {
      _keep_alive_end = system_clock::now() + std::chrono::hours(1);
      com::centreon::common::defer(
          _io_context, std::chrono::milliseconds(5),
          [me = shared_from_this(), cb = std::move(callback)]() {
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
  client::pointer clt =
      client::load(g_io_context, log_v2::tcp(),
                   std::make_shared<http_config>(test_endpoint, "localhost"),
                   [&conns](const std::shared_ptr<asio::io_context>& io_context,
                            const std::shared_ptr<spdlog::logger>& logger,
                            const http_config::pointer& conf) {
                     auto dummy_conn = std::make_shared<connection_ok>(
                         io_context, logger, conf);
                     conns.push_back(dummy_conn);
                     return dummy_conn;
                   });

  request_ptr request(std::make_shared<request_base>());

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
  auto conf = std::make_shared<http_config>(test_endpoint, "localhost");
  client::pointer clt =
      client::load(g_io_context, log_v2::tcp(), conf,
                   [&conns](const std::shared_ptr<asio::io_context>& io_context,
                            const std::shared_ptr<spdlog::logger>& logger,
                            const http_config::pointer& conf) {
                     auto dummy_conn = std::make_shared<connection_ok>(
                         io_context, logger, conf);
                     conns.push_back(dummy_conn);
                     return dummy_conn;
                   });

  struct sender {
    request_ptr request = std::make_shared<request_base>();
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
  ASSERT_EQ(clt->get_nb_keep_alive_conns(), 1);
  ASSERT_EQ(clt->get_nb_busy_conns(), 0);
  ASSERT_EQ(clt->get_nb_not_connected_cons(), conf->get_max_connections() - 1);
}

/************************************************************************
               network bagot
*************************************************************************/

class connection_bagot : public connection_base {
 public:
  enum fail_stage { no_fail = 0, fail_connect, fail_send };

 protected:
  fail_stage _fail_stage;

 public:
  connection_bagot(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const http_config::pointer& conf)
      : connection_base(io_context, logger, conf) {}

  fail_stage get_fail_stage() const { return _fail_stage; }

  void shutdown() override { _state = e_not_connected; }

  void connect(connect_callback_type&& callback) override {
    _fail_stage = fail_stage(rand() % 3);
    SPDLOG_LOGGER_DEBUG(_logger, "connection_bagot connect {:p} _fail_stage={}",
                        static_cast<void*>(this),
                        static_cast<uint32_t>(_fail_stage));

    if (_fail_stage == fail_stage::fail_connect) {
      _state = e_not_connected;

      _io_context->post([cb = std::move(callback)]() {
        cb(make_error_code(asio::error::host_unreachable), "connect_error");
      });
    } else {
      _state = e_idle;
      _io_context->post([cb = std::move(callback)]() { cb({}, {}); });
    }
  }

  void send(request_ptr request, send_callback_type&& callback) override {
    if (_state != e_idle) {
      _io_context->post([cb = std::move(callback)]() {
        cb(std::make_error_code(std::errc::invalid_argument), "bad state", {});
      });
    } else {
      _keep_alive_end = system_clock::now() + std::chrono::hours(1);
      if (_fail_stage == fail_send) {
        _io_context->post([cb = std::move(callback)]() {
          cb(make_error_code(asio::error::host_unreachable), "send error",
             nullptr);
        });
      } else {
        _io_context->post([me = this, cb = std::move(callback)]() {
          auto resp = std::make_shared<response_type>();
          bool keep_alive = rand() & 0x01;
          SPDLOG_LOGGER_DEBUG(me->_logger, "{:p} keepalive={}",
                              static_cast<void*>(me), keep_alive);
          resp->keep_alive(keep_alive);

          me->gest_keepalive(resp);

          cb({}, "", resp);
        });
      }
    }
  }
};

class client_test : public http_client::client {
 public:
  client_test(const std::shared_ptr<asio::io_context>& io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              const http_client::http_config::pointer& conf,
              http_client::client::connection_creator conn_creator)
      : http_client::client(io_context, logger, conf, conn_creator) {
    _retry_unit = std::chrono::milliseconds(1);
  }
};

TEST_F(http_client_test, all_handler_called) {
  client::pointer clt = std::make_shared<client_test>(
      g_io_context, log_v2::tcp(),
      std::make_shared<http_config>(test_endpoint, "localhost"),
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const http_config::pointer& conf) {
        auto dummy_conn =
            std::make_shared<connection_bagot>(io_context, logger, conf);
        return dummy_conn;
      });

  std::mutex cond_m;
  std::condition_variable var;
  int error_handler_cpt = 0;
  int success_handler_cpt = 0;
  for (unsigned request_index = 0; request_index < 1000; ++request_index) {
    request_ptr request = std::make_shared<request_base>();
    clt->send(request, [&](const boost::beast::error_code& err,
                           const std::string&, const response_ptr&) mutable {
      {
        std::lock_guard<std::mutex> l(cond_m);
        if (err) {
          ++error_handler_cpt;
        } else {
          ++success_handler_cpt;
        }
      }
      var.notify_one();
    });
  }

  std::unique_lock<std::mutex> l(cond_m);
  bool res_wait = var.wait_for(l, std::chrono::seconds(10), [&]() -> bool {
    return error_handler_cpt + success_handler_cpt == 1000;
  });

  SPDLOG_LOGGER_INFO(log_v2::tcp(), "success:{}, failed:{}",
                     success_handler_cpt, error_handler_cpt);
  ASSERT_NE(error_handler_cpt, 0);
  ASSERT_NE(success_handler_cpt, 0);
  ASSERT_EQ(error_handler_cpt + success_handler_cpt, 1000);
};

/************************************************************************
               retry
*************************************************************************/
class connection_retry : public connection_bagot {
 public:
  static std::map<request_ptr, unsigned> nb_failed_per_request;
  static unsigned failed_before_success;

  connection_retry(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const http_config::pointer& conf)
      : connection_bagot(io_context, logger, conf) {}

  void connect(connect_callback_type&& callback) override {
    _state = e_idle;
    _io_context->post([cb = std::move(callback)]() { cb({}, {}); });
  }

  void send(request_ptr request, send_callback_type&& callback) override {
    if (nb_failed_per_request.find(request) == nb_failed_per_request.end()) {
      nb_failed_per_request[request] = 0;
    }
    if (nb_failed_per_request[request] < failed_before_success) {
      ++nb_failed_per_request[request];
      _io_context->post([cb = std::move(callback)]() {
        cb(make_error_code(asio::error::host_unreachable), "send error",
           nullptr);
      });
    } else {
      _io_context->post([me = this, cb = std::move(callback)]() {
        auto resp = std::make_shared<response_type>();
        bool keep_alive = rand() & 0x01;
        SPDLOG_LOGGER_DEBUG(me->_logger, "{:p} keepalive={}",
                            static_cast<void*>(me), keep_alive);
        resp->keep_alive(keep_alive);

        me->gest_keepalive(resp);

        cb({}, "", resp);
      });
    }
  }
};

std::map<request_ptr, unsigned> connection_retry::nb_failed_per_request;
unsigned connection_retry::failed_before_success;

TEST_F(http_client_test, retry_until_success) {
  connection_retry::nb_failed_per_request.clear();
  auto conf = std::make_shared<http_config>(test_endpoint, "localhost");
  client::pointer clt = std::make_shared<client_test>(
      g_io_context, log_v2::tcp(), conf,
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const http_config::pointer& conf) {
        auto dummy_conn =
            std::make_shared<connection_retry>(io_context, logger, conf);
        return dummy_conn;
      });

  connection_retry::failed_before_success = conf->get_max_send_retry();

  std::mutex cond_m;
  std::condition_variable var;
  std::atomic_uint nb_success(0);
  bool failed = false;
  for (unsigned request_index = 0; request_index < 100; ++request_index) {
    request_ptr request = std::make_shared<request_base>();
    clt->send(request, [&](const boost::beast::error_code& err,
                           const std::string&, const response_ptr&) {
      if (err) {
        failed = true;
      } else {
        nb_success.fetch_add(1);
        var.notify_one();
      }
    });
  }

  std::unique_lock<std::mutex> l(cond_m);
  bool res_wait = var.wait_for(l, std::chrono::seconds(10),
                               [&]() -> bool { return nb_success == 100; });
  ASSERT_EQ(nb_success.load(), 100);
  ASSERT_TRUE(res_wait);
  ASSERT_FALSE(failed);
}
