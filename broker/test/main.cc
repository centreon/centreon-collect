/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using com::centreon::common::log_v2::log_v2;

std::shared_ptr<asio::io_context> g_io_context =
    std::make_shared<asio::io_context>();

class CentreonBrokerEnvironment : public testing::Environment {
 public:
  void SetUp() override {
    com::centreon::broker::config::applier::state::load();
    com::centreon::broker::io::protocols::load();
    com::centreon::broker::io::events::load();
  }

  void TearDown() override {
    com::centreon::broker::config::applier::state::unload();
  }
};

/**
 *  Tester entry point.
 *
 *  @param[in] argc  Argument count.
 *  @param[in] argv  Argument values.
 *
 *  @return 0 on success, any other value on failure.
 */
int main(int argc, char* argv[]) {
  // GTest initialization.
  testing::InitGoogleTest(&argc, argv);

  // Set specific environment.
  testing::AddGlobalTestEnvironment(new CentreonBrokerEnvironment());

  log_v2::load("test");
  com::centreon::common::pool::load(g_io_context,
                                    log_v2::instance().get(log_v2::CORE));
  com::centreon::common::pool::set_pool_size(0);

  // Run all tests.
  int ret = RUN_ALL_TESTS();
  spdlog::shutdown();
  g_io_context->stop();
  com::centreon::common::pool::unload();
  return ret;
}
