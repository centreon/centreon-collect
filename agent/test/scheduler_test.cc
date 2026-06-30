/**
 * Copyright 2024-2025 Centreon
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
#include "check_exec.hh"
#include "common/crypto/aes256.hh"

#include "scheduler.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;

class tempo_check : public check {
  asio::system_timer _completion_timer;
  int _command_exit_status;
  duration _completion_delay;

 public:
  static std::vector<std::pair<tempo_check*, time_point>> check_starts;
  static std::mutex check_starts_m;

  static uint64_t completion_time;

  tempo_check(const std::shared_ptr<asio::io_context>& io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              time_point exp,
              const Service& serv,
              const engine_to_agent_request_ptr& cnf,
              int command_exit_status,
              duration completion_delay,
              check::completion_handler&& handler,
              const checks_statistics::pointer& stat)
      : check(io_context, logger, exp, serv, cnf, std::move(handler), stat),
        _completion_timer(*io_context),
        _command_exit_status(command_exit_status),
        _completion_delay(completion_delay) {}

  void start_check(const duration& timeout) override {
    {
      std::lock_guard l(check_starts_m);
      SPDLOG_INFO("start tempo check");
      check_starts.emplace_back(this, std::chrono::system_clock::now());
    }
    if (!_start_check(timeout)) {
      return;
    }
    _completion_timer.expires_after(_completion_delay);
    _completion_timer.async_wait([me = shared_from_this(), this,
                                  check_running_index =
                                      _get_running_check_index()](
                                     [[maybe_unused]] const boost::system::
                                         error_code& err) {
      SPDLOG_TRACE("end of completion timer for serv {}", get_service());
      me->on_completion(
          check_running_index, _command_exit_status,
          com::centreon::common::perfdata::parse_perfdata(
              0, 0,
              "rta=0,031ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,109ms;;;; "
              "rtmin=0,011ms;;;;",
              _logger),
          {fmt::format("Command OK: {}", me->get_command_line())});
      completion_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    });
  }
};

std::vector<std::pair<tempo_check*, time_point>> tempo_check::check_starts;
std::mutex tempo_check::check_starts_m;
uint64_t tempo_check::completion_time;

class scheduler_closer {
  std::shared_ptr<scheduler> _to_stop;

 public:
  scheduler_closer(const std::shared_ptr<scheduler>& to_stop)
      : _to_stop(to_stop) {}

  ~scheduler_closer() {
    asio::post(*g_io_context, [to_stop = _to_stop]() { to_stop->stop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
};

class scheduler_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    spdlog::default_logger()->set_level(spdlog::level::trace);
  }

  void TearDown() override {
    // let time to async check to end
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::shared_ptr<com::centreon::agent::MessageToAgent> create_conf(
      unsigned nb_serv,
      unsigned second_check_period,
      unsigned export_period,
      unsigned max_concurent_check,
      unsigned check_timeout);

  std::shared_ptr<com::centreon::agent::MessageToAgent> force_check(
      uint64_t host_id,
      uint64_t serv_id);
};

std::shared_ptr<com::centreon::agent::MessageToAgent>
scheduler_test::create_conf(unsigned nb_serv,
                            unsigned second_check_period,
                            unsigned export_period,
                            unsigned max_concurent_check,
                            unsigned check_timeout) {
  std::shared_ptr<com::centreon::agent::MessageToAgent> conf =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_export_period(export_period);
  cnf->set_max_concurrent_checks(max_concurent_check);
  cnf->set_check_timeout(check_timeout);
  cnf->set_use_exemplar(true);
  for (unsigned serv_index = 0; serv_index < nb_serv; ++serv_index) {
    auto serv = cnf->add_services();
    serv->set_service_description(fmt::format("serv{}", serv_index + 1));
    serv->set_command_name(fmt::format("command{}", serv_index + 1));
    serv->set_command_line("/usr/bin/ls");
    serv->set_check_interval(second_check_period);
    serv->set_host_id(1);
    serv->set_service_id(serv_index + 1);
  }
  return conf;
}

std::shared_ptr<com::centreon::agent::MessageToAgent>
scheduler_test::force_check(uint64_t host_id, uint64_t serv_id) {
  std::shared_ptr<com::centreon::agent::MessageToAgent> request =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  auto force = request->mutable_force_check();
  force->set_host_id(host_id);
  force->set_serv_id(serv_id);

  return request;
}

TEST_F(scheduler_test, no_config) {
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      scheduler::default_config(),
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>&,
         const std::shared_ptr<spdlog::logger>&, time_point /* start expected*/,
         const Service&,
         const engine_to_agent_request_ptr& /*engine to agent request*/,
         check::completion_handler&&, const checks_statistics::pointer&,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::shared_ptr<check>();
      });

  std::weak_ptr<scheduler> weak_shed(sched);
  sched.reset();

  // scheduler must be owned by asio
  ASSERT_TRUE(weak_shed.lock());

  weak_shed.lock()->stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  ASSERT_FALSE(weak_shed.lock());
}

static bool tempo_check_assert_pred(const time_point& after,
                                    const time_point& before) {
  if ((after - before) <= std::chrono::milliseconds(250)) {
    SPDLOG_ERROR("after={}, before={}", after, before);
    return false;
  }
  if ((after - before) >= std::chrono::milliseconds(750)) {
    SPDLOG_ERROR("after={}, before={}", after, before);
    return false;
  }
  return true;
}

static bool tempo_check_assert_pred3(const time_point& after,
                                     const time_point& before,
                                     const unsigned min_second_check_interval) {
  const std::chrono::milliseconds delta =
      std::chrono::duration_cast<std::chrono::milliseconds>(after - before);

  const std::chrono::milliseconds expected(
      std::chrono::milliseconds(min_second_check_interval * 1000 / 20));
  const std::chrono::milliseconds tol(250);

  if (delta < expected - tol || delta > expected + tol) {
    SPDLOG_ERROR(
        "delta={}ms expected={}ms±{}ms (min_second_check_interval={}) "
        "after={}, before={}",
        delta.count(), expected.count(), tol.count(), min_second_check_interval,
        after, before);
    return false;
  }
  return true;
}

static bool tempo_check_interval(const check* chk,
                                 const time_point& after,
                                 const time_point& before) {
  if ((after - before) <=
      (chk->get_check_interval() - std::chrono::seconds(1))) {
    SPDLOG_ERROR("after={}, before={}", after, before);
    return false;
  }
  if ((after - before) >=
      (chk->get_check_interval() + std::chrono::seconds(1))) {
    SPDLOG_ERROR("after={}, before={}", after, before);
    return false;
  }
  return true;
}

/**
 * @brief We plan 20 checks with a 10 s check period
 * We expect that first between check interval is around 500ms
 * Then we check that check interval is about 10s
 *
 */
TEST_F(scheduler_test, correct_schedule) {
  {
    std::lock_guard l(tempo_check::check_starts_m);
    tempo_check::check_starts.clear();
  }

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      create_conf(20, 10, 1, 50, 1),
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::make_shared<tempo_check>(io_context, logger, start_expected,
                                             service, engine_to_agent_request,
                                             0, std::chrono::milliseconds(50),
                                             std::move(handler), stat);
      });

  scheduler_closer closer(sched);

  std::this_thread::sleep_for(std::chrono::milliseconds(11100));

  {
    std::lock_guard l(tempo_check::check_starts_m);
    ASSERT_GE(tempo_check::check_starts.size(), 20);
    bool first = true;
    std::pair<tempo_check*, time_point> previous;
    for (const auto& check_time : tempo_check::check_starts) {
      if (first) {
        first = false;
      } else {
        ASSERT_NE(previous.first, check_time.first);
        // check if we have a delay of 500ms between two checks
        ASSERT_PRED2(tempo_check_assert_pred, check_time.second,
                     previous.second);
      }
      previous = check_time;
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(16000));
  // we have at least two checks for each service
  {
    std::lock_guard l(tempo_check::check_starts_m);
    ASSERT_GE(tempo_check::check_starts.size(), 40);
    std::map<tempo_check*, time_point> previous;
    for (const auto& [check, tp] : tempo_check::check_starts) {
      auto yet_one = previous.find(check);
      if (yet_one == previous.end()) {
        previous.emplace(check, tp);
      } else {
        ASSERT_PRED3(tempo_check_interval, check, tp, yet_one->second);
        yet_one->second = tp;
      }
    }
  }
}

/**
 * @brief We plan 20 checks with a 5 to 10 s check period
 * We expect that first between check interval is around 500ms
 * Then we check that check intervals are respected
 *
 */
TEST_F(scheduler_test, correct_schedule_diff_intervals) {
  {
    std::lock_guard l(tempo_check::check_starts_m);
    tempo_check::check_starts.clear();
  }

  std::shared_ptr<com::centreon::agent::MessageToAgent> conf =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_export_period(10);
  cnf->set_max_concurrent_checks(50);
  cnf->set_check_timeout(10);
  cnf->set_use_exemplar(true);
  unsigned min_interval = 10;
  for (unsigned serv_index = 0; serv_index < 20; ++serv_index) {
    auto serv = cnf->add_services();
    serv->set_service_description(fmt::format("serv{}", serv_index + 1));
    serv->set_command_name(fmt::format("command{}", serv_index + 1));
    serv->set_command_line("/usr/bin/ls");
    serv->set_check_interval(5 + (rand() % 5));
    serv->set_max_attempts(1);
    serv->set_retry_interval(1 + (rand() % 5));
    if (serv->check_interval() < min_interval) {
      min_interval = std::min(serv->check_interval(), serv->retry_interval());
    }
  }

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::make_shared<tempo_check>(io_context, logger, start_expected,
                                             service, engine_to_agent_request,
                                             0, std::chrono::milliseconds(50),
                                             std::move(handler), stat);
      });

  scheduler_closer closer(sched);

  // the time that we wait is less than the min interval to be sure that
  // we are in the first round of checks
  std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  {
    std::lock_guard l(tempo_check::check_starts_m);
    ASSERT_GE(tempo_check::check_starts.size(), 20);
    bool first = true;
    std::pair<tempo_check*, time_point> previous;
    for (const auto& check_time : tempo_check::check_starts) {
      if (first) {
        first = false;
      } else {
        ASSERT_NE(previous.first, check_time.first);
        // check if we have a delay of 450ms between two checks
        SPDLOG_INFO(
            "Checking tempo_check delay for serv{}: after={}, before={}",
            check_time.first->get_service(), check_time.second,
            previous.second);
        ASSERT_PRED3(tempo_check_assert_pred3, check_time.second,
                     previous.second, min_interval);
      }
      previous = check_time;
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(16000));
  // we have at least two checks for each service
  {
    std::lock_guard l(tempo_check::check_starts_m);
    ASSERT_GE(tempo_check::check_starts.size(), 40);
    std::map<tempo_check*, time_point> previous;
    for (const auto& [check, tp] : tempo_check::check_starts) {
      auto yet_one = previous.find(check);
      if (yet_one == previous.end()) {
        previous.emplace(check, tp);
      } else {
        ASSERT_PRED3(tempo_check_interval, check, tp, yet_one->second);
        yet_one->second = tp;
      }
    }
  }
}

/**
 * @brief: we execute long checks with a 1s timeout and
 * we expect completion time = now + 1s + timeout(1s)
 */
TEST_F(scheduler_test, time_out) {
  std::shared_ptr<MessageFromAgent> exported_request;
  std::condition_variable export_cond;
  uint64_t expected_completion_time =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  std::mutex m;
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      create_conf(1, 1, 1, 1, 1),
      [&](const std::shared_ptr<MessageFromAgent>& req) {
        {
          std::lock_guard l(m);
          exported_request = req;
        }
        export_cond.notify_all();
      },
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::make_shared<tempo_check>(io_context, logger, start_expected,
                                             service, engine_to_agent_request,
                                             0, std::chrono::milliseconds(1500),
                                             std::move(handler), stat);
      });

  std::unique_lock l(m);
  export_cond.wait(l);

  ASSERT_TRUE(exported_request);
  ASSERT_EQ(exported_request->otel_request().resource_metrics_size(), 1);
  const ::opentelemetry::proto::metrics::v1::ResourceMetrics& res =
      exported_request->otel_request().resource_metrics()[0];
  const auto& res_attrib = res.resource().attributes();
  ASSERT_EQ(res_attrib.size(), 2);
  ASSERT_EQ(res_attrib.at(0).key(), "host.name");
  ASSERT_EQ(res_attrib.at(0).value().string_value(), "my_host");
  ASSERT_EQ(res_attrib.at(1).key(), "service.name");
  ASSERT_EQ(res_attrib.at(1).value().string_value(), "serv1");
  ASSERT_EQ(res.scope_metrics_size(), 1);
  const ::opentelemetry::proto::metrics::v1::ScopeMetrics& scope_metrics =
      res.scope_metrics()[0];
  ASSERT_EQ(scope_metrics.metrics_size(), 1);
  const ::opentelemetry::proto::metrics::v1::Metric metric =
      scope_metrics.metrics()[0];
  ASSERT_EQ(metric.name(), "status");
  ASSERT_EQ(metric.description(), "Timeout at execution of /usr/bin/ls");
  ASSERT_EQ(metric.gauge().data_points_size(), 1);
  const auto& data_point = metric.gauge().data_points()[0];
  ASSERT_EQ(data_point.as_int(), 3);
  // timeout 1s
  ASSERT_GE(data_point.time_unix_nano(), expected_completion_time + 2000000000);
  ASSERT_LE(data_point.time_unix_nano(), expected_completion_time + 2500000000);

  scheduler_closer closer(sched);
}

TEST_F(scheduler_test, correct_output_examplar) {
  std::shared_ptr<MessageFromAgent> exported_request;
  std::condition_variable export_cond;
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      create_conf(2, 1, 2, 10, 1),
      [&](const std::shared_ptr<MessageFromAgent>& req) {
        exported_request = req;
        export_cond.notify_all();
      },
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::make_shared<tempo_check>(io_context, logger, start_expected,
                                             service, engine_to_agent_request,
                                             0, std::chrono::milliseconds(10),
                                             std::move(handler), stat);
      });

  std::mutex m;
  std::unique_lock l(m);
  export_cond.wait(l);

  ASSERT_TRUE(exported_request);

  SPDLOG_INFO("export:{}", exported_request->otel_request());

  ASSERT_EQ(exported_request->otel_request().resource_metrics_size(), 2);
  const ::opentelemetry::proto::metrics::v1::ResourceMetrics& res =
      exported_request->otel_request().resource_metrics()[0];
  const auto& res_attrib = res.resource().attributes();
  ASSERT_EQ(res_attrib.size(), 2);
  ASSERT_EQ(res_attrib.at(0).key(), "host.name");
  ASSERT_EQ(res_attrib.at(0).value().string_value(), "my_host");
  ASSERT_EQ(res_attrib.at(1).key(), "service.name");
  ASSERT_EQ(res_attrib.at(1).value().string_value(), "serv2");
  ASSERT_EQ(res.scope_metrics_size(), 1);
  const ::opentelemetry::proto::metrics::v1::ScopeMetrics& scope_metrics =
      res.scope_metrics()[0];
  ASSERT_GE(scope_metrics.metrics_size(), 5);
  const ::opentelemetry::proto::metrics::v1::Metric metric =
      scope_metrics.metrics()[0];
  ASSERT_EQ(metric.name(), "status");
  ASSERT_EQ(metric.description(), "Command OK: /usr/bin/ls");
  ASSERT_GE(metric.gauge().data_points_size(), 1);
  const auto& data_point_state = metric.gauge().data_points()[0];
  ASSERT_EQ(data_point_state.as_int(), 0);
  uint64_t first_time_point = data_point_state.time_unix_nano();

  const ::opentelemetry::proto::metrics::v1::ResourceMetrics& res2 =
      exported_request->otel_request().resource_metrics()[1];
  const auto& res_attrib2 = res2.resource().attributes();
  ASSERT_EQ(res_attrib2.size(), 2);
  ASSERT_EQ(res_attrib2.at(0).key(), "host.name");
  ASSERT_EQ(res_attrib2.at(0).value().string_value(), "my_host");
  ASSERT_EQ(res_attrib2.at(1).key(), "service.name");
  ASSERT_EQ(res_attrib2.at(1).value().string_value(), "serv1");
  ASSERT_EQ(res2.scope_metrics_size(), 1);

  const ::opentelemetry::proto::metrics::v1::ScopeMetrics& scope_metrics2 =
      res2.scope_metrics()[0];
  ASSERT_EQ(scope_metrics2.metrics_size(), 5);
  const ::opentelemetry::proto::metrics::v1::Metric metric2 =
      scope_metrics2.metrics()[0];
  ASSERT_EQ(metric2.name(), "status");
  ASSERT_EQ(metric2.description(), "Command OK: /usr/bin/ls");
  ASSERT_GE(metric2.gauge().data_points_size(), 1);
  const auto& data_point_state2 = metric2.gauge().data_points()[0];
  ASSERT_EQ(data_point_state2.as_int(), 0);

  ASSERT_LE(first_time_point + 400000000, data_point_state2.time_unix_nano());
  ASSERT_GE(first_time_point + 600000000, data_point_state2.time_unix_nano());

  scheduler_closer closer(sched);
}

/**
 * @brief: this fake check store executed checks and max active checks at a time
 */
class concurent_check : public check {
  asio::system_timer _completion_timer;
  int _command_exit_status;
  duration _completion_delay;

 public:
  static std::set<concurent_check*> checked;
  static std::set<concurent_check*> active_checks;
  static std::mutex checked_m;
  static unsigned max_active_check;

  concurent_check(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  time_point exp,
                  const Service& serv,
                  const engine_to_agent_request_ptr& cnf,
                  int command_exit_status,
                  duration completion_delay,
                  check::completion_handler&& handler,
                  const checks_statistics::pointer& stat)
      : check(io_context, logger, exp, serv, cnf, std::move(handler), stat),
        _completion_timer(*io_context),
        _command_exit_status(command_exit_status),
        _completion_delay(completion_delay) {}

  void start_check(const duration& timeout) override {
    if (!_start_check(timeout)) {
      return;
    }
    {
      std::lock_guard l(checked_m);
      active_checks.insert(this);
      if (active_checks.size() > max_active_check) {
        max_active_check = active_checks.size();
      }
    }
    _completion_timer.expires_after(_completion_delay);
    _completion_timer.async_wait([me = shared_from_this(), this,
                                  check_running_index =
                                      _get_running_check_index()](
                                     [[maybe_unused]] const boost::system::
                                         error_code& err) {
      {
        std::lock_guard l(checked_m);
        active_checks.erase(this);
        checked.insert(this);
      }
      SPDLOG_TRACE("end of completion timer for serv {}", get_service());
      me->on_completion(
          check_running_index, _command_exit_status,
          com::centreon::common::perfdata::parse_perfdata(
              0, 0,
              "rta=0,031ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,109ms;;;; "
              "rtmin=0,011ms;;;;",
              _logger),
          {fmt::format("Command OK: {}", me->get_command_line())});
    });
  }
};

std::set<concurent_check*> concurent_check::checked;
std::set<concurent_check*> concurent_check::active_checks;
unsigned concurent_check::max_active_check;
std::mutex concurent_check::checked_m;

/**
 * @brief we configure a scheduler with 200 services and we expect
 * that scheduler will respect maximum concurent check (10)
 */
TEST_F(scheduler_test, max_concurent) {
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      create_conf(200, 10, 1, 10, 1),
      [&]([[maybe_unused]] const std::shared_ptr<MessageFromAgent>& req) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::make_shared<concurent_check>(
            io_context, logger, start_expected, service,
            engine_to_agent_request, 0,
            std::chrono::milliseconds(750 -
                                      10) /*the - 10 is for some delay in test
                                             execution from start expected*/
            ,
            std::move(handler), stat);
      });

  scheduler_closer closer(sched);

  // to many tests to be completed in 12s
  std::this_thread::sleep_for(std::chrono::milliseconds(12000));
  EXPECT_LT(concurent_check::checked.size(), 200);
  EXPECT_EQ(concurent_check::max_active_check, 10);

  // all tests must be completed in 16s ((0.75*200)/10 + 1) but we add 2s
  // because of windows slowness
  std::this_thread::sleep_for(std::chrono::milliseconds(6000));
  EXPECT_EQ(concurent_check::max_active_check, 10);
  EXPECT_EQ(concurent_check::checked.size(), 200);
}

/**
 * @brief we fill scheduler waitqueue with 200 services, we force check of midle
 * and last service and we expect that its will be executed before others
 *
 */
TEST_F(scheduler_test, force_check) {
  concurent_check::checked.clear();
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      create_conf(200, 10, 1, 10, 1),
      [&]([[maybe_unused]] const std::shared_ptr<MessageFromAgent>& req) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        return std::make_shared<concurent_check>(
            io_context, logger, start_expected, service,
            engine_to_agent_request, 0, std::chrono::milliseconds(500),
            std::move(handler), stat);
      });

  scheduler_closer closer(sched);

  asio::post(*g_io_context, [this, sched]() {
    sched->on_engine_request(force_check(1, 100));
  });

  asio::post(*g_io_context, [this, sched]() {
    sched->on_engine_request(force_check(1, 200));
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std::lock_guard l(concurent_check::checked_m);

  bool serv_100_found = false;
  bool serv_200_found = false;
  std::for_each(concurent_check::checked.begin(),
                concurent_check::checked.end(),
                [&](const concurent_check* checked) {
                  if (checked->get_service_id() == 100) {
                    serv_100_found = true;
                  } else if (checked->get_service_id() == 200) {
                    serv_200_found = true;
                  }
                });

  ASSERT_TRUE(serv_100_found) << "serv 100 must be checked before others";
  ASSERT_TRUE(serv_200_found) << "serv 201 must be checked before others";
}

TEST_F(scheduler_test, can_not_decrypt) {
  std::shared_ptr<check> created;

  std::shared_ptr<com::centreon::agent::MessageToAgent> conf =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_export_period(15);
  cnf->set_max_concurrent_checks(10);
  cnf->set_check_timeout(10);
  cnf->set_use_exemplar(true);
  cnf->set_key("SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=");
  cnf->set_salt("U2FsdA==");
  auto serv = cnf->add_services();
  serv->set_service_description("serv");
  serv->set_command_name("command");
  serv->set_command_line("encrypt::/usr/bin/ls");
  serv->set_check_interval(10);

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [&](const std::shared_ptr<asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          time_point start_expected, const Service& service,
          const engine_to_agent_request_ptr& engine_to_agent_request,
          check::completion_handler&& handler,
          const checks_statistics::pointer& stat,
          const std::shared_ptr<com::centreon::common::crypto::aes256>&
              credentials_decrypt) {
        created = scheduler::default_check_builder(
            io_context, logger, start_expected, service,
            engine_to_agent_request, std::move(handler), stat,
            credentials_decrypt);
        return created;
      });

  scheduler_closer closer(sched);

  auto check_created = std::dynamic_pointer_cast<check_dummy>(created);
  ASSERT_TRUE(check_created);
  EXPECT_EQ(
      check_created->get_output(),
      "Unable to decrypt command line The content is not AES256 encrypted");
}

TEST_F(scheduler_test, can_decrypt) {
  auto credentials_decrypt =
      std::make_unique<com::centreon::common::crypto::aes256>(
          "SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=", "U2FsdA==");

  std::string command_line =
      "encrypt::" + credentials_decrypt->encrypt("/usr/bin/ls *.*");

  std::shared_ptr<check> created;

  std::shared_ptr<com::centreon::agent::MessageToAgent> conf =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_export_period(15);
  cnf->set_max_concurrent_checks(10);
  cnf->set_check_timeout(10);
  cnf->set_use_exemplar(true);
  cnf->set_key("SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=");
  cnf->set_salt("U2FsdA==");
  auto serv = cnf->add_services();
  serv->set_service_description("serv");
  serv->set_command_name("command");
  serv->set_command_line(command_line);
  serv->set_check_interval(10);

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [&](const std::shared_ptr<asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          time_point start_expected, const Service& service,
          const engine_to_agent_request_ptr& engine_to_agent_request,
          check::completion_handler&& handler,
          const checks_statistics::pointer& stat,
          const std::shared_ptr<com::centreon::common::crypto::aes256>&
              credentials_decrypt) {
        created = scheduler::default_check_builder(
            io_context, logger, start_expected, service,
            engine_to_agent_request, std::move(handler), stat,
            credentials_decrypt);
        return created;
      });

  scheduler_closer closer(sched);

  auto check_created = std::dynamic_pointer_cast<check_exec>(created);
  ASSERT_TRUE(check_created);
  check_created->get_process_args()->decrypt_args(*credentials_decrypt);
  ASSERT_EQ(check_created->get_process_args()->get_c_args().size(), 3);
  EXPECT_EQ(check_created->get_process_args()->get_exe_path(), "/usr/bin/ls");
  EXPECT_EQ(check_created->get_process_args()->get_args()[0], "*.*");
}

/*
 * struct to store data about a check execution
 * used by scripted_check to store data about each check execution
 */
struct datapoint {
  unsigned status;
  int last_status;
  time_point time;
  bool confirmed;
  unsigned current_attempt;
  unsigned id_data;
};

/**
 * @brief: scripted check that perform status change on each execution
 * We use it to test retry interval and check interval usage
 */
class scripted_check : public check {
  asio::system_timer _completion_timer;
  std::vector<unsigned> _statuses;
  size_t idx = 0;
  duration _completion_delay;

 public:
  static std::map<scripted_check*, std::vector<datapoint>> map_check_starts;
  static std::mutex check_m;

  unsigned _id = 0;

  scripted_check(const std::shared_ptr<asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger,
                 time_point exp,
                 const Service& serv,
                 const engine_to_agent_request_ptr& cnf,
                 duration completion_delay,
                 check::completion_handler&& handler,
                 const checks_statistics::pointer& stat,
                 std::vector<unsigned> statuses)
      : check(io_context, logger, exp, serv, cnf, std::move(handler), stat),
        _completion_timer(*io_context),
        _statuses(std::move(statuses)),
        _completion_delay(completion_delay) {}

  void start_check(const duration& timeout) override {
    {
      std::lock_guard l(check_m);
      _id++;
      datapoint dp{};
      dp.time = std::chrono::system_clock::now();
      dp.id_data = _id;
      map_check_starts[this].push_back(std::move(dp));
    }
    if (!_start_check(timeout)) {
      return;
    }
    _completion_timer.expires_after(_completion_delay);
    _completion_timer.async_wait([me = std::static_pointer_cast<scripted_check>(
                                      shared_from_this()),
                                  this,
                                  check_running_index =
                                      _get_running_check_index()](
                                     [[maybe_unused]] const boost::system::
                                         error_code& err) {
      // save the last status and status confirmed
      {
        std::lock_guard l(check_m);
        unsigned current_attempt = me->get_current_attempt();
        bool confirmed = me->get_status_confirmed();
        unsigned last_status = me->get_last_status();
        // perform status change
        me->calcul_status_confirmation(_statuses[idx % _statuses.size()]);

        if (me->_id == map_check_starts[this].back().id_data) {
          map_check_starts[this].back().status =
              _statuses[idx % _statuses.size()];
          map_check_starts[this].back().last_status = last_status;
          map_check_starts[this].back().confirmed = me->get_status_confirmed();
          map_check_starts[this].back().current_attempt =
              me->get_current_attempt();
        } else {
          SPDLOG_ERROR(
              "skipping datapoint for check {} id_data={} current_id={}",
              get_service(), map_check_starts[this].back().id_data, me->_id);
        }

        // restore last status and confirmed it will be done in the handler
        // check builder
        me->set_last_status(last_status);
        me->set_status_confirmed(confirmed);
        me->set_current_attempt(current_attempt);
      }

      me->on_completion(
          check_running_index, _statuses[idx % _statuses.size()],
          com::centreon::common::perfdata::parse_perfdata(
              0, 0,
              "rta=0,031ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,109ms;;;; "
              "rtmin=0,011ms;;;;",
              _logger),
          {fmt::format("Command OK: {}", me->get_command_line())});

      idx++;
    });
  }
};

std::map<scripted_check*, std::vector<datapoint>>
    scripted_check::map_check_starts;
std::mutex scripted_check::check_m;

/* function that calculate if delta is about expected with a tolerance
 * return true if delta is in [expected - tol, expected + tol]
 * return false otherwise and log error
 */
static bool delta_about(const time_point& after,
                        const time_point& before,
                        std::chrono::milliseconds expected,
                        std::chrono::milliseconds tol) {
  const auto d =
      std::chrono::duration_cast<std::chrono::milliseconds>(after - before);
  if (d < (expected - tol) || d > (expected + tol)) {
    SPDLOG_ERROR(
        "delta={}ms not in [{}, {}]ms (expected ~{}ms) "
        "(after={}, before={})",
        d.count(), (expected - tol).count(), (expected + tol).count(),
        expected.count(), after, before);
    return false;
  }
  return true;
}

/* Test: retry uses retry_interval for soft failures until hard, then uses
  check_interval. Single service: check_interval=3s, retry_interval=1s,
  max_attempts=3. Expected deltas between starts: ~3s, ~1s, ~1s, ~3s. */
TEST_F(scheduler_test, retry_interval_is_used_until_hard_then_check_interval) {
  {
    std::lock_guard lk(scripted_check::check_m);
    scripted_check::map_check_starts.clear();
  }

  // Build config with one service and explicit retry + attempts
  auto conf = std::make_shared<com::centreon::agent::MessageToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_export_period(1);
  cnf->set_max_concurrent_checks(5);
  cnf->set_check_timeout(5);
  cnf->set_use_exemplar(true);

  auto* serv = cnf->add_services();
  serv->set_service_description("svc_retry_vs_normal");
  serv->set_command_name("cmd_retry_vs_normal");
  serv->set_command_line("cmdline");
  serv->set_check_interval(3);
  serv->set_retry_interval(1);
  serv->set_max_attempts(3);
  serv->set_host_id(1);
  serv->set_service_id(1);

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host", conf,
      []([[maybe_unused]] const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& req,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        // OK hard, CRIT soft1, CRIT soft1, CRIT hard, OK hard , OK hard
        std::vector<unsigned> status_script = {
            static_cast<unsigned>(e_status::ok),
            static_cast<unsigned>(e_status::critical),
            static_cast<unsigned>(e_status::critical),
            static_cast<unsigned>(e_status::critical),
            static_cast<unsigned>(e_status::ok),
            static_cast<unsigned>(e_status::ok)};
        return std::make_shared<scripted_check>(
            io_context, logger, start_expected, service, req,
            std::chrono::milliseconds(10), std::move(handler), stat,
            std::move(status_script));
      });

  scheduler_closer closer(sched);

  // Wait long enough to see 6 starts:
  //  start0, +~3s, +~1s, +~1s,+~3s, +~3s  => total ~11s (+ margins)
  std::this_thread::sleep_for(std::chrono::milliseconds(14000));

  std::vector<datapoint> starts_copy;
  {
    std::lock_guard lk(scripted_check::check_m);
    ASSERT_EQ(scripted_check::map_check_starts.size(), 1u);
    starts_copy = scripted_check::map_check_starts.begin()->second;
  }

  // print for each check the starting time

  for (size_t i = 0; i < starts_copy.size(); ++i) {
    SPDLOG_INFO(
        "check start[{}] id_data={} time={} status={} last_status={} "
        "confirmed={} current_attempt={}",
        i, starts_copy[i].id_data, starts_copy[i].time, starts_copy[i].status,
        starts_copy[i].last_status, starts_copy[i].confirmed,
        starts_copy[i].current_attempt);
  }

  ASSERT_GE(starts_copy.size(), 5u);
  // deltas: [~3s, ~1s, ~1s, ~3s, ~3s]
  ASSERT_PRED5(
      [](const time_point& a0, const time_point& a1, const time_point& a2,
         const time_point& a3, const time_point& a4) {
        return delta_about(a1, a0, std::chrono::milliseconds(3000),
                           std::chrono::milliseconds(600)) &&  // ok hard
               delta_about(a2, a1, std::chrono::milliseconds(1000),
                           std::chrono::milliseconds(
                               600)) &&  // retry #1 non soft 1 sec
               delta_about(
                   a3, a2, std::chrono::milliseconds(1000),
                   std::chrono::milliseconds(600))  // retry #2 hard 1 sec
               && delta_about(a4, a3, std::chrono::milliseconds(3000),
                              std::chrono::milliseconds(600));  //  no ok  HARD
      },
      starts_copy[0].time, starts_copy[1].time, starts_copy[2].time,
      starts_copy[3].time, starts_copy[4].time);
}

/**
 * multiple_services_intervals_are_respected
 *
 * Build a scheduler with 100 services that have different check_interval,
 * retry_interval and max_attempts. Each service uses scripted_check to
 * produce deterministic status sequences. The test runs long enough to
 * record multiple starts per service and verifies that the time between
 * consecutive starts equals either the service's check_interval (if the
 * previous result was confirmed) or its retry_interval otherwise, within
 * a 600 ms tolerance.
 */
TEST_F(scheduler_test, multiple_services_intervals_are_respected) {
  {
    std::lock_guard lk(scripted_check::check_m);
    scripted_check::map_check_starts.clear();
  }

  // Build config with one service and explicit retry + attempts
  auto conf = std::make_shared<com::centreon::agent::MessageToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_export_period(1);
  cnf->set_max_concurrent_checks(5);
  cnf->set_check_timeout(5);
  cnf->set_use_exemplar(true);

  for (int i = 0; i < 100; ++i) {
    auto* serv = cnf->add_services();
    serv->set_service_description(fmt::format("svc_{}", i));
    serv->set_command_name(fmt::format("cmd_{}", i));
    serv->set_command_line("/bin/true");
    serv->set_check_interval(3 + (i % 3));
    serv->set_retry_interval(1 + (i % 3));
    serv->set_max_attempts(3 + (i % 3));
    serv->set_host_id(1);
    serv->set_service_id(i + 1);
  }

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host", conf,
      []([[maybe_unused]] const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const Service& service,
         const engine_to_agent_request_ptr& req,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat,
         const std::shared_ptr<com::centreon::common::crypto::aes256>&) {
        std::vector<unsigned> status_script;
        // Three deterministic scenarios to exercise Nagios-like state
        // transitions:
        // scenario 0: OK -> WARNING -> CRITICAL -> CRITICAL -> CRITICAL -> OK
        // scenario 1: CRITICAL -> OK -> CRITICAL -> CRITICAL -> OK -> OK
        // scenario 2: UNKNOWN -> WARNING -> CRITICAL -> WARNING -> OK -> OK
        static const e_status scenario0[] = {
            e_status::ok,       e_status::warning,  e_status::critical,
            e_status::critical, e_status::critical, e_status::ok};
        static const e_status scenario1[] = {
            e_status::critical, e_status::ok, e_status::critical,
            e_status::critical, e_status::ok, e_status::ok};
        static const e_status scenario2[] = {
            e_status::unknown, e_status::warning, e_status::critical,
            e_status::warning, e_status::ok,      e_status::ok};

        // Choose scenario based on service id to distribute test cases across
        // services.
        const uint64_t sid = service.service_id() ? service.service_id() : 0;
        const e_status* choices = nullptr;
        switch (sid % 3) {
          case 0:
            choices = scenario0;
            break;
          case 1:
            choices = scenario1;
            break;
          default:
            choices = scenario2;
            break;
        }

        for (int i = 0; i < 6; ++i)
          status_script.push_back(static_cast<unsigned>(choices[i]));

        return std::make_shared<scripted_check>(
            io_context, logger, start_expected, service, req,
            std::chrono::milliseconds(10), std::move(handler), stat,
            std::move(status_script));
      });

  scheduler_closer closer(sched);

  // Wait long enough to see 6 starts:
  // worst case (3+3) * 6 = 36s (+ margins)
  std::this_thread::sleep_for(std::chrono::milliseconds(38000));

  std::map<scripted_check*, std::vector<datapoint>> starts_copy;
  {
    std::lock_guard lk(scripted_check::check_m);
    ASSERT_EQ(scripted_check::map_check_starts.size(), 100u);
    starts_copy = scripted_check::map_check_starts;
  }

  // check if the interval are respected for each service
  for (const auto& [check, starts] : starts_copy) {
    for (size_t i = 1; i < starts.size(); ++i) {
      uint64_t check_interval = check->get_check_interval_service();
      uint64_t retry_interval = check->get_retry_interval();

      // for debugging
      //  SPDLOG_INFO(
      //      "check {} start[{}] id_data={} time={} status={} last_status={} "
      //      "confirmed={} current_attempt={}/{}, check_interval={}s "
      //      ",check_retry={}s",
      //      check->get_service(), i, starts[i].id_data, starts[i].time,
      //      starts[i].status, starts[i].last_status, starts[i].confirmed,
      //      starts[i].current_attempt, check->get_max_attempts(),
      //      check_interval, retry_interval);

      ASSERT_TRUE(
          delta_about(starts[i].time, starts[i - 1].time,
                      starts[i - 1].confirmed
                          ? std::chrono::milliseconds(check_interval * 1000)
                          : std::chrono::milliseconds(retry_interval * 1000),
                      std::chrono::milliseconds(600)));
    }
  }
}