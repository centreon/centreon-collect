/*
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

#include "com/centreon/connector/ssh/reporter.hh"

#include <gtest/gtest.h>

#include "buffer_handle.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh;

TEST(SSHSession, CtorDefault) {
  // Object.
  reporter r;

  // Check.
  ASSERT_TRUE(r.can_report());
  ASSERT_TRUE(r.get_buffer().empty());
}

TEST(SSHSession, Error) {
  // Check result.
  checks::result cr;
  cr.set_command_id(42);

  // Buffer handle.
  buffer_handle bh;

  // Reporter.
  reporter r;
  r.send_result(cr);

  // Notify of error.
  ASSERT_THROW(r.error(bh), exceptions::basic);

  // Reporter cannot report anymore.
  ASSERT_FALSE(r.can_report());
}

TEST(SSHSession, SendResult) {
  // Check result.
  checks::result cr;
  cr.set_command_id(42);
  cr.set_executed(true);
  cr.set_exit_code(3);
  cr.set_output("this is my output");
  cr.set_error("some error might have occurred");

  // Reporter.
  reporter r;
  r.send_result(cr);

  // Buffer handle.
  buffer_handle bh;
  while (r.want_write(bh))
    r.write(bh);

  // Compare what reporter wrote with what is expected.
  char buffer
      [sizeof("3\00042\0001\0003\0some error might have occurred\0this is my "
              "output\0\0\0\0") -
       1];
  ASSERT_EQ(bh.read(buffer, sizeof(buffer)), sizeof(buffer));
  ASSERT_FALSE(
      memcmp(buffer,
             "3\00042\0001\0003\0some error might have occurred\0this is my "
             "output\0\0\0\0",
             sizeof(buffer)));
}

TEST(SSHSession, SendVersion) {
  // Reporter.
  reporter r;
  r.send_version(42, 84);

  // Buffer handle.
  buffer_handle bh;
  while (r.want_write(bh))
    r.write(bh);

  // Compare what reporter wrote with what is expected.
  char buffer[sizeof("1\00042\00084\0\0\0\0") - 1];
  ASSERT_EQ(bh.read(buffer, sizeof(buffer)), sizeof(buffer));
  ASSERT_FALSE(memcmp(buffer, "1\00042\00084\0\0\0\0", sizeof(buffer)));
}
