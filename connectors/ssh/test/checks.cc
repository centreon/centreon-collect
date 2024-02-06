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

#include <gtest/gtest.h>

#include "com/centreon/connector/result.hh"
#include "com/centreon/connector/ssh/checks/check.hh"

using namespace com::centreon::connector;
using namespace com::centreon::connector::ssh;
using namespace com::centreon::connector::ssh::checks;

TEST(SSHChecks, ResultAssign) {
  // Base object.
  result r1;
  r1.set_command_id(14598753ull);
  r1.set_error("a random error string");
  r1.set_executed(true);
  r1.set_exit_code(-46582);
  r1.set_output("another random string, but for the output property");

  // Copied object.
  result r2;
  r2 = r1;

  // Reset base object.
  r1.set_command_id(42);
  r1.set_error("foo bar");
  r1.set_executed(false);
  r1.set_exit_code(7536);
  r1.set_output("baz qux");

  ASSERT_EQ(r1.get_command_id(), 42u);
  ASSERT_EQ(r1.get_error(), "foo bar");
  ASSERT_EQ(r1.get_executed(), false);
  ASSERT_EQ(r1.get_exit_code(), 7536);
  ASSERT_EQ(r1.get_output(), "baz qux");
  ASSERT_EQ(r2.get_command_id(), 14598753ull);
  ASSERT_EQ(r2.get_error(), "a random error string");
  ASSERT_EQ(r2.get_executed(), true);
  ASSERT_EQ(r2.get_exit_code(), -46582);
  ASSERT_EQ(r2.get_output(),
            "another random string, but for the output property");
}

TEST(SSHChecks, CommandId) {
  // Object.
  result r;

  // Checks.
  r.set_command_id(71184);
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(r.get_command_id(), 71184u);
  r.set_command_id(15);
  ASSERT_EQ(r.get_command_id(), 15u);
  r.set_command_id(741258963148368872ull);
  for (unsigned int i = 0; i < 10000; ++i)
    ASSERT_EQ(r.get_command_id(), 741258963148368872ull);
}

TEST(SSHChecks, CtorCopy) {
  // Base object.
  result r1;
  r1.set_command_id(14598753ull);
  r1.set_error("a random error string");
  r1.set_executed(true);
  r1.set_exit_code(-46582);
  r1.set_output("another random string, but for the output property");

  // Copied object.
  result r2(r1);

  // Reset base object.
  r1.set_command_id(42);
  r1.set_error("foo bar");
  r1.set_executed(false);
  r1.set_exit_code(7536);
  r1.set_output("baz qux");

  // Check content.
  ASSERT_EQ(r1.get_command_id(), 42u);
  ASSERT_EQ(r1.get_error(), "foo bar");
  ASSERT_EQ(r1.get_executed(), false);
  ASSERT_EQ(r1.get_exit_code(), 7536);
  ASSERT_EQ(r1.get_output(), "baz qux");
  ASSERT_EQ(r2.get_command_id(), 14598753ull);
  ASSERT_EQ(r2.get_error(), "a random error string");
  ASSERT_EQ(r2.get_executed(), true);
  ASSERT_EQ(r2.get_exit_code(), -46582);
  ASSERT_EQ(r2.get_output(),
            "another random string, but for the output property");
}

TEST(SSHChecks, CtorDefault) {
  // Object.
  result r;

  // Check.
  ASSERT_EQ(r.get_command_id(), 0u);
  ASSERT_EQ(r.get_error().empty(), true);
  ASSERT_EQ(r.get_executed(), false);
  ASSERT_EQ(r.get_exit_code(), -1);
  ASSERT_EQ(r.get_output().empty(), true);
}

TEST(SSHChecks, Error) {
  // Object.
  result r;

  // Checks.
  r.set_error("this is the first string");
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(r.get_error(), "this is the first string");
  r.set_error("this string might be longer");
  ASSERT_EQ(r.get_error(), "this string might be longer");
  r.set_error(
      "this is the last string that makes Centreon SSH Connector rocks !");
  for (unsigned int i = 0; i < 10000; ++i)
    ASSERT_EQ(
        r.get_error(),
        "this is the last string that makes Centreon SSH Connector rocks !");
}

TEST(SSHChecks, Executed) {
  // Object.
  result r;

  // Checks.
  r.set_executed(false);
  ASSERT_EQ(r.get_executed(), false);
  r.set_executed(true);
  for (unsigned int i = 0; i < 10000; ++i)
    ASSERT_EQ(r.get_executed(), true);
  r.set_executed(false);
  for (unsigned int i = 0; i < 10000; ++i)
    ASSERT_EQ(r.get_executed(), false);
}

TEST(SSHChecks, ExitCode) {
  // Object.
  result r;

  // Checks.
  r.set_exit_code(71184);
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(r.get_exit_code(), 71184);
  r.set_exit_code(3);
  ASSERT_EQ(r.get_exit_code(), 3);
  r.set_exit_code(-47829);
  for (unsigned int i = 0; i < 10000; ++i)
    ASSERT_EQ(r.get_exit_code(), -47829);
}

TEST(SSHChecks, Output) {
  // Object.
  result r;

  // Checks.
  r.set_output("this is the first string");
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(r.get_output(), "this is the first string");
  r.set_output("this string might be longer");
  ASSERT_EQ(r.get_output(), "this string might be longer");
  r.set_output(
      "this is the last string that makes Centreon SSH Connector rocks !");
  for (unsigned int i = 0; i < 10000; ++i)
    ASSERT_EQ(
        r.get_output(),
        "this is the last string that makes Centreon SSH Connector rocks !");
}
