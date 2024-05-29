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
              const std::string& hst,
              const std::string& serv,
              const std::string& cmd_name,
              const std::string& cmd_line,
              const engine_to_agent_request_ptr& cnf,
              int command_exit_status,
              duration completion_delay,
              check::completion_handler&& handler)
      : check(io_context,
              logger,
              exp,
              hst,
              serv,
              cmd_name,
              cmd_line,
              cnf,
              handler),
        _completion_timer(*io_context),
        _command_exit_status(command_exit_status),
        _completion_delay(completion_delay) {}

  void start_check(const duration& timeout) override {
    {
      std::lock_guard l(check_starts_m);
      check_starts.emplace_back(this, std::chrono::system_clock::now());
    }
    check::start_check(timeout);
    _completion_timer.expires_from_now(_completion_delay);
    _completion_timer.async_wait([me = shared_from_this(), this,
                                  check_running_index =
                                      _get_running_check_index()](
                                     const boost::system::error_code& err) {
      SPDLOG_TRACE("end of completion timer for host {}, serv {}", get_host(),
                   get_service());
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

class scheduler_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    spdlog::default_logger()->set_level(spdlog::level::trace);
  }

  std::shared_ptr<com::centreon::agent::EngineToAgent> create_conf(
      unsigned nb_host,
      unsigned nb_serv_by_host,
      unsigned second_check_period,
      unsigned second_export_period,
      unsigned max_concurent_check,
      unsigned second_check_timeout);
};

std::shared_ptr<com::centreon::agent::EngineToAgent>
scheduler_test::create_conf(unsigned nb_host,
                            unsigned nb_serv_by_host,
                            unsigned second_check_period,
                            unsigned second_export_period,
                            unsigned max_concurent_check,
                            unsigned second_check_timeout) {
  std::shared_ptr<com::centreon::agent::EngineToAgent> conf =
      std::make_shared<com::centreon::agent::EngineToAgent>();
  auto cnf = conf->mutable_config();
  cnf->set_second_check_interval(second_check_period);
  cnf->set_second_export_period(second_export_period);
  cnf->set_max_concurrent_checks(max_concurent_check);
  cnf->set_second_check_timeout(second_check_timeout);
  cnf->set_use_examplar(true);
  for (; nb_host; --nb_host) {
    auto hst = cnf->add_hosts();
    hst->set_host(fmt::format("host{}", nb_host));
    for (unsigned serv_index = 0; serv_index < nb_serv_by_host; ++serv_index) {
      auto serv = hst->add_services();
      serv->set_service_description(
          fmt::format("serv{}", nb_host * nb_serv_by_host + serv_index));
      serv->set_command_name(
          fmt::format("command{}", nb_host * nb_serv_by_host + serv_index));
      serv->set_command_line("/usr/bin/ls");
    }
  }
  return conf;
}

TEST_F(scheduler_test, no_config) {
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), scheduler::default_config(),
      [](const export_metric_request_ptr&) {},
      [](const std::shared_ptr<asio::io_context>&,
         const std::shared_ptr<spdlog::logger>&, time_point /* start expected*/,
         const std::string& /*host*/, const std::string& /*service*/,
         const std::string& /*cmd_name*/, const std::string& /*cmd_line*/,
         const engine_to_agent_request_ptr& /*engine to agent request*/,
         check::completion_handler&&) { return std::shared_ptr<check>(); });

  std::weak_ptr<scheduler> weak_shed(sched);
  sched.reset();

  // scheduler must be owned by asio
  ASSERT_TRUE(weak_shed.lock());

  weak_shed.lock()->stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_FALSE(weak_shed.lock());
}

TEST_F(scheduler_test, correct_schedule) {
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), create_conf(2, 10, 1, 1, 50, 1),
      [](const export_metric_request_ptr&) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const std::string& host,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, host, service, cmd_name,
            cmd_line, engine_to_agent_request, 0, std::chrono::milliseconds(50),
            std::move(handler));
      });

  {
    std::lock_guard l(tempo_check::check_starts_m);
    tempo_check::check_starts.clear();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // we have 2 * 10 = 20 checks spread over 1 second
  duration expected_interval = std::chrono::milliseconds(50);

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
        ASSERT_GT((check_time.second - previous.second),
                  expected_interval - std::chrono::milliseconds(1));
        ASSERT_LT((check_time.second - previous.second),
                  expected_interval + std::chrono::milliseconds(1));
      }
      previous = check_time;
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  {
    std::lock_guard l(tempo_check::check_starts_m);
    ASSERT_GE(tempo_check::check_starts.size(), 40);
    bool first = true;
    std::pair<tempo_check*, time_point> previous;
    for (const auto& check_time : tempo_check::check_starts) {
      if (first) {
        first = false;
      } else {
        ASSERT_NE(previous.first, check_time.first);
        ASSERT_TRUE((check_time.second - previous.second) >
                    expected_interval - std::chrono::milliseconds(1));
        ASSERT_TRUE((check_time.second - previous.second) <
                    expected_interval + std::chrono::milliseconds(1));
      }
      previous = check_time;
    }
  }

  sched->stop();
}

TEST_F(scheduler_test, time_out) {
  export_metric_request_ptr exported_request;
  std::condition_variable export_cond;
  time_point now = std::chrono::system_clock::now();
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), create_conf(1, 1, 1, 1, 1, 1),
      [&](const export_metric_request_ptr& req) {
        exported_request = req;
        export_cond.notify_all();
      },
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const std::string& host,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, host, service, cmd_name,
            cmd_line, engine_to_agent_request, 0,
            std::chrono::milliseconds(1500), std::move(handler));
      });
  std::mutex m;
  std::unique_lock l(m);
  export_cond.wait(l);

  uint64_t completion_time = tempo_check::completion_time;

  ASSERT_TRUE(exported_request);
  ASSERT_EQ(exported_request->resource_metrics_size(), 1);
  const ::opentelemetry::proto::metrics::v1::ResourceMetrics& res =
      exported_request->resource_metrics()[0];
  const auto& res_attrib = res.resource().attributes();
  ASSERT_EQ(res_attrib.size(), 2);
  ASSERT_EQ(res_attrib.at(0).key(), "host.name");
  ASSERT_EQ(res_attrib.at(0).value().string_value(), "host1");
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
  ASSERT_LE(data_point.time_unix_nano(), completion_time);
  ASSERT_GE(data_point.time_unix_nano(), completion_time - 100000000);

  sched->stop();
}

TEST_F(scheduler_test, correct_output_examplar) {
  export_metric_request_ptr exported_request;
  std::condition_variable export_cond;
  time_point now = std::chrono::system_clock::now();
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), create_conf(1, 2, 1, 2, 10, 1),
      [&](const export_metric_request_ptr& req) {
        exported_request = req;
        export_cond.notify_all();
      },
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const std::string& host,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler) {
        return std::make_shared<tempo_check>(
            io_context, logger, start_expected, host, service, cmd_name,
            cmd_line, engine_to_agent_request, 0, std::chrono::milliseconds(10),
            std::move(handler));
      });
  std::mutex m;
  std::unique_lock l(m);
  export_cond.wait(l);

  ASSERT_TRUE(exported_request);

  SPDLOG_INFO("export:{}", exported_request->DebugString());

  ASSERT_EQ(exported_request->resource_metrics_size(), 2);
  const ::opentelemetry::proto::metrics::v1::ResourceMetrics& res =
      exported_request->resource_metrics()[0];
  const auto& res_attrib = res.resource().attributes();
  ASSERT_EQ(res_attrib.size(), 2);
  ASSERT_EQ(res_attrib.at(0).key(), "host.name");
  ASSERT_EQ(res_attrib.at(0).value().string_value(), "host1");
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
      exported_request->resource_metrics()[1];
  const auto& res_attrib2 = res2.resource().attributes();
  ASSERT_EQ(res_attrib2.size(), 2);
  ASSERT_EQ(res_attrib2.at(0).key(), "host.name");
  ASSERT_EQ(res_attrib2.at(0).value().string_value(), "host1");
  ASSERT_EQ(res_attrib2.at(1).key(), "service.name");
  ASSERT_EQ(res_attrib2.at(1).value().string_value(), "serv3");
  ASSERT_EQ(res2.scope_metrics_size(), 1);

  const ::opentelemetry::proto::metrics::v1::ScopeMetrics& scope_metrics2 =
      res2.scope_metrics()[0];
  ASSERT_EQ(scope_metrics2.metrics_size(), 5);
  const ::opentelemetry::proto::metrics::v1::Metric metric2 =
      scope_metrics2.metrics()[0];
  ASSERT_EQ(metric2.name(), "status");
  ASSERT_EQ(metric2.description(), "Command OK: /usr/bin/ls");
  ASSERT_EQ(metric2.gauge().data_points_size(), 1);
  const auto& data_point_state2 = metric2.gauge().data_points()[0];
  ASSERT_EQ(data_point_state2.as_int(), 0);

  ASSERT_LE(first_time_point + 400000000, data_point_state2.time_unix_nano());
  ASSERT_GE(first_time_point + 600000000, data_point_state2.time_unix_nano());

  sched->stop();
}

class concurent_check : public check {
  asio::system_timer _completion_timer;
  int _command_exit_status;
  duration _completion_delay;

 public:
  static std::set<concurent_check*> checked;
  static std::set<concurent_check*> active_checks;
  static unsigned max_active_check;

  concurent_check(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  time_point exp,
                  const std::string& hst,
                  const std::string& serv,
                  const std::string& cmd_name,
                  const std::string& cmd_line,
                  const engine_to_agent_request_ptr& cnf,
                  int command_exit_status,
                  duration completion_delay,
                  check::completion_handler&& handler)
      : check(io_context,
              logger,
              exp,
              hst,
              serv,
              cmd_name,
              cmd_line,
              cnf,
              handler),
        _completion_timer(*io_context),
        _command_exit_status(command_exit_status),
        _completion_delay(completion_delay) {}

  void start_check(const duration& timeout) override {
    check::start_check(timeout);
    active_checks.insert(this);
    if (active_checks.size() > max_active_check) {
      max_active_check = active_checks.size();
    }
    _completion_timer.expires_from_now(_completion_delay);
    _completion_timer.async_wait([me = shared_from_this(), this,
                                  check_running_index =
                                      _get_running_check_index()](
                                     const boost::system::error_code& err) {
      active_checks.erase(this);
      checked.insert(this);
      SPDLOG_TRACE("end of completion timer for host {}, serv {}", get_host(),
                   get_service());
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

TEST_F(scheduler_test, max_concurent) {
  std::shared_ptr<scheduler> sched = scheduler::load(
      g_io_context, spdlog::default_logger(), create_conf(10, 20, 1, 1, 10, 1),
      [&](const export_metric_request_ptr& req) {},
      [](const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         time_point start_expected, const std::string& host,
         const std::string& service, const std::string& cmd_name,
         const std::string& cmd_line,
         const engine_to_agent_request_ptr& engine_to_agent_request,
         check::completion_handler&& handler) {
        return std::make_shared<concurent_check>(
            io_context, logger, start_expected, host, service, cmd_name,
            cmd_line, engine_to_agent_request, 0, std::chrono::milliseconds(75),
            std::move(handler));
      });

  // to many tests to be completed in one second
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  ASSERT_LT(concurent_check::checked.size(), 200);
  ASSERT_EQ(concurent_check::max_active_check, 10);

  // all tests must be completed in 1.5s
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_EQ(concurent_check::max_active_check, 10);
  ASSERT_EQ(concurent_check::checked.size(), 200);

  sched->stop();
}