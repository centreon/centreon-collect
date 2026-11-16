/**
 * Copyright 2026 Centreon
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

#include <filesystem>
#include <thread>

#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/neb/internal.hh"

using namespace com::centreon::broker;

namespace {
/* P level: downtimes and acknowledgements. */
std::shared_ptr<io::data> P(uint64_t id) {
  auto d = std::make_shared<neb::pb_downtime>();
  d->mut_obj().set_id(id);
  return d;
}

/* C level: fresh host/service statuses. */
std::shared_ptr<io::data> C(uint64_t id) {
  auto s = std::make_shared<neb::pb_service_status>();
  s->mut_obj().set_service_id(id);
  return s;
}

/* H level: everything else. */
std::shared_ptr<io::data> H(uint64_t id) {
  auto l = std::make_shared<neb::pb_log_entry>();
  l->mut_obj().set_ctime(id);
  return l;
}

/* The neb module is not loaded in unit tests, so BBDO cannot serialize these
 * events to the memory file unless we declare them. */
void register_test_events() {
  io::events& e{io::events::instance()};
  e.register_event(neb::pb_downtime::static_type(), "Downtime",
                   &neb::pb_downtime::operations, "downtimes");
  e.register_event(neb::pb_service_status::static_type(), "ServiceStatus",
                   &neb::pb_service_status::operations, "services");
  e.register_event(neb::pb_log_entry::static_type(), "LogEntry",
                   &neb::pb_log_entry::operations, "logs");
}

std::string label(const std::shared_ptr<io::data>& d) {
  if (!d)
    return "none";
  if (d->type() == neb::pb_downtime::static_type())
    return fmt::format(
        "P{}", std::static_pointer_cast<neb::pb_downtime>(d)->obj().id());
  if (d->type() == neb::pb_service_status::static_type())
    return fmt::format(
        "C{}",
        std::static_pointer_cast<neb::pb_service_status>(d)->obj().service_id());
  if (d->type() == neb::pb_log_entry::static_type())
    return fmt::format(
        "H{}", std::static_pointer_cast<neb::pb_log_entry>(d)->obj().ctime());
  return "unexpected";
}
}  // namespace

class MultiplexingMuxerPriority : public ::testing::Test {
 protected:
  std::shared_ptr<multiplexing::muxer> _m;
  uint32_t _saved_threshold = 0;
  std::string _saved_cache_dir;
  std::filesystem::path _tmp_dir;

 public:
  void SetUp() override {
    try {
      config::applier::init<
          com::centreon::broker::config::applier::broker_state>(
          "", 0, "test_broker", 0);
    } catch (std::exception const& e) {
      (void)e;
    }
    _saved_threshold = multiplexing::muxer::priority_age_threshold();
    _saved_cache_dir = config::applier::state::instance().cache_dir();
  }

  void TearDown() override {
    _m.reset();
    multiplexing::muxer::priority_age_threshold(_saved_threshold);
    config::applier::state::instance().set_cache_dir(_saved_cache_dir);
    config::applier::deinit();
    if (!_tmp_dir.empty())
      std::filesystem::remove_all(_tmp_dir);
  }

  void setup(const std::string& name, bool persistent = false) {
    multiplexing::muxer_filter all;
    _m = multiplexing::muxer::create(name, multiplexing::engine::instance_ptr(),
                                     all, all, persistent);
  }

  void publish(std::initializer_list<std::shared_ptr<io::data>> events) {
    std::deque<std::shared_ptr<io::data>> q(events);
    _m->publish(q);
  }

  std::string read_one() {
    std::shared_ptr<io::data> d;
    _m->read(d, 0);
    return label(d);
  }

  std::vector<std::string> read_all() {
    std::vector<std::string> retval;
    for (;;) {
      std::string l = read_one();
      if (l == "none")
        break;
      retval.push_back(std::move(l));
    }
    return retval;
  }

  /* The muxer may still be referenced by a task posted on the thread pool. We
   * wait to hold the last reference so that the destructor (and the memory
   * file writing) runs here, synchronously. */
  bool destroy_muxer() {
    for (int i = 0; i < 500 && _m.use_count() > 1; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (_m.use_count() > 1)
      return false;
    _m.reset();
    return true;
  }
};

using strings = std::vector<std::string>;

// Given events of the three levels published in a mixed order
// When they are read
// Then P events come first, then C events, then H events, each level keeping
// its publication order.
TEST_F(MultiplexingMuxerPriority, ReadsPriorityThenCurrentThenHistorical) {
  setup("MultiplexingMuxerPriority_ReadOrder");
  publish({H(1), C(1), P(1), H(2), C(2), P(2)});
  ASSERT_EQ(read_all(), (strings{"P1", "P2", "C1", "C2", "H1", "H2"}));
}

// Given C events older than priority_age_threshold
// When they are read
// Then they have been demoted at the end of the H level.
TEST_F(MultiplexingMuxerPriority, StaleCurrentEventsAreDemotedBehindHistorical) {
  multiplexing::muxer::priority_age_threshold(0);
  setup("MultiplexingMuxerPriority_Demote");
  publish({C(1), H(1), C(2), P(1)});
  ASSERT_EQ(read_all(), (strings{"P1", "H1", "C1", "C2"}));
}

// Given a C event read and not acknowledged yet, and an unread C event
// When the unread C event becomes stale
// Then it is moved behind H, it is counted once, and the acknowledgements
// still follow the read order.
TEST_F(MultiplexingMuxerPriority, DemoteWhileCurrentEventsWaitForAck) {
  setup("MultiplexingMuxerPriority_DemoteUnacked");
  publish({C(1), C(2), H(1)});
  ASSERT_EQ(read_one(), "C1");
  multiplexing::muxer::priority_age_threshold(0);
  ASSERT_EQ(read_one(), "H1");
  ASSERT_EQ(read_one(), "C2");
  ASSERT_EQ(_m->get_event_queue_size(), 3u);
  _m->ack_events(1);
  _m->nack_events();
  ASSERT_EQ(read_all(), (strings{"H1", "C2"}));
  _m->ack_events(2);
  ASSERT_EQ(_m->get_event_queue_size(), 0u);
}

// Given an H event read before a P event was published and read
// When one event is acknowledged
// Then the acknowledged event is the H one (the first read), so after a nack
// only the P event is replayed.
TEST_F(MultiplexingMuxerPriority, AckRemovesEventsInReadOrderAcrossLevels) {
  setup("MultiplexingMuxerPriority_AckReadOrder");
  publish({H(1)});
  ASSERT_EQ(read_one(), "H1");
  publish({P(1)});
  ASSERT_EQ(read_one(), "P1");
  _m->ack_events(1);
  _m->nack_events();
  ASSERT_EQ(read_all(), (strings{"P1"}));
}

// Given events read from the three levels, H first, then C, then P
// When two events are acknowledged
// Then H1 and C1 are removed and only P1 is replayed after a nack.
TEST_F(MultiplexingMuxerPriority, AckFollowsReadOrderOverThreeLevels) {
  setup("MultiplexingMuxerPriority_AckThreeLevels");
  publish({H(1)});
  ASSERT_EQ(read_one(), "H1");
  publish({C(1)});
  ASSERT_EQ(read_one(), "C1");
  publish({P(1)});
  ASSERT_EQ(read_one(), "P1");
  _m->ack_events(2);
  _m->nack_events();
  ASSERT_EQ(read_all(), (strings{"P1"}));
}

// Given events partially read and partially acknowledged
// When nack_events() is called
// Then in each level the unacknowledged events are replayed before the ones
// never read.
TEST_F(MultiplexingMuxerPriority, NackReplaysUnackedBeforePending) {
  setup("MultiplexingMuxerPriority_Nack");
  publish({P(1), P(2), C(1), C(2), H(1), H(2)});
  ASSERT_EQ(read_one(), "P1");
  ASSERT_EQ(read_one(), "P2");
  ASSERT_EQ(read_one(), "C1");
  _m->ack_events(1);
  _m->nack_events();
  ASSERT_EQ(read_all(), (strings{"P2", "C1", "C2", "H1", "H2"}));
}

// Given events read with the bulk read()
// Then the order is the same as with the single read() and the return value
// tells whether events are still waiting.
TEST_F(MultiplexingMuxerPriority, BulkReadKeepsOrder) {
  setup("MultiplexingMuxerPriority_BulkRead");
  publish({H(1), C(1), P(1), H(2)});
  std::vector<std::shared_ptr<io::data>> v;
  ASSERT_TRUE(_m->read(v, 3));
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(label(v[0]), "P1");
  EXPECT_EQ(label(v[1]), "C1");
  EXPECT_EQ(label(v[2]), "H1");
  v.clear();
  ASSERT_FALSE(_m->read(v, 3));
  ASSERT_EQ(v.size(), 1u);
  EXPECT_EQ(label(v[0]), "H2");
}

// Given published, read and acknowledged events
// Then the queue size counts read-but-unacknowledged events and pending ones,
// and the statistics give the unacknowledged count.
TEST_F(MultiplexingMuxerPriority, QueueSizeCountsUnackedAndPending) {
  setup("MultiplexingMuxerPriority_Size");
  publish({P(1), C(1), H(1)});
  ASSERT_EQ(_m->get_event_queue_size(), 3u);
  read_one();
  read_one();
  _m->ack_events(1);
  ASSERT_EQ(_m->get_event_queue_size(), 2u);
  nlohmann::json tree;
  _m->statistics(tree);
  ASSERT_EQ(tree["unacknowledged_events"].get<size_t>(), 1u);
}

// Given a persistent muxer whose P and C levels both hold an unacknowledged
// event and a pending one
// When it is destroyed and created again
// Then the events come back, P first (unacked before pending), then the others
// in C then H order.
TEST_F(MultiplexingMuxerPriority, MemoryFileKeepsPriorityOrder) {
  register_test_events();
  _tmp_dir = std::filesystem::temp_directory_path() / "muxer_priority_test";
  std::filesystem::create_directories(_tmp_dir);
  config::applier::state::instance().set_cache_dir(_tmp_dir / "broker");
  const std::string name{"MultiplexingMuxerPriority_Memory"};

  setup(name, true);
  publish({C(1), C(2), C(3), H(1)});
  ASSERT_EQ(read_one(), "C1");
  ASSERT_EQ(read_one(), "C2");
  _m->ack_events(1);
  publish({P(1), P(2)});
  ASSERT_EQ(read_one(), "P1");
  // P: unacked P1, pending P2 — C: unacked C2, pending C3 — H: pending H1
  ASSERT_TRUE(destroy_muxer());

  setup(name, true);
  EXPECT_EQ(read_all(), (strings{"P1", "P2", "C2", "C3", "H1"}));
  _m->ack_events(5);
  ASSERT_TRUE(destroy_muxer());
}
