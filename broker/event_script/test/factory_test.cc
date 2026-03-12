/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/event_script/factory.hh"
#include <gtest/gtest.h>
#include "bbdo/storage.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "neb.pb.h"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

using pb_service_status =
    io::protobuf<ServiceStatus, make_type(io::neb, neb::de_pb_service_status)>;
using pb_adaptive_service_status =
    io::protobuf<AdaptiveServiceStatus,
                 make_type(io::neb, neb::de_pb_adaptive_service_status)>;

class event_script_factory : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    io::events& e(io::events::instance());

    e.register_event(make_type(io::neb, neb::de_pb_service_status),
                     "ServiceStatus", &pb_service_status::operations,
                     "services");
    e.register_event(make_type(io::neb, neb::de_pb_adaptive_service_status),
                     "AdaptiveServiceStatus",
                     &pb_adaptive_service_status::operations, "services");
  }
};

TEST_F(event_script_factory, no_filter) {
  event_script::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  std::shared_ptr<persistent_cache> cache;
  bool is_acceptor;

  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);
}

TEST_F(event_script_factory, several_filter) {
  event_script::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  std::shared_ptr<persistent_cache> cache;
  bool is_acceptor;

  cfg.params["script_path"] = "/toto.sh";

  cfg.write_filters.insert("all");
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);
  cfg.write_filters.clear();
  cfg.write_filters.insert("neb:ServiceStatus");
  cfg.write_filters.insert("neb:AdaptiveServiceStatus");
  ASSERT_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache), msg_fmt);
  cfg.write_filters.clear();
  cfg.write_filters.insert("neb:ServiceStatus");
  ASSERT_NO_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache));
}

TEST_F(event_script_factory, onefilter) {
  event_script::factory fact;
  config::endpoint cfg(config::endpoint::io_type::output);
  std::shared_ptr<persistent_cache> cache;
  bool is_acceptor;

  cfg.params["script_path"] = "/toto.sh";

  cfg.write_filters.insert("neb:AdaptiveServiceStatus");
  ASSERT_NO_THROW(fact.new_endpoint(cfg, {}, is_acceptor, cache));
}
