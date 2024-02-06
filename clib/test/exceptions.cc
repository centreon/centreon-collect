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
#include <limits.h>
#include <sstream>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::exceptions;

TEST(ClibException, BasicCtor) {
  const unsigned int line = __LINE__;

  basic ex1(__FILE__, __func__, line);  // __func__ <-- FUNCTION

  std::ostringstream oss;
  oss << "[" << __FILE__ << ":" << line << "(" << __func__
      << ")] ";  // __func__ <-- FUNCTION
  ASSERT_STREQ(ex1.what(), oss.str().c_str());
}

TEST(ClibException, BasicCopy) {
  static char const message[] = "Centreon Clib";
  const unsigned int line = __LINE__;

  basic ex(__FILE__, __func__, line);  // __func__ <-- FUNCTION
  ex << message;

  std::ostringstream oss;
  oss << "[" << __FILE__ << ":" << line << "(" << __func__ << ")] " << message;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}

TEST(ClibException, BasicInsertChar) {
  basic ex;
  ex << static_cast<char>(CHAR_MIN);
  ex << static_cast<char>(CHAR_MAX);

  char ref[] = {CHAR_MIN, CHAR_MAX, 0};
  ASSERT_STREQ(ex.what(), ref);
}

TEST(ClibException, BasicInsertInt) {
  basic ex;
  ex << static_cast<int>(INT_MIN);
  ex << static_cast<int>(INT_MAX);

  std::ostringstream oss;
  oss << INT_MIN << INT_MAX;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}

TEST(ClibException, BasicInsertLong) {
  basic ex;
  ex << static_cast<long>(LONG_MIN);
  ex << static_cast<long>(LONG_MAX);

  std::ostringstream oss;
  oss << LONG_MIN << LONG_MAX;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}

TEST(ClibException, BasicInsertLongLong) {
  basic ex;
  ex << static_cast<long long>(LLONG_MIN);
  ex << static_cast<long long>(LLONG_MAX);

  std::ostringstream oss;
  oss << LLONG_MIN << LLONG_MAX;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}

TEST(ClibException, BasicInsertPChar) {
  basic ex;
  ex << __FILE__;
  ASSERT_STREQ(ex.what(), __FILE__);
}

TEST(ClibException, BasicInsertUInt) {
  basic ex;
  ex << static_cast<unsigned int>(0);
  ex << static_cast<unsigned int>(UINT_MAX);

  std::ostringstream oss;
  oss << 0 << UINT_MAX;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}

TEST(ClibException, BasicInsertULong) {
  basic ex;
  ex << static_cast<unsigned long>(0);
  ex << static_cast<unsigned long>(ULONG_MAX);

  std::ostringstream oss;
  oss << 0 << ULONG_MAX;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}

TEST(ClibException, BasicInsertULongLong) {
  basic ex;
  ex << static_cast<unsigned long long>(0);
  ex << static_cast<unsigned long long>(ULLONG_MAX);

  std::ostringstream oss;
  oss << 0 << ULLONG_MAX;
  ASSERT_STREQ(ex.what(), oss.str().c_str());
}
