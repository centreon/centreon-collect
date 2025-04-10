/**
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

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"

using namespace com::centreon::broker;

class MultiplexingMuxerRead : public ::testing::Test {
 protected:
  std::shared_ptr<multiplexing::muxer> _m;

 public:
  void SetUp() override {
    try {
      config::applier::init(com::centreon::common::BROKER, "", 0, "test_broker",
                            0);
    } catch (std::exception const& e) {
      (void)e;
    }
  }

  void TearDown() override {
    _m.reset();
    config::applier::deinit();
  }

  void setup(std::string const& name) {
    multiplexing::muxer_filter f{io::raw::static_type()};
    _m = multiplexing::muxer::create(name, multiplexing::engine::instance_ptr(),
                                     f, f, false);
  }

  void publish_events(int count = 10000) {
    std::deque<std::shared_ptr<io::data>> q;
    for (int i = 0; i < count; ++i) {
      std::shared_ptr<io::raw> r{std::make_shared<io::raw>()};
      r->resize(sizeof(i));
      memcpy(r->data(), &i, sizeof(i));
      q.push_back(std::move(r));
    }
    _m->publish(q);
  }

  void reread_events(int from = 0, int to = 10000) {
    std::shared_ptr<io::data> d;
    for (int i = from; i < to; ++i) {
      d.reset();
      _m->read(d, 0);
      ASSERT_FALSE(!d);
      ASSERT_EQ(d->type(), io::raw::static_type());
      int reread;
      memcpy(&reread, std::static_pointer_cast<io::raw>(d)->data(),
             sizeof(reread));
      ASSERT_EQ(reread, i);
    }
  }
};

// Given a muxer object with all filters
// When some events are given to publish()
// Then I can read() the events back
TEST_F(MultiplexingMuxerRead, Read) {
  setup("MultiplexingMuxerRead_Read");
  publish_events();
  reread_events();
  std::shared_ptr<io::data> d;
  _m->read(d, 0);
  ASSERT_TRUE(!d);
}

// Given a muxer object with all filters
// And some events were given to write()
// And the events were read() back
// When I call nack_events()
// Then I can read() the events back
TEST_F(MultiplexingMuxerRead, NackEvents) {
  setup("MultiplexingMuxerRead_NackEvents");
  publish_events();
  reread_events();
  std::shared_ptr<io::data> d;
  _m->read(d, 0);
  ASSERT_TRUE(!d);
  _m->nack_events();
  reread_events();
  _m->read(d, 0);
  ASSERT_TRUE(!d);
}

// Given a muxer object with all filters
// And some events were given to write()
// And the events were read() back
// When all events are acknowledged with ack_events()
// Then I can read none of the events back
TEST_F(MultiplexingMuxerRead, AckEventsAll) {
  setup("MultiplexingMuxerRead_AckEventsAll");
  publish_events();
  reread_events();
  _m->ack_events(10000);
  std::shared_ptr<io::data> d;
  _m->read(d, 0);
  ASSERT_TRUE(!d);
}

// Given a muxer object with all filters
// And some events were given to write()
// And the events were read() back
// And events were partially acknowledged with ack_events()
// When I call nack_events()
// Then I can read() the unacknowledged events back
TEST_F(MultiplexingMuxerRead, AckEventsPartial) {
  setup("MultiplexingMuxerRead_AckEventsPartial");
  publish_events();
  reread_events();
  _m->ack_events(5000);
  _m->nack_events();
  reread_events(5000);
  std::shared_ptr<io::data> d;
  _m->read(d, 0);
  ASSERT_TRUE(!d);
}
