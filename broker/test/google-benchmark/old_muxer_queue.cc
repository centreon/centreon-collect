/**
 * Benchmark: std::list+iterator vs std::deque+index for the muxer event queue.
 *
 * Models the real muxer operations from multiplexing/src/muxer.cc:
 *  - _push_to_queue: push_back + update _pos when queue was exhausted
 *  - read():        sequential scan from _pos to end
 *  - ack_events():  pop_front N elements
 *  - _update_stats: std::distance(begin, _pos)  ← O(N) on list, O(1) on deque
 *
 * Each entry is pair<int64_t, shared_ptr<int>> simulating
 * pair<int64_t, shared_ptr<io::data>> planned for T1.
 */

#include <benchmark/benchmark.h>

#include <deque>
#include <list>
#include <memory>
#include <vector>

static std::shared_ptr<int> make_entry(int64_t ts) {
    return std::make_shared<int>(42);
}

// ─── std::list + iterator (_pos) ─────────────────────────────────────────────

struct ListQueue {
    std::list<std::shared_ptr<int>> events;
    std::list<std::shared_ptr<int>>::iterator pos{events.end()};

    void push(std::shared_ptr<int> e) {
        bool exhausted = (pos == events.end());
        events.push_back(std::move(e));
        if (exhausted)
            pos = std::prev(events.end());  // mirrors _push_to_queue line 720
    }

    // mirrors muxer::read() template
    void read(std::vector<std::shared_ptr<int>>& out, size_t max) {
        while (pos != events.end() && out.size() < max) {
            out.push_back(*pos);
            ++pos;
        }
    }

    // mirrors ack_events(count)
    void ack(size_t count) {
        for (size_t i = 0; i < count && events.begin() != pos; ++i)
            events.pop_front();
    }

    // mirrors _update_stats: std::distance(begin, _pos) — O(N) on list
    size_t unacked_count() const {
        return static_cast<size_t>(std::distance(events.cbegin(),
                                                  std::list<std::shared_ptr<int>>::const_iterator(pos)));
    }
};

// ─── std::deque + size_t index ───────────────────────────────────────────────

struct DequeQueue {
    std::deque<std::shared_ptr<int>> events;
    size_t read_pos{0};

    void push(std::shared_ptr<int> e) {
        events.push_back(std::move(e));
        // read_pos naturally points to the new element when queue was exhausted:
        // if read_pos == old_size, it equals new_size-1 after push — no update needed.
    }

    void read(std::vector<std::shared_ptr<int>>& out, size_t max) {
        while (read_pos < events.size() && out.size() < max) {
            out.push_back(events[read_pos]);
            ++read_pos;
        }
    }

    void ack(size_t count) {
        for (size_t i = 0; i < count && read_pos > 0; ++i) {
            events.pop_front();
            --read_pos;
        }
    }

    // O(1) — just read the index
    size_t unacked_count() const { return read_pos; }
};

// ─── Scenario 1: steady-state (small queue, continuous push/read/ack) ────────
// Simulates normal operation: a few hundred events in flight at any time.

template <typename Q>
static void BM_SteadyState(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));
    int64_t ts = 0;
    for (auto _ : state) {
        Q q;
        std::vector<std::shared_ptr<int>> buf;
        buf.reserve(batch);

        for (size_t round = 0; round < 8; ++round) {
            // push a batch
            for (size_t i = 0; i < batch; ++i)
                q.push(make_entry(ts++));

            // read all
            buf.clear();
            q.read(buf, batch * 8);
            benchmark::DoNotOptimize(buf.data());

            // stats (O(N) on list, O(1) on deque)
            benchmark::DoNotOptimize(q.unacked_count());

            // ack all
            q.ack(buf.size());
        }
    }
    state.SetItemsProcessed(state.iterations() * 8 * batch);
}

BENCHMARK(BM_SteadyState<ListQueue>)->RangeMultiplier(4)->Range(64, 4096)
    ->Name("OldMuxer/SteadyState/List");
BENCHMARK(BM_SteadyState<DequeQueue>)->RangeMultiplier(4)->Range(64, 4096)
    ->Name("OldMuxer/SteadyState/Deque");

// ─── Scenario 2: backlog drain ────────────────────────────────────────────────
// Simulates reconnection after long absence: large pre-filled queue, one
// sequential read pass, then ack everything.

template <typename Q>
static void BM_BacklogDrain(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        Q q;

        // pre-fill (all events "old": secondary queue in the real design)
        for (size_t i = 0; i < N; ++i)
            q.push(make_entry(static_cast<int64_t>(i)));

        // single sequential read pass
        std::vector<std::shared_ptr<int>> buf;
        buf.reserve(N);
        q.read(buf, N);
        benchmark::DoNotOptimize(buf.data());

        // stats
        benchmark::DoNotOptimize(q.unacked_count());

        // ack everything
        q.ack(buf.size());
    }
    state.SetItemsProcessed(state.iterations() * N);
}

BENCHMARK(BM_BacklogDrain<ListQueue>)->RangeMultiplier(4)->Range(256, 1 << 17)
    ->Name("OldMuxer/BacklogDrain/List");
BENCHMARK(BM_BacklogDrain<DequeQueue>)->RangeMultiplier(4)->Range(256, 1 << 17)
    ->Name("OldMuxer/BacklogDrain/Deque");

// ─── Scenario 3: stats overhead (distance vs index) ──────────────────────────
// Isolates the O(N) std::distance cost on list vs O(1) index on deque.
// _update_stats() is called ~every second; with large queues this matters.

template <typename Q>
static void BM_StatsOnly(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    Q q;
    for (size_t i = 0; i < N; ++i)
        q.push(make_entry(static_cast<int64_t>(i)));
    // read half so _pos/_read_pos is in the middle
    std::vector<std::shared_ptr<int>> buf;
    q.read(buf, N / 2);

    for (auto _ : state) {
        benchmark::DoNotOptimize(q.unacked_count());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_StatsOnly<ListQueue>)->RangeMultiplier(4)->Range(256, 1 << 17)
    ->Name("OldMuxer/StatsDistance/List");
BENCHMARK(BM_StatsOnly<DequeQueue>)->RangeMultiplier(4)->Range(256, 1 << 17)
    ->Name("OldMuxer/StatsDistance/Deque");
