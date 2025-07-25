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
#include "common/log_v2/log_v2.hh"
#include "pool.hh"

std::shared_ptr<asio::io_context> g_io_context(
    std::make_shared<asio::io_context>());

class CentreonEngineEnvironment : public testing::Environment {
 public:
  void SetUp() override {
    setenv("TZ", ":Europe/Paris", 1);
    return;
  }

  void TearDown() override { return; }
};

std::shared_ptr<spdlog::logger> pool_logger =
    std::make_shared<spdlog::logger>("pool_logger");

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
  sigignore(SIGPIPE);

  // Set specific environment.
  testing::AddGlobalTestEnvironment(new CentreonEngineEnvironment());

  com::centreon::common::pool::load(g_io_context, pool_logger);
  com::centreon::common::pool::set_pool_size(0);
  // Run all tests.
  int ret = RUN_ALL_TESTS();
  g_io_context->stop();
  spdlog::shutdown();
  return ret;
}
