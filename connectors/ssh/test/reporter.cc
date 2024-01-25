/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/connector/reporter.hh"

#include <gtest/gtest.h>

#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector;

static shared_io_context io_context(std::make_shared<asio::io_context>());

class mock_reporter : public reporter {
 public:
  std::string sent;
  mock_reporter(const shared_io_context& io_context) : reporter(io_context) {}

  void write() override {
    if (!_buffer->empty()) {
      sent += *_buffer;
      _buffer = std::make_shared<std::string>();
    }
  }
};

TEST(SSHSession, CtorDefault) {
  // Object.
  reporter::pointer r = reporter::create(io_context);

  // Check.
  ASSERT_TRUE(r->can_report());
  ASSERT_TRUE(r->get_buffer().empty());
}

TEST(SSHSession, Error) {
  // Check result.
  result cr;
  cr.set_command_id(42);

  // Reporter.
  reporter::pointer r = reporter::create(io_context);
  r->send_result(cr);
  r->error();

  // Reporter cannot report anymore.
  ASSERT_FALSE(r->can_report());
}

TEST(SSHSession, SendResult) {
  // Check result.
  result cr;
  cr.set_command_id(42);
  cr.set_executed(true);
  cr.set_exit_code(3);
  cr.set_output("this is my output");
  cr.set_error("some error might have occurred");

  // Reporter.
  mock_reporter r(io_context);
  r.send_result(cr);

  const char* expected =
      "3\00042\0001\0003\0some error might have occurred\0this is my "
      "output\0\0\0\0";

  ASSERT_EQ(r.sent,
            std::string(expected,
                        expected +
                            sizeof("3\00042\0001\0003\0some error might have "
                                   "occurred\0this is my output\0\0\0\0") -
                            1));
}

TEST(SSHSession, SendVersion) {
  // Reporter.
  mock_reporter r(io_context);
  r.send_version(42, 84);

  const char* s = "1\00042\00084\0\0\0\0";
  ASSERT_EQ(r.sent, std::string(s, s + sizeof("1\00042\00084\0\0\0\0") - 1));
}
