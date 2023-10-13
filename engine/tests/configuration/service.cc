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

#include "common/configuration/service_helper.hh"

using namespace com::centreon::engine;

// Given a service configuration object
// When it is default constructed
// Then its acknowledgements timeout is set to 0
TEST(ConfigurationServiceAcknowledgementTimeoutTest, DefaultConstruction) {
  configuration::Service s;
  configuration::service_helper hlp(&s);
  ASSERT_EQ(0, s.acknowledgement_timeout());
}

// Given a service configuration object
// When the acknowledgement timeout is set to a positive value
// Then the method returns true
// And the value is properly set
TEST(ConfigurationServiceAcknowledgementTimeoutTest, SetToPositiveValue) {
  configuration::Service s;
  configuration::service_helper s_hlp(&s);
  s.set_acknowledgement_timeout(42);
  ASSERT_EQ(42, s.acknowledgement_timeout());
}

// Given a service configuration object
// When the acknowledgement timeout is set to 0
// Then the method returns true
// And the value is properly set
TEST(ConfigurationServiceAcknowledgementTimeoutTest, SetToZero) {
  configuration::Service s;
  configuration::service_helper s_hlp(&s);
  s.set_acknowledgement_timeout(42);
  s.set_acknowledgement_timeout(0);
  ASSERT_EQ(0, s.acknowledgement_timeout());
}

TEST(ConfigurationServiceParseProperties, SetCustomVariable) {
  configuration::Service s;
  configuration::service_helper s_hlp(&s);
  ASSERT_TRUE(s_hlp.insert_customvariable("_VARNAME", "TEST1"));
}
