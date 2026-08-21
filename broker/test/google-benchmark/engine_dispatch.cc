/**
 * Copyright 2025 Centreon
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
 * Benchmark: multiplexing::engine event dispatch.
 *
 * Models engine::publish() + engine::_send_to_subscribers() from
 * multiplexing/src/engine.cc, which is the fan-out point of Broker: every event
 * published by any input is handed to every subscribed muxer. A cbd connected
 * to 100 pollers has ~100 subscribed muxers (one per feeder, see
 * processing/feeder.cc:91), so what this fan-out costs per event is what Broker
 * pays at scale.
 *
 * As in old_muxer_queue.cc / muxer_queue.cc, the algorithms are *modelled* here
 * rather than linked from the real engine: the real one needs
 * config::applier::state, the stats center and a persistent_cache, none of
 * which would change between the variants we want to compare. Keeping the
 * models side by side in one binary lets Google Benchmark interleave them, so
 * the comparison does not carry the machine noise an A/B over two runs would.
 *
 * What is modelled, per event and per muxer:
 *  - engine.cc:429  one std::make_shared<std::deque<>> per batch, then a swap
 *  - engine.cc:434  one std::make_shared<callback_caller> per batch
 *  - engine.cc:446  one asio::post per muxer, capturing three shared_ptr
 *                   (kiew, mux, cb) — their refcounts are the contended part
 *  - muxer.cc:396   the muxer takes its own mutex and tests _write_filter
 *                   *inside* publish, i.e. after the post
 *  - engine.cc:368  ~callback_caller resets the flag and re-drains the queue
 *
 * Two strand variants sit alongside, because the atomic flag modelled above is
 * itself the replacement of an asio strand, dropped in 8bae7fd120:
 *  - Strand2022, a faithful model of what we shipped until then, blocking
 *    promise and per-event publish included;
 *  - StrandNext, today's algorithm with its drain loop moved onto a strand and
 *    nothing else changed, so that the cost of the strand can be read on its
 *    own rather than mixed with the two other differences of 2022.
 *
 * What is deliberately left out, being identical across variants: the stats
 * center updates, cache.publish(), the notification sink, and the retention
 * file a muxer writes to when its in-memory queue is full (that one would turn
 * the benchmark into a disk benchmark; queues are bounded here instead).
 *
 * Metrics reported per event: wall time, heap allocations, allocated bytes and
 * asio posts. Allocations are counted by the global operator new/delete
 * overrides below — cheaper and more precise than heaptrack for this purpose.
 */

#include <benchmark/benchmark.h>

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include <boost/asio.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <new>
#include <thread>
#include <vector>

namespace asio = boost::asio;

namespace {

/* ─── allocation counting ─────────────────────────────────────────────────────
 *
 * The overrides are global, so they also count the allocations of the other
 * benchmarks living in this binary. That is harmless: each benchmark reads the
 * counters itself and reports a delta.
 */

std::atomic<uint64_t> g_allocs{0};
std::atomic<uint64_t> g_alloc_bytes{0};
std::atomic<uint64_t> g_posts{0};

struct alloc_snapshot {
  uint64_t allocs;
  uint64_t bytes;
  uint64_t posts;

  static alloc_snapshot take() {
    return {g_allocs.load(std::memory_order_relaxed),
            g_alloc_bytes.load(std::memory_order_relaxed),
            g_posts.load(std::memory_order_relaxed)};
  }

  alloc_snapshot since() const {
    alloc_snapshot now = take();
    return {now.allocs - allocs, now.bytes - bytes, now.posts - posts};
  }
};

/**
 * @brief The event carried through the multiplexer.
 *
 * io::data is held by shared_ptr everywhere in Broker, and its type drives the
 * muxer write filter. Only those two properties matter to the fan-out, so the
 * payload is reduced to a type index.
 */
class fake_event {
  uint32_t _type;

 public:
  explicit fake_event(uint32_t type) : _type{type} {}
  uint32_t type() const { return _type; }
};

using event_ptr = std::shared_ptr<fake_event>;

/* The real muxer_filter is one uint64_t mask per event category
 * (multiplexing/muxer_filter.hh:45). A single 64 bit mask is enough here: what
 * we measure is whether the filter is tested before or after the post, not how
 * many categories it spans. */
using filter_mask = uint64_t;

constexpr filter_mask mask_of(uint32_t type) {
  return filter_mask{1} << (type & 63u);
}

/* A cbd fans out to two kinds of muxers, and the difference is the whole point
 * of the filter question:
 *  - the outputs (unified_sql, rrd) accept the monitoring flow;
 *  - the feeders towards pollers accept configuration and commands only, so
 *    they reject every event of that flow — after having been posted, today. */
constexpr filter_mask accept_all = ~filter_mask{0};
constexpr filter_mask accept_none_of_flow = 0xffff'ffff'0000'0000ull;

/* Types present in the modelled flow: service status, host status, log, check
 * result — all in the low half, hence rejected by accept_none_of_flow. */
constexpr uint32_t flow_types = 4;

/**
 * @brief Models multiplexing::muxer as seen from the engine.
 *
 * Only publish() is modelled: it takes the muxer's own mutex, walks the batch,
 * tests the write filter per event and stacks what it accepts. The queue is
 * bounded so that a slow consumer neither grows without end nor sends us to the
 * retention file.
 */
class fake_muxer {
  static constexpr size_t _queue_max = 4096;

  absl::Mutex _m;
  std::deque<event_ptr> _queue ABSL_GUARDED_BY(_m);
  const filter_mask _filter;
  std::atomic<uint64_t> _accepted{0};

 public:
  explicit fake_muxer(filter_mask filter) : _filter{filter} {}

  filter_mask filter() const { return _filter; }

  /* What the engine could test before posting, and does not today. */
  bool accepts_any(filter_mask batch) const { return (_filter & batch) != 0; }

  void publish(const std::deque<event_ptr>& batch) {
    absl::MutexLock lck(&_m);
    for (const auto& e : batch) {
      if (!(_filter & mask_of(e->type())))
        continue;
      if (_queue.size() >= _queue_max)
        _queue.pop_front();
      _queue.push_back(e);
      _accepted.fetch_add(1, std::memory_order_relaxed);
    }
  }

  /**
   * @brief The muxer as the strand era called it: one event at a time.
   *
   * muxer::publish(deque) did not exist then; engine.cc did
   * `for (auto& e : kiew) m->publish(e);`. The mutex is therefore taken once
   * per event and per muxer instead of once per batch, which is a cost of that
   * era rather than a cost of the strand — hence a separate entry point, so the
   * two can be told apart.
   */
  void publish_one(const event_ptr& e) {
    absl::MutexLock lck(&_m);
    if (!(_filter & mask_of(e->type())))
      return;
    if (_queue.size() >= _queue_max)
      _queue.pop_front();
    _queue.push_back(e);
    _accepted.fetch_add(1, std::memory_order_relaxed);
  }

  uint64_t accepted() const {
    return _accepted.load(std::memory_order_relaxed);
  }
};

/* ─── the current algorithm ───────────────────────────────────────────────────
 *
 * Faithful model of engine::publish + engine::_send_to_subscribers, including
 * the callback_caller whose destructor resets the flag and re-drains the queue.
 */

class engine_current {
 public:
  using completion = std::function<void()>;

 private:
  class callback_caller;

  absl::Mutex _kiew_m;
  std::deque<event_ptr> _kiew ABSL_GUARDED_BY(_kiew_m);
  /* weak_ptr as in engine.hh:83, so that the lock() of engine.cc:443 — one
   * refcount bump per muxer per batch — is part of what we measure. */
  std::vector<std::weak_ptr<fake_muxer>> _muxers ABSL_GUARDED_BY(_kiew_m);
  std::atomic_bool _sending{false};
  asio::io_context& _ctx;

  bool _send_to_subscribers(completion&& callback);

 public:
  explicit engine_current(asio::io_context& ctx) : _ctx{ctx} {}

  void subscribe(const std::shared_ptr<fake_muxer>& m) {
    absl::MutexLock lck(&_kiew_m);
    _muxers.push_back(m);
  }

  void publish(const event_ptr& e) {
    {
      absl::MutexLock lck(&_kiew_m);
      _kiew.push_back(e);
    }
    _send_to_subscribers(nullptr);
  }

  /**
   * @brief True once the queue is empty and no batch is in flight.
   *
   * _sending only goes back to false from ~callback_caller, that is after the
   * last posted lambda has run, so this covers the whole fan-out.
   */
  bool drained() {
    if (_sending.load(std::memory_order_acquire))
      return false;
    absl::MutexLock lck(&_kiew_m);
    return _kiew.empty();
  }
};

/**
 * @brief Model of multiplexing::detail::callback_caller.
 *
 * Held by shared_ptr and captured by every posted lambda, so the destructor
 * runs when the last muxer is done. It then resets the flag and, because
 * publish() gives up while a batch is in flight, re-drains whatever piled up
 * meanwhile.
 */
class engine_current::callback_caller {
  completion _callback;
  engine_current* _parent;

 public:
  callback_caller(completion&& callback, engine_current* parent)
      : _callback{std::move(callback)}, _parent{parent} {}

  ~callback_caller() {
    bool expected = true;
    if (_parent->_sending.compare_exchange_strong(expected, false)) {
      if (_callback)
        _callback();
      bool pending;
      {
        absl::MutexLock lck(&_parent->_kiew_m);
        pending = !_parent->_muxers.empty() && !_parent->_kiew.empty();
      }
      if (pending)
        _parent->_send_to_subscribers(nullptr);
    }
  }
};

bool engine_current::_send_to_subscribers(completion&& callback) {
  bool expected = false;
  if (!_sending.compare_exchange_strong(expected, true))
    return false;

  std::shared_ptr<std::deque<event_ptr>> kiew;
  std::shared_ptr<callback_caller> cb;
  bool retval = false;
  {
    absl::MutexLock lck(&_kiew_m);
    if (_muxers.empty() || _kiew.empty()) {
      bool expected_true = true;
      _sending.compare_exchange_strong(expected_true, false);
      return false;
    }

    kiew = std::make_shared<std::deque<event_ptr>>();
    std::swap(_kiew, *kiew);
    cb = std::make_shared<callback_caller>(std::move(callback), this);

    for (auto& mux : _muxers) {
      std::shared_ptr<fake_muxer> m = mux.lock();
      if (m) {
        retval = true;
        g_posts.fetch_add(1, std::memory_order_relaxed);
        asio::post(_ctx, [kiew, m, cb]() { m->publish(*kiew); });
      }
    }
  }
  return retval;
}

/* ─── the proposed algorithm ──────────────────────────────────────────────────
 *
 * Same contract, three changes:
 *
 *  1. The batch is a *member*, reused from one batch to the next instead of a
 *     std::make_shared<std::deque<>> per batch. This is legitimate because a
 *     single batch is in flight at a time: the drain loop only comes back for a
 *     new batch when the last worker of the previous one has finished, so no
 *     reader survives. clear() on a libstdc++ deque keeps one node, hence the
 *     map and the node survive too.
 *  2. The write filter is tested *before* posting. The mask is remembered
 *     engine-side at subscribe time, so deciding does not even require locking
 *     the weak_ptr — a cbd whose 98 feeders reject the flow posts to the 2
 *     muxers that want it instead of to all 100.
 *  3. No callback_caller, hence no work in a destructor and no recursion: the
 *     loop is explicit, and whoever finishes last carries it on. Lambdas
 * capture a raw muxer* whose lifetime is held by the drainer's own snapshot, so
 * the three shared_ptr copies per muxer per batch are gone.
 *
 * The completion callback of start/stop/unsubscribe is not modelled: in the
 * real engine it would become a _kiew_m.Await(!_draining && _kiew.empty()),
 * which costs nothing on the publish path measured here.
 */

/**
 * @brief Does the drainer publish to one muxer itself, or post them all?
 *
 * Doing one share itself saves an asio round trip, which is what makes the
 * spaced profile fast — but it runs muxer::publish, and therefore a possible
 * retention write, in the calling thread. Posting everything keeps the producer
 * clean at the cost of that round trip. `small_batch` only takes a share when
 * the batch is short, which is exactly when the round trip dominates.
 */
enum class self_share { always, never, small_batch, first_turn };

template <self_share Policy>
class engine_next_impl {
  /* Above this batch size, `small_batch` delegates instead of publishing
   * itself. First guess, to be calibrated: spaced batches hold 1 event, bursts
   * held ~63 in the runs so far. */
  static constexpr size_t _small_batch_max = 8;

  struct subscriber {
    std::weak_ptr<fake_muxer> mux;
    filter_mask filter;
  };

  absl::Mutex _kiew_m;
  std::deque<event_ptr> _kiew ABSL_GUARDED_BY(_kiew_m);
  std::vector<subscriber> _muxers ABSL_GUARDED_BY(_kiew_m);
  /* Single role: is a drain loop running. Replaces _sending_to_subscribers,
   * which also served as a return value and as a completion signal. */
  bool _draining ABSL_GUARDED_BY(_kiew_m) = false;

  /* Batches taken since this drain sequence started, reset when the queue runs
   * dry. Reaching a second turn means the producers kept going while we were
   * distributing, which is the only honest signal of a burst: unlike a batch
   * size threshold, it does not depend on how fast we ourselves drain. */
  size_t _turns ABSL_GUARDED_BY(_kiew_m) = 0;

  /* Owned by the drain loop, which is unique — hence no mutex. Read by the
   * workers only between the arming of _pending and its fall back to zero. */
  std::deque<event_ptr> _batch;
  std::vector<std::shared_ptr<fake_muxer>> _hold;
  std::vector<fake_muxer*> _targets;
  std::atomic<size_t> _pending{0};

  asio::io_context& _ctx;

  /**
   * @brief OR of the type masks present in the batch, so that each muxer is
   * decided with one AND instead of a walk over the batch.
   */
  static filter_mask _batch_mask(const std::deque<event_ptr>& batch) {
    filter_mask m = 0;
    for (const auto& e : batch)
      m |= mask_of(e->type());
    return m;
  }

  /**
   * @brief Decrement the in-flight counter.
   *
   * @return true if the caller was the last one, and so owns the loop.
   */
  bool _one_done() {
    return _pending.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  /**
   * @brief Take batch after batch until the queue runs dry.
   *
   * Runs in the producer's thread on the first turn, then in whichever worker
   * finished last — there is only ever one instance of this loop, which is what
   * lets _batch, _hold and _targets be plain members.
   */
  void _drain() {
    for (;;) {
      size_t turn;
      {
        absl::MutexLock lck(&_kiew_m);
        if (_kiew.empty()) {
          _draining = false;
          _turns = 0;
          return;
        }
        turn = ++_turns;
        _batch.clear();
        std::swap(_kiew, _batch);

        _hold.clear();
        _targets.clear();
        const filter_mask wanted = _batch_mask(_batch);
        for (const auto& s : _muxers) {
          if (!(s.filter & wanted))
            continue;
          if (auto m = s.mux.lock()) {
            _hold.push_back(std::move(m));
            _targets.push_back(_hold.back().get());
          }
        }
      }

      /* Nobody wants this batch: dropping it is what the current code achieves
       * too, by having every muxer reject every event of it. */
      if (_targets.empty())
        continue;

      bool take_share;
      switch (Policy) {
        case self_share::always:
          take_share = true;
          break;
        case self_share::never:
          take_share = false;
          break;
        case self_share::first_turn:
          /* Synchronous on the first batch of a sequence, delegated as soon as
           * the flow proves continuous — so the producer and the pool pipeline
           * under load, and no round trip is paid when there is nothing to
           * overlap. */
          take_share = turn == 1;
          break;
        default:
          take_share = _batch.size() <= _small_batch_max;
          break;
      }

      /* Armed before any post, otherwise a worker could reach zero while we are
       * still posting and two loops would run at once. */
      _pending.store(_targets.size(), std::memory_order_release);
      for (size_t i = take_share ? 1 : 0; i < _targets.size(); ++i) {
        g_posts.fetch_add(1, std::memory_order_relaxed);
        asio::post(_ctx, [this, m = _targets[i]]() {
          m->publish(_batch);
          if (_one_done())
            _drain();
        });
      }

      if (!take_share)
        return;  // the last worker owns the loop

      /* The drainer takes one share itself, as engine.cc:437-441 claims to do —
       * the current code posts every muxer, including the first. */
      _targets[0]->publish(_batch);
      if (!_one_done())
        return;  // a worker is still running and will carry the loop on
    }
  }

 public:
  explicit engine_next_impl(asio::io_context& ctx) : _ctx{ctx} {}

  void subscribe(const std::shared_ptr<fake_muxer>& m) {
    absl::MutexLock lck(&_kiew_m);
    _muxers.push_back({m, m->filter()});
  }

  void publish(const event_ptr& e) {
    bool mine = false;
    {
      absl::MutexLock lck(&_kiew_m);
      _kiew.push_back(e);
      if (!_draining) {
        _draining = true;
        mine = true;
      }
    }
    /* Whoever finds a drain already running just leaves: that drainer will see
     * this event before giving up. No catch-up pass needed. */
    if (mine)
      _drain();
  }

  bool drained() {
    absl::MutexLock lck(&_kiew_m);
    return !_draining && _kiew.empty();
  }
};

/* The drainer publishes to one muxer itself: fastest, but muxer work —
 * including a retention write — lands in the producer's thread. */
using engine_next = engine_next_impl<self_share::always>;

/* The drainer only posts: no muxer work in the producer's thread, at the cost
 * of one asio round trip per batch. */
using engine_next_post_all = engine_next_impl<self_share::never>;

/* Take a share only on short batches, where the round trip dominates.
 *
 * Measured 2026-08-17: useless as it stands. Taking a share makes the drain so
 * fast that the queue never reaches the threshold, so the policy validates
 * itself and behaves exactly like `always` — including its regression on the
 * single muxer burst. Kept as the record of a dead end. */
using engine_next_adaptive = engine_next_impl<self_share::small_batch>;

/* Synchronous on the first batch of a sequence, delegated afterwards. */
using engine_next_first_turn = engine_next_impl<self_share::first_turn>;

/* ─── the strand algorithm, as it stood until December 2022 ───────────────────
 *
 * Model of engine::_send_to_subscribers before 8bae7fd120 ("Mon 16050 periodic
 * log flush"), the commit that dropped the strand for the atomic flag and the
 * callback_caller `Current` models above.
 *
 * Included to answer a question, and NOT as a candidate — see the deadlock note
 * below. What did removing the strand actually buy? Three things differ from
 * `Current` at once, and only the first is about the strand itself:
 *
 *  1. Serialisation is asio's, not a std::atomic_bool. `publish` posts and
 *     returns, so nothing of the fan-out runs in the producer's thread.
 *  2. The strand handler blocks on promise.get_future().wait() until every
 *     muxer is done — a pool thread parked for the whole fan-out.
 *  3. Muxers are fed event by event, so each takes its mutex once per event
 *     rather than once per batch.
 *
 * So a slow result here does not convict the strand: (2) and (3) would be just
 * as slow without it, which is what engine_strand_next below is for.
 *
 * (2) is also why this variant can never come back as it stands. The handler
 * runs on a pool thread and waits for lambdas posted to the same io_context, so
 * it needs a second thread to make progress — and pool_size is configurable
 * (broker JSON key, and cbd's --pool_size, main.cc:272), with no floor in
 * pool::_set_pool_size(): `pool_size: 1` freezes Broker on its first published
 * event. Worse, the real subscribe/unsubscribe/stop of that era blocked their
 * *caller* on the strand too, so a caller that is itself a pool thread
 * deadlocks at pool_size 2 — the requirement was never "2 threads", it was
 * "2 + however many blocking callers there are at once", against a default of
 * max(hardware_concurrency, 3).
 */

class engine_strand_2022 {
  absl::Mutex _kiew_m;
  std::deque<event_ptr> _kiew ABSL_GUARDED_BY(_kiew_m);

  /* shared_ptr, not weak_ptr, and no mutex annotation: _muxers was confined to
   * the strand back then — subscribe and unsubscribe posted to it and blocked
   * on a promise — so the strand was both the lock and the ordering. That is
   * the part of the design that was genuinely clean. */
  std::vector<std::shared_ptr<fake_muxer>> _muxers;

  bool _sending ABSL_GUARDED_BY(_kiew_m) = false;

  asio::io_context& _ctx;
  asio::strand<asio::io_context::executor_type> _strand;

  void _send_to_subscribers() {
    std::deque<event_ptr> kiew;
    {
      absl::MutexLock lck(&_kiew_m);
      if (_kiew.empty()) {
        _sending = false;
        return;
      }
      std::swap(_kiew, kiew);
    }

    std::atomic<int> count{static_cast<int>(_muxers.size()) - 1};
    if (count >= 0) {
      std::promise<void> promise;

      /* kiew, count and promise are captured *by reference* — locals of a frame
       * the workers outlive on paper. It is only safe because of the wait
       * below, which is the real reason that wait existed: not ordering, but
       * lifetime. Replacing it with a completion counter, as
       * engine_strand_next does, means the batch has to be owned elsewhere. */
      auto it_last = --_muxers.end();
      for (auto it = _muxers.begin(); it != it_last; ++it) {
        g_posts.fetch_add(1, std::memory_order_relaxed);
        asio::post(_ctx, [&kiew, m = *it, &count, &promise] {
          for (auto& e : kiew)
            m->publish_one(e);
          if (count.fetch_sub(1, std::memory_order_relaxed) == 0)
            promise.set_value();
        });
      }

      /* The last muxer is served by this thread — which is a pool thread, since
       * we are inside a strand handler. */
      auto m = *it_last;
      for (auto& e : kiew)
        m->publish_one(e);
      if (count.fetch_sub(1, std::memory_order_relaxed) == 0)
        promise.set_value();

      promise.get_future().wait();
    }

    /* Unconditional re-post: the loop stops on the next pass, when the queue is
     * found empty. One empty strand hop per drain sequence, no more. */
    g_posts.fetch_add(1, std::memory_order_relaxed);
    asio::post(_strand, [this] { _send_to_subscribers(); });
  }

 public:
  explicit engine_strand_2022(asio::io_context& ctx)
      : _ctx{ctx}, _strand{asio::make_strand(ctx)} {}

  /* Subscription happens before the measured run, so the strand hop the real
   * one paid is not modelled. */
  void subscribe(const std::shared_ptr<fake_muxer>& m) { _muxers.push_back(m); }

  void publish(const event_ptr& e) {
    bool post_it = false;
    {
      absl::MutexLock lck(&_kiew_m);
      _kiew.push_back(e);
      if (!_sending) {
        _sending = true;
        post_it = true;
      }
    }
    if (post_it) {
      g_posts.fetch_add(1, std::memory_order_relaxed);
      asio::post(_strand, [this] { _send_to_subscribers(); });
    }
  }

  /**
   * @brief True once the queue is empty and nothing is in flight.
   *
   * _sending only falls back to false inside a strand handler that found the
   * queue empty, and that handler only runs after the previous one waited on
   * its promise — so this covers the whole fan-out.
   */
  bool drained() {
    absl::MutexLock lck(&_kiew_m);
    return !_sending && _kiew.empty();
  }
};

/* ─── the strand, rewritten ───────────────────────────────────────────────────
 *
 * What a strand-based engine looks like once the two costs that were *not* the
 * strand are removed: batch publish instead of per-event, and a completion
 * counter instead of a pool thread parked on a promise.
 *
 * The result is deliberately engine_next_post_all with one change and one only:
 * the drain loop runs as a strand handler instead of running inline in the
 * thread that published. Everything else — the reused member batch, the mask
 * pre-filter, the raw muxer* held alive by _hold, the last-worker-carries-on
 * baton — is the same code. So the delta between the two entries is the price
 * of the strand hop, and nothing else.
 *
 * What the strand gives back for that hop:
 *  - _drain never runs in the producer's thread, so the batch scan and the
 *    target selection leave Engine's check loop alone;
 *  - serialisation is asio's rather than resting on _draining, and _batch,
 *    _hold and _targets become strand-confined instead of merely being
 *    documented as owned by whoever holds the baton.
 *
 * _draining survives even so: without it, every publish would post a drain to
 * the strand, one hop per event on the spaced profile. It stops being what
 * guarantees exclusion and becomes what avoids a redundant post.
 *
 * Note what is absent: no promise, no wait, nowhere. The baton is passed by
 * _pending, so no handler ever blocks on another handler of the same
 * io_context, and the whole thing still makes progress with a single pool
 * thread. That is the property engine_strand_2022 lacked and the one that has
 * to be preserved whatever we choose — it is a correctness property, not a
 * performance one.
 */

class engine_strand_next {
  struct subscriber {
    std::weak_ptr<fake_muxer> mux;
    filter_mask filter;
  };

  absl::Mutex _kiew_m;
  std::deque<event_ptr> _kiew ABSL_GUARDED_BY(_kiew_m);
  std::vector<subscriber> _muxers ABSL_GUARDED_BY(_kiew_m);
  bool _draining ABSL_GUARDED_BY(_kiew_m) = false;

  /* Strand-confined: written by _drain only, read by the workers between the
   * arming of _pending and its fall back to zero. */
  std::deque<event_ptr> _batch;
  std::vector<std::shared_ptr<fake_muxer>> _hold;
  std::vector<fake_muxer*> _targets;
  std::atomic<size_t> _pending{0};

  asio::io_context& _ctx;
  asio::strand<asio::io_context::executor_type> _strand;

  static filter_mask _batch_mask(const std::deque<event_ptr>& batch) {
    filter_mask m = 0;
    for (const auto& e : batch)
      m |= mask_of(e->type());
    return m;
  }

  bool _one_done() {
    return _pending.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  void _post_drain() {
    g_posts.fetch_add(1, std::memory_order_relaxed);
    asio::post(_strand, [this] { _drain(); });
  }

  /**
   * @brief Take one batch and hand it out. Always runs on the strand.
   *
   * Unlike engine_next::_drain this does not loop over batches, except when a
   * batch has no taker: once the workers are posted it returns, and the last of
   * them posts the next drain back to the strand. The loop is still there — it
   * just goes through asio, which is what keeps it off the producer's thread.
   */
  void _drain() {
    for (;;) {
      {
        absl::MutexLock lck(&_kiew_m);
        if (_kiew.empty()) {
          _draining = false;
          return;
        }
        _batch.clear();
        std::swap(_kiew, _batch);

        _hold.clear();
        _targets.clear();
        const filter_mask wanted = _batch_mask(_batch);
        for (const auto& s : _muxers) {
          if (!(s.filter & wanted))
            continue;
          if (auto m = s.mux.lock()) {
            _hold.push_back(std::move(m));
            _targets.push_back(_hold.back().get());
          }
        }
      }

      /* Nobody wants this batch, so there is no worker to hand the baton to:
       * carry on here rather than posting to ourselves. */
      if (_targets.empty())
        continue;

      _pending.store(_targets.size(), std::memory_order_release);
      for (fake_muxer* m : _targets) {
        g_posts.fetch_add(1, std::memory_order_relaxed);
        asio::post(_ctx, [this, m] {
          m->publish(_batch);
          if (_one_done())
            _post_drain();
        });
      }
      return;
    }
  }

 public:
  explicit engine_strand_next(asio::io_context& ctx)
      : _ctx{ctx}, _strand{asio::make_strand(ctx)} {}

  void subscribe(const std::shared_ptr<fake_muxer>& m) {
    absl::MutexLock lck(&_kiew_m);
    _muxers.push_back({m, m->filter()});
  }

  void publish(const event_ptr& e) {
    bool mine = false;
    {
      absl::MutexLock lck(&_kiew_m);
      _kiew.push_back(e);
      if (!_draining) {
        _draining = true;
        mine = true;
      }
    }
    if (mine)
      _post_drain();
  }

  bool drained() {
    absl::MutexLock lck(&_kiew_m);
    return !_draining && _kiew.empty();
  }
};

/* ─── harness ───────────────────────────────────────────────────────────────*/

/**
 * @brief The asio pool the posted lambdas run on.
 *
 * Modelled on common::pool: an io_context kept alive by a work guard and served
 * by a fixed set of threads. One instance is shared by every benchmark, so its
 * startup cost never lands inside a measurement.
 */
class bench_pool {
  asio::io_context _ctx;
  asio::executor_work_guard<asio::io_context::executor_type> _guard;
  std::vector<std::thread> _threads;

 public:
  bench_pool() : _guard{asio::make_work_guard(_ctx)} {
    /* At least two threads, and that floor is what keeps engine_strand_2022
     * from hanging the whole binary: its strand handler parks on a promise
     * while the muxers it posted run on the *other* threads. One thread and it
     * deadlocks — not a harness artefact but the real thing, since cbd's
     * pool_size is settable to 1 with no floor. The floor is spelled out here
     * rather than inherited from hardware_concurrency so that running this
     * benchmark on a single-core machine does not look like a benchmark bug. */
    uint32_t n =
        std::max(2u, std::min(8u, std::thread::hardware_concurrency()));
    for (uint32_t i = 0; i < n; ++i)
      _threads.emplace_back([this] { _ctx.run(); });
  }

  ~bench_pool() {
    _guard.reset();
    _ctx.stop();
    for (auto& t : _threads)
      t.join();
  }

  asio::io_context& ctx() { return _ctx; }

  static bench_pool& instance() {
    static bench_pool pool;
    return pool;
  }
};

/**
 * @brief Build an engine with `muxers` subscribers, `accepting` of which accept
 * the modelled flow.
 *
 * The others stand for the feeders towards pollers: subscribed, posted to, and
 * rejecting every event once there.
 */
template <typename Engine>
std::shared_ptr<Engine> make_engine(
    size_t muxers,
    size_t accepting,
    std::vector<std::shared_ptr<fake_muxer>>& out) {
  auto e = std::make_shared<Engine>(bench_pool::instance().ctx());

  /* Engines and muxers are kept alive for the whole run, on purpose.
   *
   * A drain is observably finished slightly before its last worker has
   * returned: the flag is cleared, or the mutex released, a few instructions
   * before the lambda actually ends. Destroying the engine in that window
   * unlocks a mutex that no longer exists, which absl reports as "thread should
   * hold at least a read lock on Mutex" from UnlockSlow(). The real engine
   * cannot hit this — its callback_caller holds a shared_ptr<engine>
   * (engine.cc:357) — so this is a harness concern only, and keeping every
   * engine alive settles it without adding a refcount to the path being
   * measured. */
  static std::vector<std::shared_ptr<void>> keep_alive;
  keep_alive.push_back(e);

  for (size_t i = 0; i < muxers; ++i) {
    auto m = std::make_shared<fake_muxer>(i < accepting ? accept_all
                                                        : accept_none_of_flow);
    out.push_back(m);
    keep_alive.push_back(m);
    e->subscribe(m);
  }
  return e;
}

/**
 * @brief Pre-built events, reused across iterations.
 *
 * Distinct objects on purpose: publishing one shared_ptr over and over would
 * pile every muxer onto a single refcount and invent a contention the real flow
 * does not have.
 */
std::vector<event_ptr> make_events(size_t n) {
  std::vector<event_ptr> events;
  events.reserve(n);
  for (size_t i = 0; i < n; ++i)
    events.push_back(std::make_shared<fake_event>(i % flow_types));
  return events;
}

template <typename Engine>
void wait_drained(Engine& e) {
  while (!e.drained())
    std::this_thread::yield();
}

/* ─── scenario A: fan-out cost per event ──────────────────────────────────────
 *
 * One producer, two profiles, because the per-batch machinery is amortised over
 * whatever happens to be queued and the two extremes differ by an order of
 * magnitude:
 *
 *  - Spaced: the queue is drained between two publishes, so every event opens
 *    its own batch. This is the worst case, and the one comparable to what
 *    heaptrack measured on cbmod (1.585 batches per check, i.e. barely any
 *    batching): it should report the 4 allocations per batch of the current
 *    algorithm, which is how we check the model against the real thing.
 *  - Burst: 512 events are published back to back, so batches form while one is
 *    in flight. This is the loaded cbd profile, where the machinery is
 * amortised and the per-muxer fan-out dominates instead.
 *
 * range(0) = number of subscribed muxers, range(1) = how many accept the flow.
 */
template <typename Engine, bool DrainEachEvent>
void BM_FanOut(benchmark::State& state) {
  const size_t muxers = state.range(0);
  const size_t accepting = std::min<size_t>(state.range(1), muxers);
  const size_t events_per_iteration = DrainEachEvent ? 32 : 512;

  std::vector<std::shared_ptr<fake_muxer>> muxer_list;
  auto engine = make_engine<Engine>(muxers, accepting, muxer_list);
  auto events = make_events(events_per_iteration);

  /* Warm up: the first batch grows the muxer queues and asio's per-thread
   * handler pools, which would otherwise be charged to the first measured
   * iteration. */
  for (const auto& e : events)
    engine->publish(e);
  wait_drained(*engine);

  const alloc_snapshot before = alloc_snapshot::take();
  uint64_t published = 0;
  for (auto _ : state) {
    for (const auto& e : events) {
      engine->publish(e);
      if (DrainEachEvent)
        wait_drained(*engine);
    }
    if (!DrainEachEvent)
      wait_drained(*engine);
    published += events.size();
  }
  const alloc_snapshot delta = before.since();

  state.SetItemsProcessed(published);
  state.counters["allocs/event"] =
      benchmark::Counter(static_cast<double>(delta.allocs) / published);
  state.counters["bytes/event"] =
      benchmark::Counter(static_cast<double>(delta.bytes) / published);
  state.counters["posts/event"] =
      benchmark::Counter(static_cast<double>(delta.posts) / published);
}

BENCHMARK(BM_FanOut<engine_current, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Name("Engine/FanOutSpaced/Current")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Name("Engine/FanOutSpaced/Next")
    ->UseRealTime();

/* The two self-share policies, on the cases that decide between them: the
 * spaced profile where taking a share wins by an order of magnitude, and the
 * single muxer burst where it loses — that one being cbmod's shape. */
BENCHMARK(BM_FanOut<engine_next_post_all, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Name("Engine/FanOutSpaced/NextPostAll")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next_adaptive, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({100, 2})
    ->Name("Engine/FanOutSpaced/NextAdaptive")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next_first_turn, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({100, 2})
    ->Name("Engine/FanOutSpaced/NextFirstTurn")
    ->UseRealTime();

/* The two strand entries. Strand2022 is what we shipped until December 2022,
 * StrandNext is engine_next_post_all with the drain moved onto a strand — so
 * StrandNext against NextPostAll is the price of the strand alone, while
 * Strand2022 against Current is what the 2022 rewrite actually bought. */
BENCHMARK(BM_FanOut<engine_strand_2022, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Name("Engine/FanOutSpaced/Strand2022")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_strand_next, true>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Name("Engine/FanOutSpaced/StrandNext")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_current, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/Current")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/Next")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next_post_all, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/NextPostAll")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next_adaptive, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({100, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/NextAdaptive")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_next_first_turn, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({100, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/NextFirstTurn")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_strand_2022, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/Strand2022")
    ->UseRealTime();

BENCHMARK(BM_FanOut<engine_strand_next, false>)
    ->ArgNames({"muxers", "accepting"})
    ->Args({1, 1})
    ->Args({10, 2})
    ->Args({100, 2})
    ->Args({200, 2})
    ->Args({100, 100})
    ->Name("Engine/FanOutBurst/StrandNext")
    ->UseRealTime();

/* ─── scenario B: producer contention ─────────────────────────────────────────
 *
 * A cbd with 100 pollers has as many inbound flows calling publish()
 * concurrently, all serialised on _kiew_m. Each iteration releases `producers`
 * threads which publish their share, then waits for the whole batch to be
 * distributed, so the measure covers both the contended enqueue and the
 * fan-out.
 *
 * range(0) = number of muxers, range(1) = number of producer threads.
 */
template <typename Engine>
void BM_Contention(benchmark::State& state) {
  const size_t muxers = state.range(0);
  const size_t producers = state.range(1);
  constexpr size_t events_per_producer = 256;

  std::vector<std::shared_ptr<fake_muxer>> muxer_list;
  auto engine = make_engine<Engine>(muxers, 2, muxer_list);
  auto events = make_events(events_per_producer);

  for (const auto& e : events)
    engine->publish(e);
  wait_drained(*engine);

  const alloc_snapshot before = alloc_snapshot::take();
  uint64_t published = 0;
  for (auto _ : state) {
    std::vector<std::thread> threads;
    threads.reserve(producers);
    for (size_t p = 0; p < producers; ++p)
      threads.emplace_back([&engine, &events] {
        for (const auto& e : events)
          engine->publish(e);
      });
    for (auto& t : threads)
      t.join();
    wait_drained(*engine);
    published += producers * events.size();
  }
  const alloc_snapshot delta = before.since();

  state.SetItemsProcessed(published);
  /* The threads themselves allocate (a handful per thread), which is why this
   * scenario reports allocations only as an order of magnitude; scenario A is
   * the one to trust on that count. */
  state.counters["allocs/event"] =
      benchmark::Counter(static_cast<double>(delta.allocs) / published);
  state.counters["posts/event"] =
      benchmark::Counter(static_cast<double>(delta.posts) / published);
}

BENCHMARK(BM_Contention<engine_current>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 1})
    ->Args({100, 4})
    ->Args({100, 16})
    ->Name("Engine/Contention/Current")
    ->UseRealTime();

BENCHMARK(BM_Contention<engine_next>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 1})
    ->Args({100, 4})
    ->Args({100, 16})
    ->Name("Engine/Contention/Next")
    ->UseRealTime();

BENCHMARK(BM_Contention<engine_next_post_all>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 16})
    ->Name("Engine/Contention/NextPostAll")
    ->UseRealTime();

BENCHMARK(BM_Contention<engine_next_adaptive>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 16})
    ->Name("Engine/Contention/NextAdaptive")
    ->UseRealTime();

BENCHMARK(BM_Contention<engine_next_first_turn>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 1})
    ->Args({100, 16})
    ->Name("Engine/Contention/NextFirstTurn")
    ->UseRealTime();

/* The scenario where a strand should show its worth, if it has any: 16 threads
 * publishing at once. Inline drains make one of those producers do the batch
 * scan and the posting while the fifteen others queue behind _kiew_m; a strand
 * hands that work to the pool and lets all sixteen return to their own job. */
BENCHMARK(BM_Contention<engine_strand_2022>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 1})
    ->Args({100, 4})
    ->Args({100, 16})
    ->Name("Engine/Contention/Strand2022")
    ->UseRealTime();

BENCHMARK(BM_Contention<engine_strand_next>)
    ->ArgNames({"muxers", "producers"})
    ->Args({100, 1})
    ->Args({100, 4})
    ->Args({100, 16})
    ->Name("Engine/Contention/StrandNext")
    ->UseRealTime();

}  // namespace

/* ─── global operator new / delete ────────────────────────────────────────────
 *
 * Outside the anonymous namespace: these have to be the program-wide
 * replacements. malloc/free are used underneath so that every form pairs up.
 */

void* operator new(size_t size) {
  g_allocs.fetch_add(1, std::memory_order_relaxed);
  g_alloc_bytes.fetch_add(size, std::memory_order_relaxed);
  void* p = std::malloc(size);
  if (!p)
    throw std::bad_alloc();
  return p;
}

void* operator new[](size_t size) {
  return operator new(size);
}

void* operator new(size_t size, std::align_val_t align) {
  g_allocs.fetch_add(1, std::memory_order_relaxed);
  g_alloc_bytes.fetch_add(size, std::memory_order_relaxed);
  size_t a = static_cast<size_t>(align);
  /* aligned_alloc requires a size multiple of the alignment. */
  void* p = std::aligned_alloc(a, ((size + a - 1) / a) * a);
  if (!p)
    throw std::bad_alloc();
  return p;
}

void* operator new[](size_t size, std::align_val_t align) {
  return operator new(size, align);
}

void operator delete(void* p) noexcept {
  std::free(p);
}

void operator delete[](void* p) noexcept {
  std::free(p);
}

void operator delete(void* p, size_t) noexcept {
  std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
  std::free(p);
}

void operator delete(void* p, std::align_val_t) noexcept {
  std::free(p);
}

void operator delete[](void* p, std::align_val_t) noexcept {
  std::free(p);
}

void operator delete(void* p, size_t, std::align_val_t) noexcept {
  std::free(p);
}

void operator delete[](void* p, size_t, std::align_val_t) noexcept {
  std::free(p);
}
