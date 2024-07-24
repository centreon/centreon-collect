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

#include "common/engine_conf/host_helper.hh"

using namespace com::centreon::engine;

// Given a host configuration object
// When it is default constructed
// Then its acknowledgements timeout is set to 0
TEST(ConfigurationHostAcknowledgementTimeoutTest, PbDefaultConstruction) {
  configuration::Host h;
  configuration::host_helper hlp(&h);
  ASSERT_EQ(0, h.acknowledgement_timeout());
}
