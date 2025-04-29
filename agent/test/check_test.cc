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
#include <chrono>

#include "check.hh"

using namespace com::centreon::agent;

TEST(duration_from_str, values) {
  EXPECT_EQ(duration{0}, duration_from_string("", 's'));
  EXPECT_EQ(std::chrono::seconds(45), duration_from_string("45s", 'd'));
  EXPECT_EQ(std::chrono::seconds(45), duration_from_string("45", 's'));
  EXPECT_EQ(std::chrono::minutes(33), duration_from_string("33m", 'd'));
  EXPECT_EQ(std::chrono::minutes(33), duration_from_string("33", 'm'));
  EXPECT_EQ(std::chrono::hours(17), duration_from_string("17h", 'd'));
  EXPECT_EQ(std::chrono::hours(17), duration_from_string("17", 'h'));
  EXPECT_EQ(std::chrono::hours(13 * 24), duration_from_string("13d", 'd'));
  EXPECT_EQ(std::chrono::hours(13 * 24), duration_from_string("13d", 'h'));
  EXPECT_EQ(std::chrono::hours(13 * 24), duration_from_string("13", 'd'));
  EXPECT_EQ(std::chrono::hours(11 * 7 * 24), duration_from_string("11w", 'd'));
  EXPECT_EQ(std::chrono::hours(11 * 7 * 24), duration_from_string("11", 'w'));
  EXPECT_EQ(std::chrono::hours(-11 * 7 * 24), duration_from_string("-11", 'w'));
  EXPECT_EQ(std::chrono::hours(-11 * 7 * 24),
            duration_from_string("-11w", 'd'));
  EXPECT_EQ(std::chrono::hours(11 * 7 * 24),
            duration_from_string("-11w", 'd', true));
  EXPECT_EQ(std::chrono::hours(7 * 24) + std::chrono::hours(3 * 24) +
                std::chrono::hours(13) + std::chrono::minutes(5) +
                std::chrono::seconds(30),
            duration_from_string("1w3d13h5m30", 's'));
}

extern std::shared_ptr<asio::io_context> g_io_context;

class dummy_check : public check {
  duration _command_duration;
  asio::system_timer _command_timer;

 public:
  void start_check(const duration& timeout) override {
    if (!_start_check(timeout)) {
      return;
    }
    _command_timer.expires_after(_command_duration);
    _command_timer.async_wait([me = shared_from_this(), this,
                               running_index = _get_running_check_index()](
                                  const boost::system::error_code& err) {
      if (err) {
        return;
      }
      on_completion(running_index, 1,
                    std::list<com::centreon::common::perfdata>(),
                    {"output dummy_check of " + get_command_line()});
    });
  }

  template <typename handler_type>
  dummy_check(const std::string& serv,
              const std::string& command_name,
              const std::string& command_line,
              const duration& command_duration,
              handler_type&& handler)
      : check(g_io_context,
              spdlog::default_logger(),
              std::chrono::system_clock::now(),
              std::chrono::milliseconds(10),
              std::chrono::seconds(1),
              serv,
              command_name,
              command_line,
              nullptr,
              handler,
              std::make_shared<checks_statistics>()),
        _command_duration(command_duration),
        _command_timer(*g_io_context) {}
};

static std::string serv("my_serv");
static std::string cmd_name("my_command_name");
static std::string cmd_line("my_command_line");

TEST(check_test, timeout) {
  unsigned status = 0;
  std::string output;
  std::mutex cond_m;
  std::condition_variable cond;
  unsigned handler_call_cpt = 0;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, cmd_name, cmd_line, std::chrono::milliseconds(500),
      [&status, &output, &handler_call_cpt, &cond](
          const std::shared_ptr<check>&, unsigned statuss,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          const std::list<std::string>& outputs) {
        status = statuss;
        if (outputs.size() == 1) {
          output = *outputs.begin();
        }
        ++handler_call_cpt;
        cond.notify_all();
      });

  checker->start_check(std::chrono::milliseconds(100));

  std::unique_lock l(cond_m);
  cond.wait(l);

  ASSERT_EQ(status, 3);
  ASSERT_EQ(handler_call_cpt, 1);
  ASSERT_EQ(output, "Timeout at execution of my_command_line");

  // completion handler not called twice
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_EQ(status, 3);
  ASSERT_EQ(handler_call_cpt, 1);
  ASSERT_EQ(output, "Timeout at execution of my_command_line");
}

TEST(check_test, no_timeout) {
  unsigned status = 0;
  std::string output;
  std::mutex cond_m;
  std::condition_variable cond;
  unsigned handler_call_cpt = 0;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, cmd_name, cmd_line, std::chrono::milliseconds(100),
      [&status, &output, &handler_call_cpt, &cond](
          const std::shared_ptr<check>&, unsigned statuss,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          const std::list<std::string>& outputs) {
        status = statuss;
        if (outputs.size() == 1) {
          output = *outputs.begin();
        }
        ++handler_call_cpt;
        cond.notify_all();
      });

  checker->start_check(std::chrono::milliseconds(200));

  std::unique_lock l(cond_m);
  cond.wait(l);

  ASSERT_EQ(status, 1);
  ASSERT_EQ(handler_call_cpt, 1);
  ASSERT_EQ(output, "output dummy_check of my_command_line");

  // completion handler not called twice
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_EQ(status, 1);
  ASSERT_EQ(handler_call_cpt, 1);
  ASSERT_EQ(output, "output dummy_check of my_command_line");
}