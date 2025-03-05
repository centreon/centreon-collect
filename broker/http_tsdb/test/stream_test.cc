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
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/container/flat_set.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/http_tsdb/stream.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::common;
using namespace nlohmann;

extern std::shared_ptr<asio::io_context> g_io_context;

class http_tsdb_stream_test : public ::testing::Test {
 protected:
  static std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    srand(time(nullptr));

    _logger = log_v2::log_v2::instance().get(log_v2::log_v2::TCP);
    _logger->set_level(spdlog::level::info);
    file::disk_accessor::load(1000);
  }
};

std::shared_ptr<spdlog::logger> http_tsdb_stream_test::_logger;

class request_test : public http_tsdb::request {
  uint _request_id;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  static std::atomic_uint id_gen;

  request_test() : _request_id(id_gen.fetch_add(1)) {
    _logger = log_v2::log_v2::instance().get(log_v2::log_v2::TCP);
    SPDLOG_LOGGER_TRACE(_logger, "create request {}", _request_id);
  }

  ~request_test() {
    SPDLOG_LOGGER_TRACE(_logger, "delete request {}", _request_id);
  }

  void add_metric(const storage::pb_metric& metric [[maybe_unused]]) override {
    ++_nb_metric;
  }

  void add_status(const storage::pb_status& status [[maybe_unused]]) override {
    ++_nb_status;
  }

  unsigned get_request_id() const { return _request_id; }

  void dump(std::ostream& str) const override {
    str << "request id:" << _request_id << ' ';
    http_tsdb::request::dump(str);
  }
};

std::atomic_uint request_test::id_gen(0);

class stream_test : public http_tsdb::stream {
 public:
  stream_test(const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
              http::connection_creator conn_creator)
      : http_tsdb::stream("stream_test",
                          g_io_context,
                          log_v2::log_v2::instance().get(log_v2::log_v2::TCP),
                          conf,
                          conn_creator) {}
  http_tsdb::request::pointer create_request() const override {
    return std::make_shared<request_test>();
  }
};

TEST_F(http_tsdb_stream_test, NotRead) {
  auto conf = std::make_shared<http_tsdb::http_tsdb_config>();
  http::connection_creator conn_creator = [conf]() {
    return http::http_connection::load(
        g_io_context, log_v2::log_v2::instance().get(log_v2::log_v2::TCP),
        conf);
  };
  stream_test test(conf, conn_creator);

  std::shared_ptr<io::data> d(std::make_shared<io::data>(1));
  ASSERT_THROW(test.read(d, 0), msg_fmt);
}

class connection_send_bagot : public http::connection_base {
  asio::ip::tcp::socket _not_used;

 public:
  static std::atomic_uint success;
  static std::condition_variable success_cond;

  connection_send_bagot(const std::shared_ptr<asio::io_context>& io_context,
                        const std::shared_ptr<spdlog::logger>& logger,
                        const http::http_config::pointer& conf)
      : connection_base(io_context, logger, conf), _not_used(*io_context) {}

  void shutdown() override { _state = e_not_connected; }

  void connect(http::connect_callback_type&& callback) override {
    _state = e_idle;
    _io_context->post([cb = std::move(callback)]() { cb({}, {}); });
  }

  void send(http::request_ptr request,
            http::send_callback_type&& callback) override {
    if (_state != e_idle) {
      _io_context->post([cb = std::move(callback)]() {
        cb(std::make_error_code(std::errc::invalid_argument), "bad state", {});
      });
    } else {
      if (rand() & 3) {
        SPDLOG_LOGGER_ERROR(
            _logger, "fail id:{} nb_data={}",
            std::static_pointer_cast<request_test>(request)->get_request_id(),
            std::static_pointer_cast<request_test>(request)->get_nb_data());
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
        success_cond.notify_one();
        SPDLOG_LOGGER_DEBUG(
            _logger, "success id:{} nb_data={}",
            std::static_pointer_cast<request_test>(request)->get_request_id(),
            std::static_pointer_cast<request_test>(request)->get_nb_data());
        _io_context->post([cb = std::move(callback)]() {
          auto resp = std::make_shared<http::response_type>();
          resp->keep_alive(false);
          cb({}, "", resp);
        });
      }
    }
  }

  void _on_accept(http::connect_callback_type&& callback
                  [[maybe_unused]]) override{};

  void answer(const http::response_ptr& response [[maybe_unused]],
              http::answer_callback_type&& callback [[maybe_unused]]) override {
  }
  void receive_request(http::request_callback_type&& callback
                       [[maybe_unused]]) override {}

  asio::ip::tcp::socket& get_socket() { return _not_used; }
};

std::atomic_uint connection_send_bagot::success(0);
std::condition_variable connection_send_bagot::success_cond;

TEST_F(http_tsdb_stream_test, all_event_sent) {
  http::http_config conf(
      asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 80),
      "localhost", false, std::chrono::seconds(10), std::chrono::seconds(10),
      std::chrono::seconds(10), 30, std::chrono::seconds(1), 100,
      std::chrono::seconds(1), 5);

  auto tsdb_conf = std::make_shared<http_tsdb::http_tsdb_config>(conf, 10);

  std::shared_ptr<stream_test> str(
      std::make_shared<stream_test>(tsdb_conf, [tsdb_conf]() {
        auto dummy_conn = std::make_shared<connection_send_bagot>(
            g_io_context, log_v2::log_v2::instance().get(log_v2::log_v2::TCP),
            tsdb_conf);
        return dummy_conn;
      }));

  request_test::id_gen = 0;
  connection_send_bagot::success = 0;

  for (unsigned request_index = 0; request_index < 1000; ++request_index) {
    std::shared_ptr<storage::pb_metric> event(
        std::make_shared<storage::pb_metric>());
    event->mut_obj().set_metric_id(request_index);
    str->write(event);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(1));  // to let io_context thread fo the job
  }
  str->flush();
  SPDLOG_LOGGER_DEBUG(_logger, "wait");
  std::mutex dummy;
  std::unique_lock<std::mutex> l(dummy);
  connection_send_bagot::success_cond.wait_for(
      l, std::chrono::seconds(60),
      []() { return connection_send_bagot::success == 1000; });
  ASSERT_EQ(connection_send_bagot::success, 1000);
}
