/**
 * Copyright 2011 - 2021 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

class Modules : public testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
    _logger = log_v2::instance().get(log_v2::CORE);
  }

  void TearDown() override { config::applier::deinit(); }
};

/**
 *  Verify that the module version checks work.
 */
TEST_F(Modules, loadNoVersion) {
  config::applier::modules modules(_logger);
  ASSERT_FALSE(modules.load_file(CENTREON_BROKER_TEST_MODULE_PATH
                                 "./libnull_module.so"));
}

TEST_F(Modules, loadBadVersion) {
  config::applier::modules modules(_logger);
  ASSERT_FALSE(modules.load_file(CENTREON_BROKER_TEST_MODULE_PATH
                                 "./libbad_version_module.so"));
}
