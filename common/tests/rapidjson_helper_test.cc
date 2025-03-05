/**
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <fstream>

#include <absl/strings/numbers.h>

#include "com/centreon/exceptions/msg_fmt.hh"

#include "rapidjson_helper.hh"

using namespace com::centreon;
using namespace com::centreon::common;

#ifdef _WIN32
#define JSON_FILE_PATH "C:/Users/Public/toto.json"
#else
#define JSON_FILE_PATH "/tmp/toto.json"
#endif

TEST(rapidjson_helper_test, unknown_file) {
  ::unlink(JSON_FILE_PATH);
  ASSERT_THROW(rapidjson_helper::read_from_file(JSON_FILE_PATH),
               exceptions::msg_fmt);
}

TEST(rapidjson_helper_test, bad_file) {
  ::unlink(JSON_FILE_PATH);

  std::ofstream oss(JSON_FILE_PATH);
  oss << "fkjsdgheirgiergegeg";
  oss.close();
  ASSERT_THROW(rapidjson_helper::read_from_file(JSON_FILE_PATH),
               exceptions::msg_fmt);
}

TEST(rapidjson_helper_test, good_file) {
  ::unlink(JSON_FILE_PATH);

  std::ofstream oss(JSON_FILE_PATH);
  oss << R"(
{
    "int_val":5,
    "s_int_val": "5",
    "double":3.14,
    "s_double": "3.14",
    "double_1": 121345678,
    "double_2": -123
}
)";
  oss.close();
  auto json_doc = rapidjson_helper::read_from_file(JSON_FILE_PATH);
  rapidjson_helper test(json_doc);

  ASSERT_EQ(5, test.get_int("int_val"));
  ASSERT_EQ(5, test.get_int("s_int_val"));
  ASSERT_EQ(3.14, test.get_double("double"));
  ASSERT_EQ(3.14, test.get_double("s_double"));
  ASSERT_EQ(121345678, test.get_double("double_1"));
  ASSERT_EQ(-123, test.get_double("double_2"));
}

TEST(rapidjson_helper_test, bad_array) {
  ::unlink(JSON_FILE_PATH);

  std::ofstream oss(JSON_FILE_PATH);
  oss << R"(
{
    "toto": 5
}
)";
  oss.close();
  auto json_doc = rapidjson_helper::read_from_file(JSON_FILE_PATH);
  rapidjson_helper test(json_doc);
  ASSERT_THROW(test.begin(), exceptions::msg_fmt);
}

TEST(rapidjson_helper_test, good_array) {
  auto json_doc = rapidjson_helper::read_from_string(R"(
[
    {
        "toto":1
    }
]
)");
  rapidjson_helper test(json_doc);
  ASSERT_NO_THROW(test.begin());
}
