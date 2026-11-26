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
#include "common/crypto/aes256.hh"

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

class check_test : public testing::Test {
 public:
  Service serv;

  check_test() {
    serv.set_service_description("my_serv");
    serv.set_command_name("my_command_name");
    serv.set_command_line("my_command_line");
    serv.set_check_interval(2);
    serv.set_retry_interval(1);
    serv.set_max_attempts(3);
  }
};
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
  dummy_check(const Service& serv,
              const duration& command_duration,
              handler_type&& handler,
              check::command_executor executor = {})
      : check(g_io_context,
              spdlog::default_logger(),
              std::chrono::system_clock::now(),
              serv,
              nullptr,
              std::move(handler),
              std::make_shared<checks_statistics>(),
              nullptr,
              std::move(executor)),
        _command_duration(command_duration),
        _command_timer(*g_io_context) {}
};

TEST_F(check_test, timeout) {
  unsigned status = 0;
  std::string output;
  std::mutex cond_m;
  std::condition_variable cond;
  unsigned handler_call_cpt = 0;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(500),
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

TEST_F(check_test, no_timeout) {
  unsigned status = 0;
  std::string output;
  std::mutex cond_m;
  std::condition_variable cond;
  unsigned handler_call_cpt = 0;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(100),
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

// non ok soft  ->  ok soft -> ok hard
TEST_F(check_test, nagios_confirmation_and_retry_soft_recovery) {
  // prepare a dummy checker
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1),
      [](const std::shared_ptr<check>&, unsigned,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {});

  // simulate previous non-OK soft state
  checker->set_last_status(2);           // previous status was non-OK
  checker->set_status_confirmed(false);  // previous was NOT confirmed (soft)
  checker->set_current_attempt(1);
  ASSERT_EQ(checker->get_max_attempts(), 3);

  checker->calcul_status_confirmation(0);  // now OK

  // soft recovery: status should remain not confirmed, last_status updated to
  // OK
  EXPECT_FALSE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 0);
  EXPECT_EQ(checker->get_current_attempt(), 2);

  // further OK
  checker->calcul_status_confirmation(0);  // now OK
  // further OK: should be confirmed (hard) and last_status updated to OK
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 0);
  EXPECT_EQ(checker->get_current_attempt(), 1);
}

// non ok hard  ->  ok hard
TEST_F(check_test, nagios_confirmation_and_retry_hard_recovery) {
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1),
      [](const std::shared_ptr<check>&, unsigned,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {});

  // simulate previous non-OK hard state
  checker->set_last_status(2);          // previous status was non-OK
  checker->set_status_confirmed(true);  // previous was confirmed (hard)

  checker->calcul_status_confirmation(0);  // now OK

  // hard recovery: should be confirmed and last_status updated to OK
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 0);
}

// 1st check ok hard
TEST_F(check_test, first_check_ok_hard) {
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1),
      [](const std::shared_ptr<check>&, unsigned,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {});

  checker->calcul_status_confirmation(0);  // now OK

  // hard
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 0);
}

// 1st check Nok soft
TEST_F(check_test, first_check_nok_hard) {
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1),
      [](const std::shared_ptr<check>&, unsigned,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {});

  checker->calcul_status_confirmation(2);  // NOK

  // hard
  EXPECT_FALSE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 2);
}

// max attemps 3 : init ok hard -> non ok soft -> non ok soft -> non ok hard ->
// ok hard -> ok hard
TEST_F(check_test, nagios_confirmation_and_retry_problem_state) {
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1),
      [](const std::shared_ptr<check>&, unsigned,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {});

  // first problem: from initial state to non-OK
  checker->set_last_status(0);          // simulate first run before problem
  checker->set_status_confirmed(true);  // irrelevant but explicit

  checker->calcul_status_confirmation(2);  // non-OK

  // first non-OK: should be not confirmed (soft) and last_status updated to
  EXPECT_FALSE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 2);
  EXPECT_EQ(checker->get_current_attempt(), 1);

  // second problem: still non-OK
  checker->calcul_status_confirmation(2);  // non-OK
  // second non-OK: should be not confirmed (soft) and last_status updated to
  EXPECT_FALSE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 2);
  EXPECT_EQ(checker->get_current_attempt(), 2);

  // third problem: still non-OK
  checker->calcul_status_confirmation(2);  // non-OK
  // third non-OK: should be confirmed (hard) and last_status updated to
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 2);
  EXPECT_EQ(checker->get_current_attempt(), 3);

  // further problems: still non-OK
  checker->calcul_status_confirmation(2);  // non-OK
  // further non-OK: should be confirmed (hard) and last_status updated to
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 2);
  EXPECT_EQ(checker->get_current_attempt(), 3);  // should not increase

  // recovery to OK
  checker->calcul_status_confirmation(0);  // now OK
  // recovery from hard: should be confirmed (hard) and last_status updated to
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 0);
  EXPECT_EQ(checker->get_current_attempt(), 1);  // reset to 1

  // further OK
  checker->calcul_status_confirmation(0);  // now OK
  // further OK: should be confirmed (hard) and last_status updated to OK
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 0);
  EXPECT_EQ(checker->get_current_attempt(), 1);  // should not increase
}
// ---------------------------------------------------------------------------
// global event handler / event handler
// ---------------------------------------------------------------------------

// the completion handler of the tests below: nothing to wait, event handler
// commands are dispatched synchronously
static void empty_completion_handler(
    const std::shared_ptr<check>&,
    unsigned,
    const std::list<com::centreon::common::perfdata>&,
    const std::list<std::string>&) {}

// global event handler and event handler are both transmitted to the command
// executor, global one first, with the check as second argument
TEST_F(check_test, event_handler_order_global_then_local) {
  serv.set_service_id(4);
  serv.set_global_event_handler("global_hnd");
  serv.set_event_handler("local_hnd");

  std::vector<std::pair<std::string, const check*>> executed;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      [&](const std::string& cmd_line, const std::shared_ptr<check>& caller) {
        executed.emplace_back(cmd_line, caller.get());
      });

  // first check non-OK: raises both event handlers
  checker->calcul_status_confirmation(2);

  ASSERT_EQ(executed.size(), 2U);
  EXPECT_EQ(executed[0].first, "global_hnd");
  EXPECT_EQ(executed[1].first, "local_hnd");
  EXPECT_EQ(executed[0].second, checker.get());
  EXPECT_EQ(executed[1].second, checker.get());
}

// a service without event handler command never calls the executor
TEST_F(check_test, no_event_handler_no_executor_call) {
  serv.set_service_id(4);
  serv.set_global_event_handler("");
  serv.set_event_handler("");

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      []([[maybe_unused]] const std::string& cmd_line,
         [[maybe_unused]] const std::shared_ptr<check>& caller) {
        FAIL() << "event handler executor should not be called";
      });

  // non-OK then OK... no handler configured, so executor must stay silent
  checker->calcul_status_confirmation(2);
  checker->calcul_status_confirmation(2);
  checker->calcul_status_confirmation(0);
}

// an empty command executor doesn't break the check: the event handler state
// machine runs, no command is executed (no bad_function_call)
TEST_F(check_test, event_handler_without_executor) {
  serv.set_service_id(4);
  serv.set_global_event_handler("global_hnd");
  serv.set_event_handler("local_hnd");

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler);

  // first non-OK check from OK state: would raise the two event handlers
  EXPECT_NO_THROW(checker->calcul_status_confirmation(2));

  // state machine went on as expected
  EXPECT_FALSE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_last_status(), 2);
}

// on a host pseudo check (no service id), the HOSTSTATE family macros are
// substituted with the state of the check when the handler is raised
TEST_F(check_test, event_handler_host_state_macros) {
  serv.set_service_id(0);
  serv.set_global_event_handler(
      "restart $HOSTSTATE$ $HOSTSTATETYPE$ $HOSTATTEMPT$");

  std::vector<std::string> executed;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      [&](const std::string& cmd_line, const std::shared_ptr<check>&) {
        executed.push_back(cmd_line);
      });

  // first check non-OK from OK: soft down, attempt 1
  checker->calcul_status_confirmation(1);

  ASSERT_EQ(executed.size(), 1U);
  EXPECT_EQ(executed[0], "restart DOWN SOFT 1");
}

// on a service, the SERVICESTATE family macros are substituted
TEST_F(check_test, event_handler_service_state_macros) {
  serv.set_service_id(42);
  serv.set_event_handler(
      "notify $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$");

  std::vector<std::string> executed;

  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      [&](const std::string& cmd_line, const std::shared_ptr<check>&) {
        executed.push_back(cmd_line);
      });

  // first check critical from OK: soft critical, attempt 1
  checker->calcul_status_confirmation(2);
  ASSERT_EQ(executed.size(), 1U);
  EXPECT_EQ(executed[0], "notify CRITICAL SOFT 1");
}

// collects the command lines given to the command executor
struct executed_commands {
  std::vector<std::string> lines;

  check::command_executor executor() {
    return [this](const std::string& cmd_line, const std::shared_ptr<check>&) {
      lines.push_back(cmd_line);
    };
  }
};

// as in engine, a service event handler is raised on every SOFT problem
// result, on the switch to HARD and on the recovery, never while the state
// stays OK or HARD
TEST_F(check_test, event_handler_raised_on_state_transitions) {
  serv.set_service_id(4);
  serv.set_event_handler(
      "hnd $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  // first result OK: same state as the initial one, nothing raised
  checker->calcul_status_confirmation(0);
  EXPECT_TRUE(executed.lines.empty());

  checker->calcul_status_confirmation(2);  // SOFT 1
  checker->calcul_status_confirmation(2);  // SOFT 2
  checker->calcul_status_confirmation(2);  // HARD 3
  checker->calcul_status_confirmation(2);  // still HARD
  checker->calcul_status_confirmation(0);  // HARD recovery
  checker->calcul_status_confirmation(0);  // still OK

  std::vector<std::string> expected{"hnd CRITICAL SOFT 1",
                                    "hnd CRITICAL SOFT 2",
                                    "hnd CRITICAL HARD 3", "hnd OK HARD 1"};
  EXPECT_EQ(executed.lines, expected);
}

// with max_attempts = 1, the first problem is HARD at once and raises the
// event handler only once
TEST_F(check_test, event_handler_max_attempts_1_hard_at_once) {
  serv.set_service_id(4);
  serv.set_max_attempts(1);
  serv.set_event_handler("hnd $SERVICESTATE$ $SERVICESTATETYPE$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  checker->calcul_status_confirmation(2);
  EXPECT_TRUE(checker->get_status_confirmed());
  EXPECT_EQ(checker->get_current_attempt(), 1);

  checker->calcul_status_confirmation(2);  // still HARD
  checker->calcul_status_confirmation(0);  // HARD recovery

  std::vector<std::string> expected{"hnd CRITICAL HARD", "hnd OK HARD"};
  EXPECT_EQ(executed.lines, expected);
}

// a recovery during the SOFT attempts raises the event handler with a SOFT
// state type
TEST_F(check_test, event_handler_raised_on_soft_recovery) {
  serv.set_service_id(4);
  serv.set_event_handler("hnd $SERVICESTATE$ $SERVICESTATETYPE$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  checker->calcul_status_confirmation(1);  // WARNING SOFT 1
  checker->calcul_status_confirmation(0);  // SOFT recovery

  std::vector<std::string> expected{"hnd WARNING SOFT", "hnd OK SOFT"};
  EXPECT_EQ(executed.lines, expected);
}

// a host check uses the host macros: UP when the status is OK, DOWN otherwise
TEST_F(check_test, event_handler_host_recovery_is_up) {
  serv.set_service_id(0);
  serv.set_event_handler("hnd $HOSTSTATE$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  checker->calcul_status_confirmation(1);
  checker->calcul_status_confirmation(0);

  std::vector<std::string> expected{"hnd DOWN", "hnd UP"};
  EXPECT_EQ(executed.lines, expected);
}

// the macros of the other kind of object are left untouched: a host check
// doesn't replace the service macros and a service check doesn't replace the
// host ones
TEST_F(check_test, event_handler_other_object_macros_untouched) {
  serv.set_event_handler("hnd $HOSTSTATE$ $SERVICESTATE$");

  executed_commands host_executed;
  serv.set_service_id(0);
  std::shared_ptr<dummy_check> host_checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      host_executed.executor());
  host_checker->calcul_status_confirmation(1);
  ASSERT_EQ(host_executed.lines.size(), 1U);
  EXPECT_EQ(host_executed.lines[0], "hnd DOWN $SERVICESTATE$");

  executed_commands serv_executed;
  serv.set_service_id(4);
  std::shared_ptr<dummy_check> serv_checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      serv_executed.executor());
  serv_checker->calcul_status_confirmation(2);
  ASSERT_EQ(serv_executed.lines.size(), 1U);
  EXPECT_EQ(serv_executed.lines[0], "hnd $HOSTSTATE$ CRITICAL");
}

// engine keeps the state macros with their $ delimiters
// (engine/src/macros/process.cc, kept_macros): the whole tag is replaced
TEST_F(check_test, event_handler_dollar_delimited_macros) {
  serv.set_service_id(4);
  serv.set_event_handler(
      "/usr/lib/hnd $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  checker->calcul_status_confirmation(2);
  ASSERT_EQ(executed.lines.size(), 1U);
  EXPECT_EQ(executed.lines[0], "/usr/lib/hnd CRITICAL SOFT 1");
}

using aes256 = com::centreon::common::crypto::aes256;

static std::shared_ptr<aes256> create_test_crypto() {
  return std::make_shared<aes256>(
      "SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=", "U2FsdA==");
}

// a check that is never started, built with a decryption key
class crypto_check : public check {
 public:
  crypto_check(const Service& serv,
               const std::shared_ptr<aes256>& credentials_decrypt,
               check::command_executor&& executor)
      : check(g_io_context,
              spdlog::default_logger(),
              std::chrono::system_clock::now(),
              serv,
              nullptr,
              empty_completion_handler,
              std::make_shared<checks_statistics>(),
              credentials_decrypt,
              std::move(executor)) {}

  void start_check(const duration&) override {}
};

// an encrypted event handler is decrypted with the key given to the check,
// then its macros are replaced
TEST_F(check_test, encrypted_event_handler_is_decrypted) {
  std::shared_ptr<aes256> crypto = create_test_crypto();
  serv.set_service_id(4);
  serv.set_global_event_handler("encrypt::" +
                                crypto->encrypt("global $SERVICESTATE$"));
  serv.set_event_handler("encrypt::" + crypto->encrypt("local $SERVICESTATE$"));

  executed_commands executed;
  auto checker =
      std::make_shared<crypto_check>(serv, crypto, executed.executor());
  checker->calcul_status_confirmation(2);

  std::vector<std::string> expected{"global CRITICAL", "local CRITICAL"};
  EXPECT_EQ(executed.lines, expected);
}

// an event handler that cannot be decrypted is not executed, the other one
// still is
TEST_F(check_test, undecryptable_event_handler_is_not_executed) {
  serv.set_service_id(4);
  serv.set_global_event_handler("encrypt::not an aes256 content");
  serv.set_event_handler("local $SERVICESTATE$");

  executed_commands executed;
  auto checker = std::make_shared<crypto_check>(serv, create_test_crypto(),
                                                executed.executor());
  EXPECT_NO_THROW(checker->calcul_status_confirmation(2));

  std::vector<std::string> expected{"local CRITICAL"};
  EXPECT_EQ(executed.lines, expected);
  // the state machine is not disturbed by the failure
  EXPECT_EQ(checker->get_last_status(), 2);
  EXPECT_FALSE(checker->get_status_confirmed());
}

// decrypt_command_line only decrypts encrypt::-prefixed command lines, and
// only when a key is given
TEST(check_decrypt_command_line, prefix_and_key) {
  std::shared_ptr<aes256> crypto = create_test_crypto();
  const std::string encrypted = "encrypt::" + crypto->encrypt("/bin/hnd arg");

  std::string cmd = encrypted;
  check::decrypt_command_line(cmd, crypto);
  EXPECT_EQ(cmd, "/bin/hnd arg");

  cmd = "/bin/hnd arg";
  check::decrypt_command_line(cmd, crypto);
  EXPECT_EQ(cmd, "/bin/hnd arg");

  cmd = encrypted;
  check::decrypt_command_line(cmd, nullptr);
  EXPECT_EQ(cmd, encrypted);

  cmd = "encrypt::not an aes256 content";
  EXPECT_THROW(check::decrypt_command_line(cmd, crypto), std::exception);
}

// as in engine, a state change between two HARD problems raises the event
// handler
TEST_F(check_test, event_handler_raised_on_hard_state_change) {
  serv.set_service_id(4);
  serv.set_max_attempts(1);
  serv.set_event_handler("hnd $SERVICESTATE$ $SERVICESTATETYPE$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  checker->calcul_status_confirmation(2);  // CRITICAL HARD
  checker->calcul_status_confirmation(1);  // WARNING HARD
  checker->calcul_status_confirmation(1);  // still WARNING HARD

  std::vector<std::string> expected{"hnd CRITICAL HARD", "hnd WARNING HARD"};
  EXPECT_EQ(executed.lines, expected);
  EXPECT_TRUE(checker->get_status_confirmed());
}

// as in engine, a host status is used as is: 1 is DOWN, 2 is UNREACHABLE, and
// a change between them raises the event handler
TEST_F(check_test, event_handler_host_down_unreachable) {
  serv.set_service_id(0);
  serv.set_max_attempts(1);
  serv.set_event_handler("hnd $HOSTSTATE$ $HOSTSTATETYPE$");

  executed_commands executed;
  std::shared_ptr<dummy_check> checker = std::make_shared<dummy_check>(
      serv, std::chrono::milliseconds(1), empty_completion_handler,
      executed.executor());

  checker->calcul_status_confirmation(1);  // DOWN HARD
  checker->calcul_status_confirmation(2);  // UNREACHABLE HARD
  checker->calcul_status_confirmation(2);  // still UNREACHABLE HARD
  checker->calcul_status_confirmation(1);  // DOWN HARD
  checker->calcul_status_confirmation(0);  // UP HARD

  std::vector<std::string> expected{"hnd DOWN HARD", "hnd UNREACHABLE HARD",
                                    "hnd DOWN HARD", "hnd UP HARD"};
  EXPECT_EQ(executed.lines, expected);
}
