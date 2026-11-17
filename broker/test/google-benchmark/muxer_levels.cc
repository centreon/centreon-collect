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

/**
 * Benchmark of the muxer queues (P, C and H priority levels).
 *
 * Models the muxer operations from multiplexing/src/muxer.cc:
 *  - publish():     push_back into the level of the event
 *  - read():        P first, then C, then H
 *  - ack_events():  remove the oldest read events
 *  - nack_events(): replay the unacknowledged events
 *
 * Both keep one deque per level: the read events stay in it until they are
 * acknowledged, and a read position separates them from the unread ones.
 *
 * AckByLevel mirrors the previous implementation: an element by element read
 * and acknowledgements taken from P, then C, then H, whatever the read order.
 *
 * AckByReadOrder mirrors the current one: the bulk read copies a range per
 * level, and a log of the levels read in a row lets the acknowledgements
 * follow the read order.
 *
 * Events are shared_ptr<int> created before the timed loops, so only the queue
 * cost is measured (the muxer also copies a shared_ptr per event read).
 */

#include <benchmark/benchmark.h>

#include <array>
#include <deque>
#include <memory>
#include <vector>

using event_ptr = std::shared_ptr<int>;

/* 10% P, 30% C, 60% H. */
static uint8_t level_of(size_t i) {
  size_t r = i % 10;
  return r == 0 ? 0 : r < 4 ? 1 : 2;
}

static std::vector<event_ptr> make_events(size_t n) {
  std::vector<event_ptr> retval;
  retval.reserve(n);
  for (size_t i = 0; i < n; ++i)
    retval.push_back(std::make_shared<int>(static_cast<int>(i)));
  return retval;
}

// ─── previous implementation ────────────────────────────────────────────────

struct AckByLevel {
  struct entry {
    int64_t timestamp;
    event_ptr event;
  };
  std::array<std::deque<entry>, 3> q;
  std::array<size_t, 3> read_pos{};

  void push(uint8_t lvl, const event_ptr& e, int64_t ts) {
    q[lvl].push_back({ts, e});
  }

  const event_ptr* read_next() {
    for (size_t l = 0; l < 3; ++l)
      if (read_pos[l] < q[l].size())
        return &q[l][read_pos[l]++].event;
    return nullptr;
  }

  void read(std::vector<event_ptr>& out, size_t max) {
    size_t n = 0;
    for (size_t l = 0; l < 3; ++l)
      while (read_pos[l] < q[l].size() && n < max) {
        out.push_back(q[l][read_pos[l]++].event);
        ++n;
      }
  }

  void ack(size_t count) {
    for (size_t l = 0; l < 3 && count; ++l) {
      size_t k = std::min(count, read_pos[l]);
      q[l].erase(q[l].begin(), q[l].begin() + k);
      read_pos[l] -= k;
      count -= k;
    }
  }

  void nack() { read_pos = {}; }
};

// ─── current implementation ─────────────────────────────────────────────────

struct AckByReadOrder {
  struct entry {
    event_ptr event;
    int64_t timestamp;
  };
  struct level_queue {
    std::deque<entry> events;
    size_t read = 0;
    size_t unread() const { return events.size() - read; }
  };
  struct read_run {
    uint8_t lvl;
    size_t count;
  };
  std::array<level_queue, 3> q;
  std::deque<read_run> read_order;

  void push(uint8_t lvl, const event_ptr& e, int64_t ts) {
    q[lvl].events.push_back({e, ts});
  }

  void mark_read(uint8_t lvl, size_t count) {
    q[lvl].read += count;
    if (!read_order.empty() && read_order.back().lvl == lvl)
      read_order.back().count += count;
    else
      read_order.push_back({lvl, count});
  }

  const event_ptr* read_next() {
    for (uint8_t l = 0; l < 3; ++l) {
      level_queue& lq = q[l];
      if (lq.unread() > 0) {
        const event_ptr* retval = &lq.events[lq.read].event;
        mark_read(l, 1);
        return retval;
      }
    }
    return nullptr;
  }

  void read(std::vector<event_ptr>& out, size_t max) {
    for (uint8_t l = 0; l < 3 && max > 0; ++l) {
      level_queue& lq = q[l];
      const size_t count = std::min(max, lq.unread());
      if (count == 0)
        continue;
      const auto first = lq.events.cbegin() + lq.read;
      const auto last = first + count;
      for (auto it = first; it != last; ++it)
        out.push_back(it->event);
      mark_read(l, count);
      max -= count;
    }
  }

  void ack(size_t count) {
    while (count > 0 && !read_order.empty()) {
      read_run& run = read_order.front();
      const size_t n = std::min(count, run.count);
      level_queue& lq = q[run.lvl];
      lq.events.erase(lq.events.begin(), lq.events.begin() + n);
      lq.read -= n;
      run.count -= n;
      count -= n;
      if (run.count == 0)
        read_order.pop_front();
    }
  }

  void nack() {
    for (level_queue& lq : q)
      lq.read = 0;
    read_order.clear();
  }
};

// ─── Scenario 1: steady state ────────────────────────────────────────────────
// A long-lived muxer: push a batch, read it, ack it.

template <typename Q>
static void BM_SteadyState(benchmark::State& state) {
  const size_t batch = static_cast<size_t>(state.range(0));
  std::vector<event_ptr> events = make_events(batch);
  std::vector<event_ptr> buf;
  buf.reserve(batch);
  Q q;
  for (auto _ : state) {
    for (size_t i = 0; i < batch; ++i)
      q.push(level_of(i), events[i], static_cast<int64_t>(i));
    buf.clear();
    q.read(buf, batch);
    benchmark::DoNotOptimize(buf.data());
    q.ack(buf.size());
  }
  state.SetItemsProcessed(state.iterations() * batch);
}

BENCHMARK(BM_SteadyState<AckByLevel>)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Name("MuxerLevels/SteadyState/AckByLevel");
BENCHMARK(BM_SteadyState<AckByReadOrder>)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Name("MuxerLevels/SteadyState/AckByReadOrder");

// ─── Scenario 2: backlog drain ───────────────────────────────────────────────
// Reconnection after a long absence: big queue, read in batches of 1000 with
// an ack after each batch.

template <typename Q>
static void BM_BacklogDrain(benchmark::State& state) {
  const size_t n = static_cast<size_t>(state.range(0));
  std::vector<event_ptr> events = make_events(n);
  std::vector<event_ptr> buf;
  buf.reserve(1000);
  Q q;
  for (auto _ : state) {
    for (size_t i = 0; i < n; ++i)
      q.push(level_of(i), events[i], static_cast<int64_t>(i));
    for (;;) {
      buf.clear();
      q.read(buf, 1000);
      if (buf.empty())
        break;
      benchmark::DoNotOptimize(buf.data());
      q.ack(buf.size());
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(BM_BacklogDrain<AckByLevel>)
    ->RangeMultiplier(8)
    ->Range(1 << 10, 1 << 19)
    ->Name("MuxerLevels/BacklogDrain/AckByLevel");
BENCHMARK(BM_BacklogDrain<AckByReadOrder>)
    ->RangeMultiplier(8)
    ->Range(1 << 10, 1 << 19)
    ->Name("MuxerLevels/BacklogDrain/AckByReadOrder");

// ─── Scenario 3: partial ack then nack ───────────────────────────────────────
// A peer failure: half the read events are acked, the rest is replayed.

template <typename Q>
static void BM_PartialAckNack(benchmark::State& state) {
  const size_t n = static_cast<size_t>(state.range(0));
  std::vector<event_ptr> events = make_events(n);
  std::vector<event_ptr> buf;
  buf.reserve(n);
  Q q;
  for (auto _ : state) {
    for (size_t i = 0; i < n; ++i)
      q.push(level_of(i), events[i], static_cast<int64_t>(i));
    buf.clear();
    q.read(buf, n);
    q.ack(n / 2);
    q.nack();
    buf.clear();
    q.read(buf, n);
    benchmark::DoNotOptimize(buf.data());
    q.ack(buf.size());
  }
  state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(BM_PartialAckNack<AckByLevel>)
    ->RangeMultiplier(8)
    ->Range(1 << 10, 1 << 19)
    ->Name("MuxerLevels/PartialAckNack/AckByLevel");
BENCHMARK(BM_PartialAckNack<AckByReadOrder>)
    ->RangeMultiplier(8)
    ->Range(1 << 10, 1 << 19)
    ->Name("MuxerLevels/PartialAckNack/AckByReadOrder");

// ─── Scenario 4: single reads ────────────────────────────────────────────────
// The failover reads the events one by one, then acknowledges them.

template <typename Q>
static void BM_SingleReads(benchmark::State& state) {
  const size_t batch = static_cast<size_t>(state.range(0));
  std::vector<event_ptr> events = make_events(batch);
  Q q;
  for (auto _ : state) {
    for (size_t i = 0; i < batch; ++i)
      q.push(level_of(i), events[i], static_cast<int64_t>(i));
    for (size_t i = 0; i < batch; ++i) {
      event_ptr e = *q.read_next();
      benchmark::DoNotOptimize(e);
    }
    q.ack(batch);
  }
  state.SetItemsProcessed(state.iterations() * batch);
}

BENCHMARK(BM_SingleReads<AckByLevel>)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Name("MuxerLevels/SingleReads/AckByLevel");
BENCHMARK(BM_SingleReads<AckByReadOrder>)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Name("MuxerLevels/SingleReads/AckByReadOrder");
