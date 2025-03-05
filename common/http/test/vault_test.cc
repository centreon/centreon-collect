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
#include <spdlog/sinks/stdout_color_sinks.h>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include "com/centreon/common/process/process.hh"
#include "http_config.hh"
#include "https_connection.hh"

#include "defer.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "http_client.hh"

using namespace com::centreon::common;
using namespace com::centreon::common::http;
extern std::shared_ptr<asio::io_context> g_io_context;

class vault_test : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = spdlog::stdout_color_mt("vault_test");
    _logger->set_level(spdlog::level::debug);
  };
};

TEST_F(vault_test, httpsConnection) {
  auto p = std::make_shared<process<false>>(
      g_io_context, _logger, "/usr/bin/perl " HTTP_TEST_DIR "/vault-server.pl");
  p->start_process(false);

  std::promise<std::string> promise;
  std::future<std::string> future = promise.get_future();
  asio::ip::tcp::resolver resolver(*g_io_context);
  std::string_view server_name("localhost");
  std::string_view server_port("4443");
  const auto results = resolver.resolve(server_name, server_port);
  ASSERT_FALSE(results.empty()) << "One endpoint expected at least on "
                                << server_name << ':' << server_port;
  http_config::pointer client_conf = std::make_shared<http_config>(
      results, server_name, true, std::chrono::seconds(10),
      std::chrono::seconds(30), std::chrono::seconds(30), 30,
      std::chrono::seconds(10), 5, std::chrono::hours(1), 1,
      asio::ssl::context_base::tlsv12_client);
  connection_creator conn_creator = [client_conf, logger = _logger]() {
    auto ssl_init = [](asio::ssl::context& ctx,
                       const http_config::pointer& conf [[maybe_unused]]) {
      ctx.set_verify_mode(asio::ssl::context::verify_none);
      // ctx.set_verify_mode(asio::ssl::context::verify_peer);
      // ctx.set_default_verify_paths();
    };
    return https_connection::load(g_io_context, logger, client_conf, ssl_init);
  };
  auto client = client::load(g_io_context, _logger, client_conf, conn_creator);
  auto req = std::make_shared<request_base>(
      boost::beast::http::verb::post, server_name, "/v1/auth/approle/login");
  req->body() = fmt::format("{{ \"role_id\":\"{}\", \"secret_id\":\"{}\" }}",
                            "abababab-abab-abab-abab-abababababab",
                            "abababab-abab-abab-abab-abababababab");
  req->content_length(req->body().length());
  std::string resp;
  client->send(req, [logger = _logger, &promise](
                        const boost::beast::error_code& err,
                        const std::string& detail [[maybe_unused]],
                        const response_ptr& response) mutable {
    logger->info("We are at the callback");
    if (err)
      logger->error("Error from http server: {}", err.message());
    else
      promise.set_value(response->body());
  });
  nlohmann::json js = nlohmann::json::parse(future.get());
  p->kill();
  ASSERT_EQ(js["auth"]["client_token"],
            std::string_view("hvs."
                             "key that does not exist"))
      << "No result received from https server after 20s";
}
