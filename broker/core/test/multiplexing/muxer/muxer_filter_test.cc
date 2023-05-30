/*
 * Copyright 2011 - 2023 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"

using namespace com::centreon::broker;

class muxer_filter_test : public ::testing::Test {
 public:
  void SetUp() override {
    io::protocols::load();
    io::events::load();
  }

  void TearDown() override {
    io::events::unload();
    io::protocols::unload();
  }
};

TEST_F(muxer_filter_test, all) {
  auto event_types = config::applier::endpoint::parse_filter({"all"});
  ASSERT_FALSE(event_types.empty());
  multiplexing::muxer_filter all_filter(event_types.begin(), event_types.end());

  for (uint32_t event_type : event_types) {
    ASSERT_TRUE(all_filter.allowed(event_type));
  }
}

TEST_F(muxer_filter_test, bbdo) {
  // events constructor only register some bbdo events
  auto event_types = config::applier::endpoint::parse_filter({"all"});
  ASSERT_FALSE(event_types.empty());
  multiplexing::muxer_filter all_filter(event_types.begin(), event_types.end());

  ASSERT_TRUE(all_filter.allowed(make_type(io::bbdo, bbdo::de_welcome)));
  ASSERT_FALSE(all_filter.allowed(make_type(io::neb, neb::de_comment)));
}