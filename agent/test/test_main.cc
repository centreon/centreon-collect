/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>

#ifdef _WIN32
#include "ntdll.hh"
#endif

std::shared_ptr<asio::io_context> g_io_context(
    std::make_shared<asio::io_context>());

class CentreonEngineEnvironment : public testing::Environment {
 public:
  void SetUp() override {
#ifndef _WIN32
    setenv("TZ", ":Europe/Paris", 1);
#else
    com::centreon::agent::load_nt_dll();
#endif
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

  spdlog::default_logger()->set_level(spdlog::level::trace);

  // Set specific environment.
  testing::AddGlobalTestEnvironment(new CentreonEngineEnvironment());

  auto worker{asio::make_work_guard(*g_io_context)};
  std::thread asio_thread([]() { g_io_context->run(); });
  // Run all tests.
  int ret = RUN_ALL_TESTS();
  g_io_context->stop();
  asio_thread.join();
  spdlog::shutdown();
  return ret;
}
