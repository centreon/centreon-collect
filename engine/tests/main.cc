/**
 * Copyright 2016 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include "com/centreon/clib.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/engine/globals.hh"

std::shared_ptr<asio::io_context> g_io_context(
    std::make_shared<asio::io_context>());

using com::centreon::common::log_v2::log_v2;

class CentreonEngineEnvironment : public testing::Environment {
 public:
  void SetUp() override {
    setenv("TZ", ":Europe/Paris", 1);
    return;
  }

  void TearDown() override { return; }
};

void set_time(time_t now);

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
  testing::AddGlobalTestEnvironment(new CentreonEngineEnvironment());

  com::centreon::common::log_v2::log_v2::load("engine-tests");
  init_loggers();
  com::centreon::common::pool::load(g_io_context, runtime_logger);
  com::centreon::common::pool::set_pool_size(0);
  // Run all tests.
  int ret = RUN_ALL_TESTS();
  time_t now = time(NULL);
  set_time(now + 1000);
  g_io_context->stop();
  spdlog::shutdown();
  return ret;
}
