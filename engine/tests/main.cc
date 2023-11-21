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
#include "com/centreon/engine/globals.hh"
#include "common/log_v2/log_v2.hh"

using com::centreon::common::log_v2::log_v2;

std::shared_ptr<asio::io_context> g_io_context(
    std::make_shared<asio::io_context>());
bool g_io_context_started = false;

class CentreonEngineEnvironment : public testing::Environment {
 public:
  void SetUp() override {
    setenv("TZ", ":Europe/Paris", 1);
    return;
  }

  void TearDown() override { return; }
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
  testing::AddGlobalTestEnvironment(new CentreonEngineEnvironment());

  com::centreon::common::log_v2::log_v2::load(
      "engine-tests",
      {log_v2::CORE, log_v2::CONFIG, log_v2::PROCESS, log_v2::CHECKS,
       log_v2::COMMANDS, log_v2::FUNCTIONS, log_v2::MACROS,
       log_v2::EXTERNAL_COMMAND, log_v2::EVENTS, log_v2::NOTIFICATIONS,
       log_v2::RUNTIME, log_v2::DOWNTIMES});
  init_loggers();
  // Run all tests.
  int ret = RUN_ALL_TESTS();
  spdlog::shutdown();
  return ret;
}
