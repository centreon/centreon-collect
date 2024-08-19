/**
 * Copyright 2016-2024 Centreon
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
#include "common/engine_legacy_conf/service.hh"
#include <gtest/gtest.h>
#include "com/centreon/engine/exceptions/error.hh"

using namespace com::centreon::engine;

// Given a service configuration object
// When it is default constructed
// Then its acknowledgements timeout is set to 0
TEST(ConfigurationServiceAcknowledgementTimeoutTest, DefaultConstruction) {
  configuration::service s;
  ASSERT_EQ(0, s.acknowledgement_timeout());
}

// Given a service configuration object
// When the acknowledgement timeout is set to a positive value
// Then the method returns true
// And the value is properly set
TEST(ConfigurationServiceAcknowledgementTimeoutTest, SetToPositiveValue) {
  configuration::service s;
  ASSERT_TRUE(s.set_acknowledgement_timeout(42));
  ASSERT_EQ(42, s.acknowledgement_timeout());
}

// Given a service configuration object
// When the acknowledgement timeout is set to 0
// Then the method returns true
// And the value is properly set
TEST(ConfigurationServiceAcknowledgementTimeoutTest, SetToZero) {
  configuration::service s;
  s.set_acknowledgement_timeout(42);
  ASSERT_TRUE(s.set_acknowledgement_timeout(0));
  ASSERT_EQ(0, s.acknowledgement_timeout());
}

// Given a service configuration object
// When the acknowledgement timeout is set to a negative value
// Then the method returns false
// And the original value is not changed
TEST(ConfigurationServiceAcknowledgementTimeoutTest, SetToNegativeValue) {
  configuration::service s;
  s.set_acknowledgement_timeout(42);
  ASSERT_FALSE(s.set_acknowledgement_timeout(-36));
  ASSERT_EQ(42, s.acknowledgement_timeout());
}

TEST(ConfigurationServiceParseProperties, SetCustomVariable) {
  configuration::service s;
  ASSERT_TRUE(s.parse("_VARNAME", "TEST1"));
}
