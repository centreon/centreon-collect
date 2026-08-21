/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

/**
 * Does a muxer still deliver when events reach it before its data handler is
 * set?
 *
 * A feeder leaves that window open by construction: muxer::create() subscribes
 * the muxer to the engine (muxer.cc:195) and returns, and only afterwards does
 * feeder::init() call set_action_on_new_data() (feeder.cc:69). Anything the
 * engine publishes in between reaches a muxer that has no handler yet.
 *
 * What made the window harmful was muxer::_execute_reader_if_needed(). It flips
 * _reader_running from false to true and posts the reader task; the task reads
 * _data_handler, and `_reader_running.store(false)` used to sit *inside* the
 * `if (to_call)` that guards against a null handler. A task running while the
 * handler was still null therefore left the flag armed for good, its
 * compare_exchange never succeeded again, and no reader was ever posted for
 * that muxer — a feeder that delivered nothing for the rest of its life, and
 * across reconnections and reloads too, since muxer::create() reuses muxers by
 * name. Setting the handler did not wake the reader either, so a muxer could
 * also sit on a queue it had inherited.
 *
 * Both are fixed; these three tests are the regression guard. Only the muxer is
 * exercised, not the engine: publish() is called directly, so the outcome does
 * not depend on the engine's fan-out or on its write filter pre-selection.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"

using namespace com::centreon::broker;

namespace {

/**
 * @brief A data handler that accepts everything and only counts.
 *
 * Returning the full size matters: a short return is what makes the reader task
 * call clear_action_on_new_data(), which is the other way into the state under
 * test and would blur what these tests measure.
 */
class counting_handler : public multiplexing::muxer::data_handler {
  std::atomic<uint32_t> _received{0};

 public:
  uint32_t on_events(
      const std::vector<std::shared_ptr<io::data>>& events) override {
    _received.fetch_add(events.size(), std::memory_order_relaxed);
    return events.size();
  }

  uint32_t received() const {
    return _received.load(std::memory_order_relaxed);
  }
};

}  // namespace

class MultiplexingMuxerReaderRunning : public ::testing::Test {
 protected:
  std::shared_ptr<multiplexing::muxer> _m;
  std::shared_ptr<counting_handler> _handler;

 public:
  void SetUp() override {
    try {
      config::applier::init<
          com::centreon::broker::config::applier::broker_state>(
          "", 0, "test_broker", 0);
    } catch (std::exception const& e) {
      (void)e;
    }
    _handler = std::make_shared<counting_handler>();
  }

  void TearDown() override {
    if (_m)
      _m->clear_action_on_new_data();
    _m.reset();
    _handler.reset();
    config::applier::deinit();
  }

  void setup(const std::string& name) {
    multiplexing::muxer_filter f{io::raw::static_type()};
    _m = multiplexing::muxer::create(name, multiplexing::engine::instance_ptr(),
                                     f, f, false);
  }

  /**
   * @brief Hand `count` events straight to the muxer.
   *
   * muxer::publish() is what the engine's workers call, and it is also what
   * calls _execute_reader_if_needed() once at least one event made it into the
   * queue.
   */
  void publish_events(int count) {
    std::deque<std::shared_ptr<io::data>> q;
    for (int i = 0; i < count; ++i) {
      auto r = std::make_shared<io::raw>();
      r->resize(sizeof(i));
      memcpy(r->data(), &i, sizeof(i));
      q.push_back(std::move(r));
    }
    _m->publish(q);
  }

  /**
   * @brief Poll until the handler has been given `expected` events.
   *
   * @return false if it never happened within the timeout.
   */
  bool wait_received(uint32_t expected) {
    for (int i = 0; i < 200; ++i) {  // 200 × 25 ms = 5 s
      if (_handler->received() >= expected)
        return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
  }
};

// Given a muxer whose data handler is set before anything is published
// When events are published
// Then the handler is given them
//
// The control: it fixes what "the muxer delivers" looks like, so that a failure
// of the next test cannot be blamed on the harness.
TEST_F(MultiplexingMuxerReaderRunning, HandlerSetBeforePublish) {
  setup("MultiplexingMuxerReaderRunning_HandlerSetBeforePublish");
  _m->set_action_on_new_data(_handler);

  publish_events(10);

  ASSERT_TRUE(wait_received(10))
      << "the handler was set before publishing and still received only "
      << _handler->received() << " events out of 10";
}

// Given a muxer that receives events before its data handler is set
// When the handler is set afterwards and more events are published
// Then the handler is given them
//
// This is the feeder startup window. The first publish arms _reader_running and
// posts a reader task that finds no handler; the question is whether the muxer
// recovers once the handler arrives, or whether that first task left the flag
// armed for good.
TEST_F(MultiplexingMuxerReaderRunning, HandlerSetAfterPublish) {
  setup("MultiplexingMuxerReaderRunning_HandlerSetAfterPublish");

  publish_events(10);

  /* Let the reader task posted by that publish actually run while the handler
   * is still null — the whole point of the test is what that task leaves
   * behind. A wait rather than a synchronisation point because the task is
   * observable by nothing when it finds no handler: it reads _data_handler and
   * returns. */
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  ASSERT_EQ(_handler->received(), 0u)
      << "the handler cannot have been called, it is not set yet";

  _m->set_action_on_new_data(_handler);
  publish_events(10);

  ASSERT_TRUE(wait_received(20))
      << "the muxer stopped delivering after a publish that preceded its "
         "handler: received "
      << _handler->received()
      << " events out of 20. _reader_running is stuck at true because "
         "_reader_running.store(false) sits inside the `if (to_call)` of "
         "muxer::_execute_reader_if_needed()";
}

// Given a muxer holding events and no data handler
// When a handler is set and nothing else is published
// Then the handler is given the events it inherited
//
// Setting the handler has to wake the reader by itself. Only publish() ever
// does it otherwise, and a feeder towards a poller — whose muxer accepts
// configuration and commands only — may wait a long time for the next one.
TEST_F(MultiplexingMuxerReaderRunning, HandlerInheritsAQueue) {
  setup("MultiplexingMuxerReaderRunning_HandlerInheritsAQueue");

  publish_events(10);
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  _m->set_action_on_new_data(_handler);

  ASSERT_TRUE(wait_received(10))
      << "setting the handler did not wake the reader: received "
      << _handler->received()
      << " events out of the 10 already queued, and nothing was published "
         "afterwards to wake it";
}
