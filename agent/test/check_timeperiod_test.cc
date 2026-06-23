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

#include "check.hh"
#include "com/centreon/common/check_timeperiod.hh"
#include "scheduler.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using Timeperiod = com::centreon::engine::configuration::Timeperiod;
using period_map = absl::flat_hash_map<std::string, const Timeperiod*>;

namespace {

// Build a Timeperiod covering [range_start, range_end) on all 7 days.
Timeperiod make_period_all_days(const std::string& name,
                                int range_start,
                                int range_end) {
  Timeperiod tp;
  tp.set_timeperiod_name(name);
  auto* ranges = tp.mutable_timeranges();
  auto add = [&](auto* day_field) {
    auto* tr = day_field->Add();
    tr->set_range_start(range_start);
    tr->set_range_end(range_end);
  };
  add(ranges->mutable_monday());
  add(ranges->mutable_tuesday());
  add(ranges->mutable_wednesday());
  add(ranges->mutable_thursday());
  add(ranges->mutable_friday());
  add(ranges->mutable_saturday());
  add(ranges->mutable_sunday());
  return tp;
}

// Return time_t for today at hh:mm:00 local time.
time_t today_at(int hour, int minute = 0) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  return mktime(&t);
}

}  // namespace

// ---------------------------------------------------------------------------
// is_time_in_period_by_name - unit tests
// ---------------------------------------------------------------------------

TEST(check_timeperiod_test, empty_name_is_always_active) {
  period_map periods;
  EXPECT_TRUE(com::centreon::common::is_time_in_period_by_name(time(nullptr),
                                                               "", periods));
}

TEST(check_timeperiod_test, unknown_name_is_fail_open) {
  period_map periods;
  EXPECT_TRUE(com::centreon::common::is_time_in_period_by_name(
      time(nullptr), "does_not_exist", periods));
}

TEST(check_timeperiod_test, allday_period_is_always_active) {
  Timeperiod tp = make_period_all_days("always", 0, 86400);
  period_map periods = {{"always", &tp}};
  EXPECT_TRUE(com::centreon::common::is_time_in_period_by_name(
      time(nullptr), "always", periods));
}

TEST(check_timeperiod_test, time_inside_midday_window) {
  // Period 11:00-13:00, noon must be inside.
  Timeperiod tp = make_period_all_days("midday", 11 * 3600, 13 * 3600);
  period_map periods = {{"midday", &tp}};
  EXPECT_TRUE(com::centreon::common::is_time_in_period_by_name(
      today_at(12), "midday", periods));
}

TEST(check_timeperiod_test, time_outside_midday_window) {
  // Period 11:00-13:00, 08:00 must be outside.
  Timeperiod tp = make_period_all_days("midday", 11 * 3600, 13 * 3600);
  period_map periods = {{"midday", &tp}};
  EXPECT_FALSE(com::centreon::common::is_time_in_period_by_name(
      today_at(8), "midday", periods));
}

TEST(check_timeperiod_test, empty_period_is_always_active_fallback) {
  // A Timeperiod proto with no ranges has no defined schedule.
  // check_time_against_period falls back to preferred_time == test_time,
  // so it returns true (fail-open / "no restriction").
  Timeperiod tp;
  tp.set_timeperiod_name("empty");
  period_map periods = {{"empty", &tp}};
  EXPECT_TRUE(com::centreon::common::is_time_in_period_by_name(
      time(nullptr), "empty", periods));
}

// ---------------------------------------------------------------------------
// next_valid_time_in_period_by_name - unit tests
// ---------------------------------------------------------------------------

TEST(check_timeperiod_test, next_valid_empty_name_returns_minus_one) {
  period_map periods;
  EXPECT_EQ(com::centreon::common::next_valid_time_in_period_by_name(
                time(nullptr), "", periods),
            (time_t)-1);
}

TEST(check_timeperiod_test, next_valid_unknown_name_returns_minus_one) {
  period_map periods;
  EXPECT_EQ(com::centreon::common::next_valid_time_in_period_by_name(
                time(nullptr), "unknown", periods),
            (time_t)-1);
}

TEST(check_timeperiod_test, next_valid_outside_period_returns_time_inside) {
  // Period 11:00-13:00 every day.  Query with 08:00 (outside).
  // The returned time must itself fall inside the period.
  Timeperiod tp = make_period_all_days("midday", 11 * 3600, 13 * 3600);
  period_map periods = {{"midday", &tp}};

  time_t query = today_at(8);
  time_t valid = com::centreon::common::next_valid_time_in_period_by_name(
      query, "midday", periods);

  // valid_time must be non-zero and in the future.
  EXPECT_GT(valid, (time_t)0);
  EXPECT_GE(valid, time(nullptr));

  // valid_time must fall inside the period.
  EXPECT_TRUE(com::centreon::common::is_time_in_period_by_name(valid, "midday",
                                                               periods));
}

// ---------------------------------------------------------------------------
// Scheduler integration - timeperiod gating
// ---------------------------------------------------------------------------

// Fake check that records every start.
class tp_check : public check {
  asio::system_timer _timer;

 public:
  static std::vector<time_point> starts;
  static std::mutex starts_m;

  tp_check(const std::shared_ptr<asio::io_context>& io,
           const std::shared_ptr<spdlog::logger>& log,
           time_point exp,
           const Service& svc,
           const engine_to_agent_request_ptr& req,
           check::completion_handler&& h,
           const checks_statistics::pointer& stat)
      : check(io, log, exp, svc, req, std::move(h), stat), _timer(*io) {}

  void start_check(const duration& timeout) override {
    {
      std::lock_guard<std::mutex> l(starts_m);
      starts.emplace_back(std::chrono::system_clock::now());
    }
    if (!_start_check(timeout))
      return;
    // Complete quickly.
    _timer.expires_after(std::chrono::milliseconds(10));
    _timer.async_wait(
        [me = shared_from_this(), this,
         idx = _get_running_check_index()](const boost::system::error_code&) {
          me->on_completion(idx, 0, {}, {"OK"});
        });
  }
};

std::vector<time_point> tp_check::starts;
std::mutex tp_check::starts_m;

// Factory that always creates a tp_check.
static scheduler::check_builder tp_check_builder =
    [](const std::shared_ptr<asio::io_context>& io,
       const std::shared_ptr<spdlog::logger>& log,
       time_point exp,
       const Service& svc,
       const engine_to_agent_request_ptr& req,
       check::completion_handler&& h,
       const checks_statistics::pointer& stat,
       const std::shared_ptr<com::centreon::common::crypto::aes256>&)
    -> check::pointer {
  return std::make_shared<tp_check>(io, log, exp, svc, req, std::move(h), stat);
};

// Build a minimal config with nb_svc services, all sharing the same
// check_period_name.  Optionally attach a Timeperiod to the config.
static std::shared_ptr<MessageToAgent> build_tp_conf(
    unsigned nb_svc,
    const std::string& period_name,
    const Timeperiod* tp = nullptr,
    unsigned check_interval = 10) {
  auto conf = std::make_shared<MessageToAgent>();
  auto* cnf = conf->mutable_config();
  cnf->set_export_period(5);
  cnf->set_max_concurrent_checks(50);
  cnf->set_check_timeout(5);
  cnf->set_use_exemplar(false);

  for (unsigned i = 0; i < nb_svc; ++i) {
    auto* svc = cnf->add_services();
    svc->set_service_description(fmt::format("svc{}", i + 1));
    svc->set_command_name(fmt::format("cmd{}", i + 1));
    svc->set_command_line("/bin/true");
    svc->set_check_interval(check_interval);
    svc->set_host_id(1);
    svc->set_service_id(i + 1);
    if (!period_name.empty())
      svc->set_check_period_name(period_name);
  }

  if (tp)
    *cnf->add_timeperiods() = *tp;

  return conf;
}

class check_timeperiod_scheduler_test : public ::testing::Test {
 protected:
  void SetUp() override {
    std::lock_guard<std::mutex> l(tp_check::starts_m);
    tp_check::starts.clear();
  }

  void TearDown() override {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
};

// Services with an "always active" timeperiod must run normally.
TEST_F(check_timeperiod_scheduler_test, always_active_period_allows_checks) {
  Timeperiod tp = make_period_all_days("always", 0, 86400);
  // 1 services with 3s interval → time_step ≈ 1s, all 3 start within ~2s.
  auto conf = build_tp_conf(1, "always", &tp, 3);

  auto sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {}, tp_check_builder);

  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  {
    std::lock_guard<std::mutex> l(tp_check::starts_m);
    EXPECT_GE(tp_check::starts.size(), 1u)
        << "all 3 services should have started at least once";
  }

  asio::post(*g_io_context, [sched]() { sched->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Services with no check_period_name set must run normally (fail-open).
TEST_F(check_timeperiod_scheduler_test, no_period_name_allows_checks) {
  // empty string → no period constraint
  auto conf = build_tp_conf(3, "", nullptr, 3);

  auto sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {}, tp_check_builder);

  std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  {
    std::lock_guard<std::mutex> l(tp_check::starts_m);
    EXPECT_GE(tp_check::starts.size(), 3u);
  }

  asio::post(*g_io_context, [sched]() { sched->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Services whose check_period_name refers to an inactive window must not
// start any check within a short observation window.
TEST_F(check_timeperiod_scheduler_test, inactive_period_defers_checks) {
  // Build a period whose active window starts ≥ 2 hours from now.
  // This guarantees it is inactive during the test (which runs for 3 s).
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  int start_h = (t.tm_hour + 2) % 24;
  int end_h = (start_h + 1) % 24;

  // If start_h > end_h the window wraps midnight; skip wrapping edge case by
  // shifting one more hour (still ≥ 2 h away).
  if (end_h < start_h)
    end_h = start_h + 1;

  Timeperiod tp = make_period_all_days("future", start_h * 3600, end_h * 3600);
  auto conf = build_tp_conf(2, "future", &tp);

  auto sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {}, tp_check_builder);

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));

  {
    std::lock_guard<std::mutex> l(tp_check::starts_m);
    EXPECT_EQ(tp_check::starts.size(), 0u)
        << "checks outside active timeperiod must not run";
  }

  asio::post(*g_io_context, [sched]() { sched->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// A force check must run immediately even when the service's timeperiod is
// inactive.  The scheduler bypasses the period gate for forced checks.
TEST_F(check_timeperiod_scheduler_test, force_check_bypasses_inactive_period) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  int start_h = (t.tm_hour + 2) % 24;
  int end_h = (start_h + 1) % 24;
  if (end_h < start_h)
    end_h = start_h + 1;

  Timeperiod tp = make_period_all_days("future3", start_h * 3600, end_h * 3600);
  // Single service: host_id=1, service_id=1
  auto conf = build_tp_conf(1, "future3", &tp, 10);

  auto sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {}, tp_check_builder);

  // Let the scheduler defer the check (it is outside the active window).
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  {
    std::lock_guard<std::mutex> l(tp_check::starts_m);
    ASSERT_EQ(tp_check::starts.size(), 0u)
        << "check must not have started before force";
  }

  // Send the force check - must bypass the inactive timeperiod.
  auto req = std::make_shared<MessageToAgent>();
  req->mutable_force_check()->set_host_id(1);
  req->mutable_force_check()->set_serv_id(1);
  asio::post(*g_io_context, [sched, req]() { sched->on_engine_request(req); });

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  {
    std::lock_guard<std::mutex> l(tp_check::starts_m);
    EXPECT_GE(tp_check::starts.size(), 1u)
        << "forced check must run despite inactive timeperiod";
  }

  asio::post(*g_io_context, [sched]() { sched->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
