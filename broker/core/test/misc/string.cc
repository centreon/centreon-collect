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
#include "com/centreon/broker/misc/string.hh"

#include <absl/strings/str_split.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/common/utf8.hh"

using namespace com::centreon::broker::misc;

TEST(StringSplit, OnePart) {
  std::list<std::string> lst{absl::StrSplit("test", ' ')};
  ASSERT_EQ(lst.size(), 1u);
  ASSERT_EQ(lst.front(), "test");
}

TEST(StringSplit, ThreePart) {
  std::list<std::string> lst{absl::StrSplit("test foo bar", ' ')};
  ASSERT_EQ(lst.size(), 3u);
  std::list<std::string> res{"test", "foo", "bar"};
  ASSERT_EQ(lst, res);
}

TEST(StringSplit, ManyPart) {
  std::list<std::string> lst{
      absl::StrSplit("  test foo bar a b  c d eeeee", ' ')};
  ASSERT_EQ(lst.size(), 11u);
  std::list<std::string> res{"",  "", "test", "foo", "bar",  "a",
                             "b", "", "c",    "d",   "eeeee"};
  ASSERT_EQ(lst, res);
}

TEST(escape, simple) {
  ASSERT_EQ("Hello", string::escape("Hello", 10));
  ASSERT_EQ("Hello", string::escape("Hello", 5));
  ASSERT_EQ("Hel", string::escape("Hello", 3));
}

TEST(escape, utf8) {
  std::string str("告'警'数\\量");
  std::string res("告\\'警\\'数\\\\量");
  std::string res1(res);
  res1.resize(com::centreon::common::adjust_size_utf8(res, 10));
  ASSERT_EQ(res, string::escape(str, 20));
  ASSERT_EQ(res1, string::escape(str, 10));
}

TEST(escape, border) {
  std::string str("'abc'");
  std::string res("\\'abc");
  ASSERT_EQ(res, string::escape(str, 6));
}

TEST(escape, complexe) {
  std::string str(
      "toto | a=23\nbidon bidon bidon "
      "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooool bla bla bla");
  std::string res(
      "toto | a=23\nbidon bidon bidon "
      "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooool bla bla bla");
  ASSERT_EQ(string::escape(str, 255), res);
  std::string str1(
      "CRITICAL: Very "
      "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong "
      "chinese 告警数量 output puté! | '告警数量'=42\navé dé long ouput oçi "
      "还有中国人! Hái yǒu zhòng guó rén!");
  std::string res1(
      "CRITICAL: Very "
      "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong "
      "chinese 告警数量 output puté! | \\'告警数量\\'=42\navé dé long ouput "
      "oçi 还有中国人! H");
  ASSERT_EQ(string::escape(str1, 255), res1);
}

TEST(escape, quote1) {
  std::string str("''''''''''''''''''''");
  std::string res("\\'\\'\\'\\'\\'");
  ASSERT_EQ(string::escape(str, 10), res);
}

TEST(escape, quote2) {
  std::string str("\\\\\\\\\\");
  std::string res("\\\\\\\\\\\\\\\\");
  ASSERT_EQ(string::escape(str, 9), res);
}

TEST(escape, quote3) {
  std::string str("\\\\\\\\\\");
  std::string res("\\\\\\\\\\\\\\\\\\\\");
  ASSERT_EQ(string::escape(str, 10), res);
}
