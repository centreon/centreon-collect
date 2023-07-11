/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <gtest/gtest.h>

#include "com/centreon/common/hex_dump.hh"

using namespace com::centreon::common;

TEST(hex_dump, simple) {
  std::string ret =
      hex_dump(reinterpret_cast<const unsigned char*>("0123456789J"), 11, 0);
  ASSERT_EQ(ret, "303132333435363738394A");
}

TEST(hex_dump, short_string) {
  std::string ret =
      hex_dump(reinterpret_cast<const unsigned char*>("0123456789J"), 11, 4);
  ASSERT_EQ(ret, "0 30313233 0123\n4 34353637 4567\n8 38394A   89J\n");
}

TEST(hex_dump, long_string) {
  std::string ret = hex_dump(reinterpret_cast<const unsigned char*>(
                                 "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
                             36, 16);
  ASSERT_EQ(ret,
            "00 30313233343536373839414243444546 0123456789ABCDEF\n\
10 4748494A4B4C4D4E4F50515253545556 GHIJKLMNOPQRSTUV\n\
20 5758595A                         WXYZ\n");
}