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
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/beast/ssl.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/http_tsdb/factory.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace nlohmann;

extern std::shared_ptr<asio::io_context> g_io_context;

class factory_test : public http_tsdb::factory {
 public:
  factory_test(const std::string& name,
               const std::shared_ptr<asio::io_context>& io_context
               [[maybe_unused]])
      : http_tsdb::factory(name, g_io_context) {}

  io::endpoint* new_endpoint(
      config::endpoint& cfg [[maybe_unused]],
      const std::map<std::string, std::string>& global_params [[maybe_unused]],
      bool& is_acceptor [[maybe_unused]],
      std::shared_ptr<persistent_cache> cache
      [[maybe_unused]] = std::shared_ptr<persistent_cache>()) const override {
    return nullptr;
  }
  void create_conf(const config::endpoint& cfg,
                   http_tsdb::http_tsdb_config& conf) const {
    http_tsdb::factory::create_conf(cfg, conf);
  }
};

TEST(HttpTsdbFactory, MissingParams) {
  factory_test fact("http_tsdb_test", g_io_context);
  config::endpoint cfg(config::endpoint::io_type::output);
  http_tsdb::http_tsdb_config conf;

  ASSERT_THROW(fact.create_conf(cfg, conf), msg_fmt);
  cfg.params["db_user"] = "admin";
  ASSERT_THROW(fact.create_conf(cfg, conf), msg_fmt);
  cfg.params["db_password"] = "pass";
  ASSERT_THROW(fact.create_conf(cfg, conf), msg_fmt);
  cfg.params["db_host"] = "host";
  ASSERT_THROW(fact.create_conf(cfg, conf), msg_fmt);
  cfg.params["db_host"] = "localhost";
  ASSERT_NO_THROW(fact.create_conf(cfg, conf));
  cfg.params["db_host"] = "127.0.0.1";
  ASSERT_NO_THROW(fact.create_conf(cfg, conf));
}

TEST(HttpTsdbFactory, DefaultParameter) {
  factory_test fact("http_tsdb_test", g_io_context);
  config::endpoint cfg(config::endpoint::io_type::output);
  http_tsdb::http_tsdb_config conf;

  cfg.params["db_user"] = "admin";
  cfg.params["db_password"] = "pass";
  cfg.params["db_host"] = "localhost";

  fact.create_conf(cfg, conf);

  ASSERT_FALSE(conf.is_crypted());
  ASSERT_EQ(conf.get_endpoint().port(), 80);
  ASSERT_EQ(conf.get_max_queries_per_transaction(), 1000);
  ASSERT_EQ(conf.get_connect_timeout(), std::chrono::seconds(10));
  ASSERT_EQ(conf.get_send_timeout(), std::chrono::seconds(10));
  ASSERT_EQ(conf.get_receive_timeout(), std::chrono::seconds(30));
  ASSERT_EQ(conf.get_second_tcp_keep_alive_interval(), 30);
  ASSERT_EQ(conf.get_default_http_keepalive_duration(), std::chrono::hours(1));
  ASSERT_EQ(conf.get_max_connections(), 5);
}

TEST(HttpTsdbFactory, ParseParameter) {
  factory_test fact("http_tsdb_test", g_io_context);
  config::endpoint cfg(config::endpoint::io_type::output);
  http_tsdb::http_tsdb_config conf;

  cfg.params["db_user"] = "admin";
  cfg.params["db_password"] = "pass";
  cfg.params["db_host"] = "localhost";
  cfg.params["encryption"] = "true";

  fact.create_conf(cfg, conf);
  ASSERT_TRUE(conf.is_crypted());
  ASSERT_EQ(conf.get_endpoint().port(), 443);

  cfg.params["db_port"] = "1024";
  cfg.params["queries_per_transaction"] = "100";
  cfg.params["connect_timeout"] = "5";
  cfg.params["send_timeout"] = "6";
  cfg.params["receive_timeout"] = "7";
  cfg.params["second_tcp_keep_alive_interval"] = "8";
  cfg.params["default_http_keepalive_duration"] = "9";
  cfg.params["max_connections"] = "10";

  fact.create_conf(cfg, conf);
  ASSERT_EQ(conf.get_user(), "admin");
  ASSERT_EQ(conf.get_pwd(), "pass");
  ASSERT_EQ(conf.get_endpoint().port(), 1024);
  ASSERT_EQ(conf.get_max_queries_per_transaction(), 100);
  ASSERT_EQ(conf.get_connect_timeout(), std::chrono::seconds(5));
  ASSERT_EQ(conf.get_send_timeout(), std::chrono::seconds(6));
  ASSERT_EQ(conf.get_receive_timeout(), std::chrono::seconds(7));
  ASSERT_EQ(conf.get_second_tcp_keep_alive_interval(), 8);
  ASSERT_EQ(conf.get_default_http_keepalive_duration(),
            std::chrono::seconds(9));
  ASSERT_EQ(conf.get_max_connections(), 10);
}
