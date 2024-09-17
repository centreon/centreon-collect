/**
 * Copyright 2022-2023 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/victoria_metrics/connector.hh"
#include "com/centreon/broker/victoria_metrics/factory.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace nlohmann;

class VictoriaMetricsFactory : public testing::Test {};

TEST_F(VictoriaMetricsFactory, MissingParams) {
  victoria_metrics::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  http_tsdb::http_tsdb_config conf;

  bool is_acceptor;

  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr), msg_fmt);
  cfg.params["db_user"] = "admin";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr), msg_fmt);
  cfg.params["db_password"] = "pass";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr), msg_fmt);
  cfg.params["db_host"] = "host";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr), msg_fmt);
  cfg.params["db_name"] = "centreon";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr), msg_fmt);
  cfg.params["db_host"] = "localhost";
  ASSERT_NO_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr));
  cfg.params["db_host"] = "127.0.0.1";
  ASSERT_NO_THROW(fact.new_endpoint(cfg, {}, is_acceptor, nullptr));
  ASSERT_FALSE(is_acceptor);
}

TEST_F(VictoriaMetricsFactory, ParseParameter) {
  victoria_metrics::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);

  cfg.params["db_user"] = "admin";
  cfg.params["db_password"] = "pass";
  cfg.params["db_host"] = "localhost";
  cfg.params["encryption"] = "true";
  cfg.params["db_port"] = "1024";
  cfg.params["queries_per_transaction"] = "100";
  cfg.params["connect_timeout"] = "5";
  cfg.params["send_timeout"] = "6";
  cfg.params["receive_timeout"] = "7";
  cfg.params["second_tcp_keep_alive_interval"] = "8";
  cfg.params["default_http_keepalive_duration"] = "9";
  cfg.params["max_connections"] = "10";
  cfg.params["http_target"] = "/WriteVictoria";
  cfg.params["account_id"] = "my_account_id";
  cfg.cfg["metrics_column"] = R"([
    {"name" : "host", "is_tag" : "true", "value" : "$HOST$", "type":"string"}])"_json;

  bool is_acceptor;
  victoria_metrics::connector* conn = static_cast<victoria_metrics::connector*>(
      fact.new_endpoint(cfg, {}, is_acceptor, nullptr));
  ASSERT_FALSE(is_acceptor);

  const http_tsdb::http_tsdb_config& conf = *conn->get_conf();

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
  ASSERT_EQ(conf.get_http_target(), "/WriteVictoria");
  ASSERT_EQ(conn->get_account_id(), "my_account_id");
  ASSERT_EQ(conf.get_metric_columns().size(), 1);
}
