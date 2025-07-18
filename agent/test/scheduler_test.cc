/**
 * Copyright 2024 Centreon
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
              duration check_interval,
              const std::string& serv,
              const std::string& cmd_name,
              const std::string& cmd_line,
              const engine_to_agent_request_ptr& cnf,
              int command_exit_status,
              duration completion_delay,
              check::completion_handler&& handler,
              const checks_statistics::pointer& stat)
      : check(io_context,
              logger,
              exp,
              check_interval,
              serv,
              cmd_name,
              cmd_line,
              cnf,
              std::move(handler),
              stat),
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
  }
  return conf;
}

TEST_F(scheduler_test, no_config) {
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host",
      scheduler::default_config(),
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>&,
         const std::shared_ptr<spdlog::logger>&, time_point /* start expected*/,
         duration /* check interval */, const std::string& /*service*/,
         const std::string& /*cmd_name*/, const std::string& /*cmd_line*/,
         const engine_to_agent_request_ptr& /*engine to agent request*/,
         check::completion_handler&&, const checks_statistics::pointer&) {
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
  std::chrono::milliseconds expected_interval(min_second_check_interval * 1000 /
                                              20);
  if ((after - before) <= expected_interval - std::chrono::milliseconds(250)) {
    SPDLOG_ERROR("after={}, before={} min_second_check_interval={}", after,
                 before, min_second_check_interval);
    return false;
  }
  if ((after - before) >= expected_interval + std::chrono::milliseconds(250)) {
    SPDLOG_ERROR("after={}, before={} min_second_check_interval={}", after,
                 before, min_second_check_interval);
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
         time_point start_expected, duration check_interval,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, check_interval, service,
            cmd_name, cmd_line, engine_to_agent_request, 0,
            std::chrono::milliseconds(50), std::move(handler), stat);
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
    if (serv->check_interval() < min_interval) {
      min_interval = serv->check_interval();
    }
  }

  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), "my_host", conf,
      [](const std::shared_ptr<MessageFromAgent>&) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, duration check_interval,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, check_interval, service,
            cmd_name, cmd_line, engine_to_agent_request, 0,
            std::chrono::milliseconds(50), std::move(handler), stat);
      });

  scheduler_closer closer(sched);

  std::this_thread::sleep_for(std::chrono::milliseconds(6100));

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
        // check if we have a delay of 250ms between two checks
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
         time_point start_expected, duration check_interval,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, check_interval, service,
            cmd_name, cmd_line, engine_to_agent_request, 0,
            std::chrono::milliseconds(1500), std::move(handler), stat);
      });
  scheduler_closer closer(sched);

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
         time_point start_expected, duration check_interval,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, check_interval, service,
            cmd_name, cmd_line, engine_to_agent_request, 0,
            std::chrono::milliseconds(10), std::move(handler), stat);
      });
  scheduler_closer closer(sched);

  std::mutex m;
  std::unique_lock l(m);
  export_cond.wait(l);

  ASSERT_TRUE(exported_request);

  SPDLOG_INFO("export:{}", exported_request->otel_request().ShortDebugString());

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
                  duration check_interval,
                  const std::string& serv,
                  const std::string& cmd_name,
                  const std::string& cmd_line,
                  const engine_to_agent_request_ptr& cnf,
                  int command_exit_status,
                  duration completion_delay,
                  check::completion_handler&& handler,
                  const checks_statistics::pointer& stat)
      : check(io_context,
              logger,
              exp,
              check_interval,
              serv,
              cmd_name,
              cmd_line,
              cnf,
              std::move(handler),
              stat),
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
         time_point start_expected, duration check_interval,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler,
         const checks_statistics::pointer& stat) {
        return std::make_shared<concurent_check>(
            io_context, logger, start_expected, check_interval, service,
            cmd_name, cmd_line, engine_to_agent_request, 0,
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
