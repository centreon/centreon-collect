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

/* What load_foreign_objects() costs, and what it holds.
 *
 * The cross-poller validation needs two indexes of names -- host_name and
 * (host_name, service_description) -> poller_id -- and rebuilds them from every
 * stored `<N>.prot` on every configuration cycle. This measures what that
 * rebuild costs as the platform grows, so that replacing it with an index kept
 * up to date incrementally can be decided on figures rather than on principle.
 *
 * DISABLED_ on purpose: this is a measurement, not an assertion, and it writes
 * hundreds of megabytes. Run it explicitly:
 *
 *   tests/ut_broker --gtest_also_run_disabled_tests \
 *                   --gtest_filter='*ForeignObjectsCost*'
 */

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "broker/core/config/applier/broker_state.hh"

using com::centreon::broker::config::applier::broker_state;
namespace cfg = com::centreon::engine::configuration;

namespace {

/* Written so that one object weighs roughly what it weighs in a real stored
 * configuration -- about 140 bytes, measured on a test `.prot`. An index built
 * from messages carrying nothing but a name would parse far too fast to say
 * anything about production. */
void fill_state(cfg::State& st,
                uint32_t poller_id,
                uint32_t hosts,
                uint32_t services_per_host) {
  st.set_poller_id(poller_id);
  st.set_poller_name(fmt::format("Poller{}", poller_id));
  for (uint32_t h = 0; h < hosts; h++) {
    auto* host = st.add_hosts();
    host->set_host_name(fmt::format("poller{}-host_{}", poller_id, h));
    host->set_alias(fmt::format("alias of host {} of poller {}", h, poller_id));
    host->set_address("192.168.100.200");
    host->set_check_command("check_host_alive!arg1!arg2");
    host->set_check_period("24x7");
    host->set_notification_period("24x7");
    host->set_display_name(fmt::format("poller{}-host_{}", poller_id, h));
    host->mutable_obj()->set_register_(true);
    for (uint32_t s = 0; s < services_per_host; s++) {
      auto* svc = st.add_services();
      svc->set_host_name(fmt::format("poller{}-host_{}", poller_id, h));
      svc->set_service_description(fmt::format("service_{}", s));
      svc->set_check_command("check_centreon_ping!3!200,20%!400,50%");
      svc->set_check_period("24x7");
      svc->set_notification_period("24x7");
      svc->set_display_name(fmt::format("service_{}", s));
      svc->mutable_obj()->set_register_(true);
    }
  }
}

/* Resident set size, to see what the structure holds while it is alive.
 *
 * Signed on purpose: the difference is taken against the RSS *before* the call,
 * and once the heap has been grown by an earlier measurement it does not shrink
 * back, so a later one can legitimately end lower than it started. Read as
 * unsigned that becomes an absurd number -- which it did. A delta is therefore
 * only meaningful on a heap that has not already been grown to that size; the
 * first measurement of a run is the one to trust. */
int64_t rss_kb() {
  std::ifstream f("/proc/self/statm");
  int64_t total = 0, resident = 0;
  f >> total >> resident;
  return resident * (sysconf(_SC_PAGESIZE) / 1024);
}

}  // namespace

class ForeignObjectsCost : public ::testing::Test {
 protected:
  std::filesystem::path _dir;

  void SetUp() override {
    com::centreon::broker::config::applier::state::load<broker_state>(
        "unittest");
    _dir = std::filesystem::temp_directory_path() / "ut_foreign_bench";
    std::filesystem::remove_all(_dir);
    std::filesystem::create_directories(_dir);
  }

  void TearDown() override {
    com::centreon::broker::config::applier::state::unload();
    std::filesystem::remove_all(_dir);
  }

  /** @brief One measurement: @p pollers stored configurations of @p hosts hosts
   * carrying @p services_per_host services each. */
  void measure(uint32_t pollers, uint32_t hosts, uint32_t services_per_host) {
    auto* st = static_cast<broker_state*>(
        &com::centreon::broker::config::applier::state::instance());
    st->set_pollers_config_dir(_dir);

    size_t bytes = 0;
    for (uint32_t p = 1; p <= pollers; p++) {
      cfg::State s;
      fill_state(s, p, hosts, services_per_host);
      const auto path = _dir / fmt::format("{}.prot", p);
      std::ofstream f(path, std::ios::binary);
      ASSERT_TRUE(s.SerializeToOstream(&f));
      f.close();
      bytes += std::filesystem::file_size(path);
    }

    /* Three runs, median kept: the first pays for warming the page cache, and
     * a single run cannot be told from noise.
     *
     * The release is inside the measurement on purpose. Timing only the call
     * would flatter any version that carries its garbage out in the returned
     * value and lets the caller pay for it afterwards -- which is precisely
     * what this function used to do, holding every parsed State so the index
     * could borrow its strings. Freeing a 50,000-service State costs about half
     * of what parsing it does, so leaving that out compares nothing worth
     * comparing. */
    std::vector<double> ms;
    int64_t held_kb = 0;
    for (int run = 0; run < 3; run++) {
      const int64_t before = rss_kb();
      const auto t0 = std::chrono::steady_clock::now();
      {
        auto foreign = st->load_foreign_objects();
        held_kb = rss_kb() - before;
        EXPECT_EQ(foreign.host_count(), pollers * hosts);
      }
      const auto t1 = std::chrono::steady_clock::now();
      ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(ms.begin(), ms.end());

    fmt::print(
        "{:>3} pollers x {:>6} services | {:>7.1f} Mo on disk | {:>8.1f} ms "
        "(release included) | {:>6.1f} Mo peak\n",
        pollers, hosts * services_per_host, bytes / 1048576.0, ms[1],
        held_kb / 1024.0);
  }
};

TEST_F(ForeignObjectsCost, DISABLED_PlatformSizes) {
  fmt::print(
      "\n--- load_foreign_objects(), one call per configuration cycle ---\n");
  for (uint32_t pollers : {1u, 5u, 10u, 20u})
    measure(pollers, 400, 25);
}

TEST_F(ForeignObjectsCost, DISABLED_LargePollers) {
  fmt::print("\n--- larger pollers ---\n");
  measure(5, 2000, 25);
  measure(10, 2000, 25);
}

/* The strategy this replaced, so the comparison rests on a measurement and not
 * on an extrapolation.
 *
 * The index used to borrow its strings from the parsed messages, which forced
 * every State to be kept alive and carried out in the returned value. The work
 * is otherwise identical -- same files, same two indexes -- so timing both with
 * the release included says what the change really cost or saved. */
namespace {

struct borrowing_index {
  std::vector<std::unique_ptr<cfg::State>> states;
  absl::flat_hash_map<std::string_view, uint64_t> hosts;
  absl::flat_hash_map<std::pair<std::string_view, std::string_view>, uint64_t>
      services;
};

borrowing_index load_borrowing(const std::filesystem::path& dir) {
  borrowing_index retval;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    auto state = std::make_unique<cfg::State>();
    std::ifstream f(entry.path(), std::ios::binary);
    if (!f || !state->ParseFromIstream(&f))
      continue;
    const uint64_t id = state->poller_id();
    for (const auto& h : state->hosts())
      retval.hosts.emplace(h.host_name(), id);
    for (const auto& s : state->services())
      retval.services.emplace(std::pair<std::string_view, std::string_view>(
                                  s.host_name(), s.service_description()),
                              id);
    retval.states.push_back(std::move(state));
  }
  return retval;
}

}  // namespace

TEST_F(ForeignObjectsCost, DISABLED_BorrowingVersusOwning) {
  auto* st = static_cast<broker_state*>(
      &com::centreon::broker::config::applier::state::instance());
  st->set_pollers_config_dir(_dir);
  for (uint32_t p = 1; p <= 10; p++) {
    cfg::State s;
    fill_state(s, p, 2000, 25);
    std::ofstream f(_dir / fmt::format("{}.prot", p), std::ios::binary);
    ASSERT_TRUE(s.SerializeToOstream(&f));
  }

  /* ABBA, so that a drift between the two halves of the run cannot be read as a
   * difference between the two strategies. */
  std::vector<double> borrow, own;
  for (int round = 0; round < 2; round++) {
    for (int which = 0; which < 2; which++) {
      const bool borrowing = (round == 0) ? which == 0 : which == 1;
      const int64_t before = rss_kb();
      int64_t peak = 0;
      const auto t0 = std::chrono::steady_clock::now();
      if (borrowing) {
        auto idx = load_borrowing(_dir);
        peak = rss_kb() - before;
        EXPECT_EQ(idx.services.size(), 500000u);
      } else {
        auto idx = st->load_foreign_objects();
        peak = rss_kb() - before;
        EXPECT_EQ(idx.service_count(), 500000u);
      }
      const auto t1 = std::chrono::steady_clock::now();
      const double ms =
          std::chrono::duration<double, std::milli>(t1 - t0).count();
      (borrowing ? borrow : own).push_back(ms);
      fmt::print("{:<9} | {:>8.1f} ms (release included) | {:>+7.1f} Mo peak\n",
                 borrowing ? "borrowing" : "owning", ms, peak / 1024.0);
    }
  }
  fmt::print("borrowing mean {:.1f} ms, owning mean {:.1f} ms\n",
             (borrow[0] + borrow[1]) / 2, (own[0] + own[1]) / 2);
}

/* Where the time actually goes.
 *
 * Making the index own its names moved the release of the parsed messages from
 * *after* load_foreign_objects() returned to *inside* it: each State is now
 * destroyed as soon as it has been walked, instead of being carried out in the
 * returned structure and freed by the caller later on. Destroying a State of
 * 50,000 services frees hundreds of thousands of protobuf allocations, so the
 * question is whether the measured slowdown is that cost entering the
 * measurement rather than any new work. */
TEST_F(ForeignObjectsCost, DISABLED_WhereTheTimeGoes) {
  auto* st = static_cast<broker_state*>(
      &com::centreon::broker::config::applier::state::instance());
  st->set_pollers_config_dir(_dir);

  cfg::State model;
  fill_state(model, 1, 2000, 25);
  const auto path = _dir / "1.prot";
  {
    std::ofstream f(path, std::ios::binary);
    ASSERT_TRUE(model.SerializeToOstream(&f));
  }

  double parse_ms = 0, destroy_ms = 0;
  for (int run = 0; run < 3; run++) {
    auto state = std::make_unique<cfg::State>();
    std::ifstream f(path, std::ios::binary);
    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(state->ParseFromIstream(&f));
    const auto t1 = std::chrono::steady_clock::now();
    state.reset();
    const auto t2 = std::chrono::steady_clock::now();
    parse_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    destroy_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
  }

  /* And what the whole call costs now, release included, which is what the
   * caller really pays either way. */
  const auto t0 = std::chrono::steady_clock::now();
  {
    auto foreign = st->load_foreign_objects();
    EXPECT_EQ(foreign.service_count(), 50000u);
  }
  const auto t1 = std::chrono::steady_clock::now();

  fmt::print(
      "\n--- one poller, 50 000 services ---\n"
      "parse of the State          : {:>7.1f} ms\n"
      "destruction of that State   : {:>7.1f} ms\n"
      "load_foreign_objects + free : {:>7.1f} ms\n",
      parse_ms, destroy_ms,
      std::chrono::duration<double, std::milli>(t1 - t0).count());
}
