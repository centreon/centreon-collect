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

#include "grpc_test_include.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;
using unique_lock = std::unique_lock<std::mutex>;

#include <com/centreon/broker/grpc/acceptor.hh>
#include "com/centreon/broker/grpc/factory.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

TEST(grpc_factory, HasEndpoint) {
  com::centreon::broker::grpc::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);

  cfg.type = "grpc";
  ASSERT_TRUE(fact.has_endpoint(cfg, nullptr));
  cfg.type = "tcp";
  ASSERT_FALSE(fact.has_endpoint(cfg, nullptr));
}

TEST(grpc_factory, Exception) {
  com::centreon::broker::grpc::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  bool is_acceptor;
  std::shared_ptr<persistent_cache> cache;

  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);
}

TEST(grpc_factory, Acceptor) {
  com::centreon::broker::grpc::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  bool is_acceptor;
  std::shared_ptr<persistent_cache> cache;

  cfg.type = "grpc";
  cfg.params["port"] = "4343";
  io::endpoint* endp = fact.new_endpoint(cfg, {}, is_acceptor, cache);

  ASSERT_TRUE(is_acceptor);
  ASSERT_TRUE(endp->is_acceptor());

  delete endp;
}

TEST(grpc_factory, BadPort) {
  com::centreon::broker::grpc::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  bool is_acceptor;
  std::shared_ptr<persistent_cache> cache;

  cfg.type = "grpc";
  cfg.params["port"] = "a4a343";
  cfg.params["host"] = "10.12.13.22";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);
}

TEST(grpc_factory, BadHost) {
  com::centreon::broker::grpc::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  bool is_acceptor;
  std::shared_ptr<persistent_cache> cache;

  cfg.type = "grpc";
  cfg.params["port"] = "4343";
  cfg.params["host"] = " 10.12.13.22";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);

  cfg.params["host"] = "10.12.13.22 ";
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);
}

TEST(grpc_factory, Connector) {
  com::centreon::broker::grpc::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  bool is_acceptor;
  std::shared_ptr<persistent_cache> cache;

  cfg.type = "grpc";
  cfg.params["port"] = "4444";
  cfg.params["host"] = "127.0.0.1";
  std::unique_ptr<io::factory> f{new com::centreon::broker::grpc::factory};
  ASSERT_TRUE(f->has_endpoint(cfg, nullptr));
  std::unique_ptr<io::endpoint> endp{
      fact.new_endpoint(cfg, {}, is_acceptor, cache)};

  ASSERT_FALSE(is_acceptor);
  ASSERT_TRUE(endp->is_connector());
}
