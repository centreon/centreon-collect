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

TEST(string_utils, trim) {
  std::string str("Hi guys!");
  string::trim(str);
  ASSERT_EQ(str, "Hi guys!");

  str = " a b c  ";
  string::trim(str);
  ASSERT_EQ(str, "a b c");

  str =
      "performance_data=rta=0.053ms;3000.000;5000.000;0; pl=0%;80;100;0;100 "
      "rtmax=0.053ms;;;; rtmin=0.053ms;;;;";
  string::trim(str);
  ASSERT_EQ(str, "performance_data=rta=0.053ms");
}

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
  ASSERT_EQ(*parse.extract(';'), "toto");
  ASSERT_EQ(*parse.extract(';'), "");
  ASSERT_EQ(*parse.extract('|'), "titi");
  ASSERT_EQ(*parse.extract('\n'), "tata");
  ASSERT_EQ(*parse.extract('*'), "");
  ASSERT_FALSE(parse.extract('*'));
}

TEST(string_utils, c_strtok_test2) {
  string::c_strtok parse("toto;;titi|tata\n");
  ASSERT_EQ(*parse.extract(';'), "toto");
  ASSERT_EQ(*parse.extract('&'), ";titi|tata\n");
  ASSERT_FALSE(parse.extract('\n'));
}

TEST(string_utils, c_strtok_test3) {
  string::c_strtok parse("|toto;;titi|tata\n");
  ASSERT_EQ(*parse.extract('|'), "");
  ASSERT_EQ(*parse.extract('|'), "toto;;titi");
  ASSERT_EQ(*parse.extract('|'), "tata\n");
  ASSERT_FALSE(parse.extract('\n'));
}

TEST(string_utils, c_strtok_test4) {
  string::c_strtok parse("toto");
  ASSERT_EQ(*parse.extract('|'), "toto");
  ASSERT_FALSE(parse.extract('\n'));
}

TEST(string_utils, c_strtok_test5) {
  string::c_strtok parse("1");
  int val;
  ASSERT_TRUE(parse.extract(';', val));
  ASSERT_EQ(val, 1);
  ASSERT_FALSE(parse.extract(';'));
}

TEST(string_utils, c_strtok_test6) {
  string::c_strtok parse("toto1");
  int val;
  ASSERT_FALSE(parse.extract(';', val));
  ASSERT_FALSE(parse.extract(';'));
}

TEST(string_utils, c_strtok_test7) {
  string::c_strtok parse("toto;1");
  ASSERT_EQ(*parse.extract(';'), "toto");
  int val;
  ASSERT_TRUE(parse.extract(';', val));
  ASSERT_EQ(val, 1);
  ASSERT_FALSE(parse.extract(';'));
}

TEST(string_utils, unescape) {
  char str[100];
  strcpy(str, "az\\ner\\nty\\n");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\ner\nty\n") == 0);
}

TEST(string_utils, unescape1) {
  char str[100];
  strcpy(str, "az\\ner\\nty\\n");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\ner\nty\n") == 0);
}

TEST(string_utils, unescape2) {
  char str[100];
  strcpy(str, "az\\ter\\tty\\n");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\ter\tty\n") == 0);
}

TEST(string_utils, unescape3) {
  char str[100];
  strcpy(str, "azerty\\");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "azerty\\") == 0);
}

TEST(string_utils, unescape4) {
  char str[100];
  strcpy(str, "az\\nerty\\");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\nerty\\") == 0);
}

TEST(string_utils, unescape5) {
  char str[100];
  strcpy(str, "az\\nerty\\\\\\\\\\a");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\nerty\\\\\\a") == 0);
}

TEST(string_utils, unescape6) {
  char str[100];
  strcpy(str, "az\\nerty\\\\\\\\\\az");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\nerty\\\\\\az") == 0);
}

TEST(string_utils, unescape7) {
  char str[100];
  strcpy(str, "az\\nerty\\\\\\\\\\az\\");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "az\nerty\\\\\\az\\") == 0);
}

TEST(string_utils, unescape8) {
  char str[100];
  strcpy(str, "");
  string::unescape(str);
  ASSERT_TRUE(strcmp(str, "") == 0);
}

TEST(string_utils, unescape9) {
  char* s = nullptr;
  string::unescape(s);
  ASSERT_EQ(s, nullptr);
}
