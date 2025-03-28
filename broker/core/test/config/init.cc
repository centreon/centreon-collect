/**
 * Copyright 2021 Centreon (https://www.centreon.com/)
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
#include "com/centreon/broker/config/applier/init.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker;

/**
 *  Check that the logger configuration class can be copied properly.
 *
 *  @return 0 on success.
 */
TEST(init, init) {
  // First object.
  config::applier::init(com::centreon::common::BROKER, 0, "test", 0);
  ASSERT_NO_THROW(config::applier::deinit());
}
