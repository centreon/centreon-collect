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

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/container/flat_set.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/namespace.hh"

#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/http_tsdb/stream.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace nlohmann;

static std::shared_ptr<asio::io_context> io_context(
    std::make_shared<asio::io_context>());
static asio::executor_work_guard<asio::io_context::executor_type> worker(
    asio::make_work_guard(*io_context));
static std::thread io_context_t;

static std::shared_ptr<persistent_cache> _dummy_cache;

class http_tsdb_stream_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    srand(time(nullptr));
    std::thread t([]() {
      do {
        io_context->run();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (!io_context->stopped());
    });
    io_context_t.swap(t);

    log_v2::tcp()->set_level(spdlog::level::trace);
    file::disk_accessor::load(1000);
    _dummy_cache = std::make_shared<persistent_cache>("/tmp/toto");
  }
  static void TearDownTestSuite() {
    worker.reset();
    io_context->stop();
    io_context_t.join();
  }
};

class request_test : public http_tsdb::request {
  uint _request_id;

 public:
  static std::atomic_uint id_gen;

  request_test() : _request_id(id_gen.fetch_add(1)) {}

  void add_metric(const storage::metric& metric) override { ++_nb_metric; }
  void add_metric(const Metric& metric) override { ++_nb_metric; }

  void add_status(const storage::status& status) override { ++_nb_status; }
  void add_status(const Status& status) override { ++_nb_status; }

  unsigned get_request_id() const { return _request_id; }
};

std::atomic_uint request_test::id_gen(0);

class stream_test : public http_tsdb::stream {
 public:
  stream_test(const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
              http_client::client::connection_creator conn_creator =
                  http_client::http_connection::load)
      : http_tsdb::stream("stream_test",
                          io_context,
                          log_v2::tcp(),
                          conf,
                          _dummy_cache,
                          conn_creator) {}
  http_tsdb::request::pointer create_request() const override {
    return std::make_shared<request_test>();
  }
};

TEST_F(http_tsdb_stream_test, NotRead) {
  stream_test test(std::make_shared<http_tsdb::http_tsdb_config>());

  std::shared_ptr<io::data> d(std::make_shared<io::data>(1));
  ASSERT_THROW(test.read(d, 0), msg_fmt);
}

class connection_send_bagot : public http_client::connection_base {
 public:
  static std::atomic_uint success;

  connection_send_bagot(const std::shared_ptr<asio::io_context>& io_context,
                        const std::shared_ptr<spdlog::logger>& logger,
                        const http_client::http_config::pointer& conf)
      : connection_base(io_context, logger, conf) {}

  void shutdown() override { _state = e_not_connected; }

  void connect(http_client::connect_callback_type&& callback) override {
    _state = e_idle;
    _io_context->post([cb = std::move(callback)]() { cb({}, {}); });
  }

  void send(http_client::request_ptr request,
            http_client::send_callback_type&& callback) override {
    if (_state != e_idle) {
      _io_context->post([cb = std::move(callback)]() {
        cb(std::make_error_code(std::errc::invalid_argument), "bad state", {});
      });
    } else {
      if (rand() & 1) {
        _io_context->post([cb = std::move(callback), request]() {
          cb(std::make_error_code(std::errc::invalid_argument),
             fmt::format("errorrrr id:{} nb_data={}",
                         std::static_pointer_cast<request_test>(request)
                             ->get_request_id(),
                         std::static_pointer_cast<request_test>(request)
                             ->get_nb_data()),
             nullptr);
        });
      } else {
        success.fetch_add(std::static_pointer_cast<http_tsdb::request>(request)
                              ->get_nb_metric());
        log_v2::tcp()->info(
            "success id:{} nb_data={}",
            std::static_pointer_cast<request_test>(request)->get_request_id(),
            std::static_pointer_cast<request_test>(request)->get_nb_data());
        _io_context->post([cb = std::move(callback)]() {
          auto resp = std::make_shared<http_client::response_type>();
          resp->keep_alive(false);
          cb({}, "", resp);
        });
      }
    }
  }
};

std::atomic_uint connection_send_bagot::success(0);

TEST_F(http_tsdb_stream_test, all_event_sent) {
  http_client::http_config conf(
      asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 80), false,
      std::chrono::seconds(10), std::chrono::seconds(10),
      std::chrono::seconds(10), 30, std::chrono::seconds(10), 0,
      std::chrono::seconds(1), 5);

  std::shared_ptr<stream_test> str(std::make_shared<stream_test>(
      std::make_shared<http_tsdb::http_tsdb_config>(
          conf, 10, std::chrono::milliseconds(500)),
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const http_client::http_config::pointer& conf) {
        auto dummy_conn =
            std::make_shared<connection_send_bagot>(io_context, logger, conf);
        return dummy_conn;
      }));

  request_test::id_gen = 0;
  connection_send_bagot::success = 0;

  std::shared_ptr<storage::pb_metric> event(
      std::make_shared<storage::pb_metric>());

  for (unsigned request_index = 0; request_index < 1000; ++request_index) {
    str->write(event);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(1));  // to let io_context thread fo the job
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  ASSERT_EQ(connection_send_bagot::success, 1000);
}
