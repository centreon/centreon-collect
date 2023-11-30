/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;

TEST(Exceptions, Throw) {
  // First throw.
  try {
    try {
      throw msg_fmt("{}{}{}", "foobar", 42, -789654ll);
      ASSERT_FALSE(true);
    } catch (msg_fmt const& e) {  // Properly caught.
      ASSERT_STREQ(e.what(), "foobar42-789654");
    }
  } catch (...) {
    ASSERT_FALSE(true);
  }

  // Second throw.
  try {
    try {
      throw msg_fmt("{}{}{}", "bazqux", -74125896321445ll, 36);
      ASSERT_FALSE(true);
    } catch (std::exception const& e) {
      ASSERT_STREQ(e.what(), "bazqux-7412589632144536");
    }
  } catch (...) {
    ASSERT_FALSE(true);
  }
}
