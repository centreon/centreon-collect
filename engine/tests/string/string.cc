/**
 * Copyright 2020-2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/string.hh"

#include "gtest/gtest.h"

using namespace com::centreon::engine;

TEST(string_utils, extractPerfdataSimple) {
  std::string perfdata(
      "metric_2=2;3;7;1;9 metric=12;25;50;0;118 metric_1=28;13;54;0;80");
  ASSERT_EQ(string::extract_perfdata(perfdata, "metric"),
            "metric=12;25;50;0;118");
}

TEST(string_utils, extractPerfdataQuotes) {
  std::string perfdata(
      "'aa a aa'=2;3;7;1;9 'a aa'=12;25;50;0;118 'aa a'=28;13;54;0;80");
  ASSERT_EQ(string::extract_perfdata(perfdata, "a aa"),
            "'a aa'=12;25;50;0;118");
  ASSERT_EQ(string::extract_perfdata(perfdata, "aa a"), "'aa a'=28;13;54;0;80");
}

TEST(string_utils, extractPerfdataGaugeDiff) {
  std::string perfdata(
      "'aa a aa'=2;3;7;1;9 g[a aa]=12;25;50;0;118 d[aa a]=28;13;54;0;80");
  ASSERT_EQ(string::extract_perfdata(perfdata, "a aa"),
            "g[a aa]=12;25;50;0;118");
  ASSERT_EQ(string::extract_perfdata(perfdata, "aa a"),
            "d[aa a]=28;13;54;0;80");
}

TEST(string_utils, extractPerfdataBrackets) {
  std::string perfdata(
      "'xx[aa a aa]'=2;3;7;1;9 '[a aa]'=12;25;50;0;118 'aa a]'=28;13;54;0;80");
  ASSERT_EQ(string::extract_perfdata(perfdata, "xx[aa a aa]"),
            "'xx[aa a aa]'=2;3;7;1;9");
  ASSERT_EQ(string::extract_perfdata(perfdata, "[a aa]"),
            "'[a aa]'=12;25;50;0;118");
  ASSERT_EQ(string::extract_perfdata(perfdata, "aa a]"),
            "'aa a]'=28;13;54;0;80");
}

TEST(string_utils, removeThresholdsWithoutThresholds) {
  std::string perfdata("a=2V");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
}

TEST(string_utils, removeThresholdsWithoutThresholds2) {
  std::string perfdata("a=2V;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
}

TEST(string_utils, removeThresholdsWithoutThresholds3) {
  std::string perfdata("a=2V;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
}

TEST(string_utils, removeThresholdsWithOneThreshold) {
  std::string perfdata("a=2V;5");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
}

TEST(string_utils, removeThresholdsWithOneThreshold2) {
  std::string perfdata("a=2V;5;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
}

TEST(string_utils, removeThresholdsWithTwoThresholds1) {
  std::string perfdata("a=2V;5;9");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V");
}

TEST(string_utils, removeThresholdsWithTwoThresholds2) {
  std::string perfdata("a=2V;5;9;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V;;;");
}

TEST(string_utils, removeThresholdsWithTwoThresholds3) {
  std::string perfdata("a=2V;;9;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V;;;");
}

TEST(string_utils, removeThresholdsWithTwoThresholds4) {
  std::string perfdata("a=2V;;;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V;;;");
}

TEST(string_utils, removeThresholdsMoreComplex) {
  std::string perfdata("a=2V;5;9;0;10");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V;;;0;10");
}

TEST(string_utils, removeThresholdsMoreComplex2) {
  std::string perfdata("a=2V;5;9;0;");
  ASSERT_EQ(string::remove_thresholds(perfdata), "a=2V;;;0;");
}

TEST(string_utils, c_strtok_test1) {
  string::c_strtok parse("toto;;titi|tata\n");
  std::string_view v;
  ASSERT_TRUE(parse.extract(';', v));
  ASSERT_EQ(v, "toto");
  ASSERT_TRUE(parse.extract(';', v));
  ASSERT_EQ(v, "");
  ASSERT_TRUE(parse.extract('|', v));
  ASSERT_EQ(v, "titi");
  ASSERT_TRUE(parse.extract('\n', v));
  ASSERT_EQ(v, "tata");
  ASSERT_TRUE(parse.extract('*', v));
  ASSERT_EQ(v, "");
  ASSERT_FALSE(parse.extract('*', v));
}

TEST(string_utils, c_strtok_test2) {
  string::c_strtok parse("toto;;titi|tata\n");
  std::string_view v;
  ASSERT_TRUE(parse.extract(';', v));
  ASSERT_EQ(v, "toto");
  ASSERT_TRUE(parse.extract('&', v));
  ASSERT_EQ(v, ";titi|tata\n");
  ASSERT_FALSE(parse.extract('\n', v));
}

TEST(string_utils, c_strtok_test3) {
  string::c_strtok parse("|toto;;titi|tata\n");
  std::string_view v;
  ASSERT_TRUE(parse.extract('|', v));
  ASSERT_EQ(v, "");
  ASSERT_TRUE(parse.extract('|', v));
  ASSERT_EQ(v, "toto;;titi");
  ASSERT_TRUE(parse.extract('|', v));
  ASSERT_EQ(v, "tata\n");
  ASSERT_FALSE(parse.extract('\n', v));
}

TEST(string_utils, c_strtok_test4) {
  string::c_strtok parse("toto");
  std::string_view v;
  ASSERT_TRUE(parse.extract('|', v));
  ASSERT_EQ(v, "toto");
  ASSERT_FALSE(parse.extract('\n', v));
}

TEST(string_utils, c_strtok_test5) {
  string::c_strtok parse("1");
  std::string_view v;
  int val;
  ASSERT_TRUE(parse.extract(';', val));
  ASSERT_EQ(val, 1);
  ASSERT_FALSE(parse.extract(';', v));
}

TEST(string_utils, c_strtok_test6) {
  string::c_strtok parse("toto1");
  std::string_view v;
  int val;
  ASSERT_FALSE(parse.extract(';', val));
  ASSERT_FALSE(parse.extract(';', v));
}

TEST(string_utils, c_strtok_test7) {
  string::c_strtok parse("toto;1");
  std::string_view v;
  ASSERT_TRUE(parse.extract(';', v));
  ASSERT_EQ(v, "toto");
  int val;
  ASSERT_TRUE(parse.extract(';', val));
  ASSERT_EQ(val, 1);
  ASSERT_FALSE(parse.extract(';', v));
}

/**
 * @brief The std::string flavour of extract(), used when the caller needs to
 * keep the field beyond the lifetime of the parsed buffer.
 */
TEST(string_utils, c_strtok_to_string) {
  string::c_strtok parse("toto;titi");
  std::string s;
  ASSERT_TRUE(parse.extract(';', s));
  ASSERT_EQ(s, "toto");
  ASSERT_TRUE(parse.extract(';', s));
  ASSERT_EQ(s, "titi");
  ASSERT_FALSE(parse.extract(';', s));
}

TEST(string_utils, unescape) {
  std::string str = "az\\ner\\nty\\n";
  string::unescape(str);
  ASSERT_EQ(str, "az\ner\nty\n");
}

TEST(string_utils, unescape_tab) {
  std::string str = "az\\ter\\tty\\n";
  string::unescape(str);
  ASSERT_EQ(str, "az\ter\tty\n");
}

TEST(string_utils, unescape_trailing_backslash) {
  std::string str = "azerty\\";
  string::unescape(str);
  ASSERT_EQ(str, "azerty\\");
}

TEST(string_utils, unescape_trailing_backslash2) {
  std::string str = "az\\nerty\\";
  string::unescape(str);
  ASSERT_EQ(str, "az\nerty\\");
}

TEST(string_utils, unescape_mixed) {
  std::string str = "az\\nerty\\\\\\\\\\a";
  string::unescape(str);
  ASSERT_EQ(str, "az\nerty\\\\\\a");
}

TEST(string_utils, unescape_mixed2) {
  std::string str = "az\\nerty\\\\\\\\\\az";
  string::unescape(str);
  ASSERT_EQ(str, "az\nerty\\\\\\az");
}

TEST(string_utils, unescape_mixed3) {
  std::string str = "az\\nerty\\\\\\\\\\az\\";
  string::unescape(str);
  ASSERT_EQ(str, "az\nerty\\\\\\az\\");
}

TEST(string_utils, unescape_empty) {
  std::string str;
  string::unescape(str);
  ASSERT_EQ(str, "");
}

TEST(string_utils, unescape_no_escape) {
  std::string str = "no backslash here";
  string::unescape(str);
  ASSERT_EQ(str, "no backslash here");
}
