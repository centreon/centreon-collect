/**
 * Copyright 2024 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>

#include "utf8.hh"

using namespace com::centreon::common;

/*
 * Given a string encoded in ISO-8859-15 and CP-1252
 * Then the check_string_utf8 function converts it to UTF-8.
 */
TEST(string_check_utf8, simple) {
  std::string txt("L'acc\350s \340 l'h\364tel est encombr\351");
  ASSERT_EQ(check_string_utf8(txt), "L'accès à l'hôtel est encombré");
}

/*
 * Given a string encoded in UTF-8
 * Then the check_string_utf8 function returns itself.
 */
TEST(string_check_utf8, utf8) {
  std::string txt("L'accès à l'hôtel est encombré");
  ASSERT_EQ(check_string_utf8(txt), "L'accès à l'hôtel est encombré");
}

/*
 * Given a string encoded in CP-1252
 * Then the check_string_utf8 function converts it to UTF-8.
 */
TEST(string_check_utf8, cp1252) {
  std::string txt("Le ticket co\xfbte 12\x80\n");
  ASSERT_EQ(check_string_utf8(txt), "Le ticket coûte 12€\n");
}

/*
 * Given a string encoded in CP-1252
 * Then the check_string_utf8 function converts it to UTF-8.
 */
TEST(string_check_utf8, cp1252_bis) {
  std::string txt("Service de plateforme des appareils connect\xe9s");
  ASSERT_EQ(check_string_utf8(txt),
            "Service de plateforme des appareils connectés");
}

/*
 * Given a string encoded in ISO-8859-15
 * Then the check_string_utf8 function converts it to UTF-8.
 */
TEST(string_check_utf8, iso8859) {
  std::string txt("Le ticket co\xfbte 12\xa4\n");
  ASSERT_EQ(check_string_utf8(txt), "Le ticket coûte 12€\n");
}

/*
 * Given a string encoded in ISO-8859-15
 * Then the check_string_utf8 function converts it to UTF-8.
 */
TEST(string_check_utf8, iso8859_cpx) {
  std::string txt("\xa4\xa6\xa8\xb4\xb8\xbc\xbd\xbe");
  ASSERT_EQ(check_string_utf8(txt), "€ŠšŽžŒœŸ");
}

/*
 * Given a string encoded in CP-1252
 * Then the check_string_utf8 function converts it to UTF-8.
 */
TEST(string_check_utf8, cp1252_cpx) {
  std::string txt("\x80\x95\x82\x89\x8a");
  ASSERT_EQ(check_string_utf8(txt), "€•‚‰Š");
}

/*
 * Given a string badly encoded in CP-1252
 * Then the check_string_utf8 function converts it to UTF-8 and replaces bad
 * characters into '_'.
 */
TEST(string_check_utf8, whatever_as_cp1252) {
  std::string txt;
  for (uint8_t c = 32; c < 255; c++)
    if (c != 127)
      txt.push_back(c);
  std::string result(
      " !\"#$%&'()*+,-./"
      "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
      "abcdefghijklmnopqrstuvwxyz{|}~€_‚ƒ„…†‡ˆ‰Š‹Œ_Ž__‘’“”•–—˜™š›œ_"
      "žŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäå"
      "æçèéêëìíîïðñòóôõö÷øùúûüýþ");
  ASSERT_EQ(check_string_utf8(txt), result);
}

/*
 * Given a string badly encoded in ISO-8859-15
 * Then the check_string_utf8 function converts it to UTF-8 and replaces bad
 * characters into '_'.
 */
TEST(string_check_utf8, whatever_as_iso8859) {
  /* Construction of a string that is not cp1252 so it should be considered as
   * iso8859-15 */
  std::string txt;
  for (uint8_t c = 32; c < 255; c++) {
    if (c == 32)
      txt.push_back(0x81);
    if (c != 127)
      txt.push_back(c);
  }
  std::string result(
      "_ "
      "!\"#$%&'()*+,-./"
      "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
      "abcdefghijklmnopqrstuvwxyz{|}~_________________________________"
      "¡¢£€¥Š§š©ª«¬­®¯°±²³Žµ¶·ž¹º»ŒœŸ¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçè"
      "éêëìíîïðñòóôõö÷øùúûüýþ");
  ASSERT_EQ(check_string_utf8(txt), result);
}

/*
 * In case of a string containing multiple encoding, the resulting string should
 * be an UTF-8 string. Here we have a string beginning with UTF-8 and finishing
 * with cp1252. The resulting string is good and is UTF-8 only encoded.
 */
TEST(string_check_utf8, utf8_and_cp1252) {
  std::string txt(
      "\xc3\xa9\xc3\xa7\xc3\xa8\xc3\xa0\xc3\xb9\xc3\xaf\xc3\xab\x7e\x23\x0a\xe9"
      "\xe7\xe8\xe0\xf9\xef\xeb\x7e\x23\x0a");
  std::string result("éçèàùïë~#\néçèàùïë~#\n");
  ASSERT_EQ(check_string_utf8(txt), result);
}

/* A check coming from windows with characters from the cmd console */
TEST(string_check_utf8, strange_string) {
  std::string txt(
      "WARNING - [Triggered by _ItemCount>0] - 1 event(s) of Severity Level: "
      "\"Error\", were recorded in the last 24 hours from the Application "
      "Event Log. (List is on next line. Fields shown are - "
      "Logfile:TimeGenerated:EventId:EventCode:SeverityLevel:Type:SourceName:"
      "Message)|'Event "
      "Count'=1;0;50;\nApplication:20200806000001.000000-000:3221243278:17806:"
      "Erreur:MSSQLSERVER:╔chec de la nÚgociation SSPI avec le code "
      "d'erreurá0x8009030c lors de l'Útablissement d'une connexion avec une "
      "sÚcuritÚ intÚgrÚeá; la connexion a ÚtÚ fermÚe. [CLIENTá: X.X.X.X]");
  ASSERT_EQ(check_string_utf8(txt), txt);
}

/* A check coming from windows with characters from the cmd console */
TEST(string_check_utf8, chinese) {
  std::string txt("超级杀手死亡检查");
  ASSERT_EQ(check_string_utf8(txt), txt);
}

/* A check coming from windows with characters from the cmd console */
TEST(string_check_utf8, vietnam) {
  std::string txt(
      "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong "
      "chinese 告警数量 output puté! | '告警数量'=42\navé dé long ouput oçi "
      "还有中国人! Hái yǒu zhòng guó rén!");
  ASSERT_EQ(check_string_utf8(txt), txt);
}

TEST(truncate, nominal1) {
  std::string str("foobar");
  ASSERT_EQ(truncate_utf8(str, 3), "foo");
}

TEST(truncate, nominal2) {
  std::string str("foobar");
  ASSERT_EQ(truncate_utf8(str, 0), "");
}

TEST(truncate, nominal3) {
  std::string str("foobar 超级杀手死亡检查");
  ASSERT_EQ(truncate_utf8(str, 1000), "foobar 超级杀手死亡检查");
}

TEST(truncate, utf8_1) {
  std::string str("告警数量");
  for (size_t i = 0; i <= str.size(); i++) {
    fmt::string_view tmp(str);
    fmt::string_view res(truncate_utf8(tmp, i));
    std::string tmp1(check_string_utf8(std::string(res.data(), res.size())));
    ASSERT_EQ(res, tmp1);
  }
}

TEST(adjust_size_utf8, nominal1) {
  std::string str("foobar");
  ASSERT_EQ(fmt::string_view(str.data(), adjust_size_utf8(str, 3)),
            fmt::string_view("foo"));
}

TEST(adjust_size_utf8, nominal2) {
  std::string str("foobar");
  ASSERT_EQ(fmt::string_view(str.data(), adjust_size_utf8(str, 0)), "");
}

TEST(adjust_size_utf8, nominal3) {
  std::string str("foobar 超级杀手死亡检查");
  ASSERT_EQ(fmt::string_view(str.data(), adjust_size_utf8(str, 1000)), str);
}

TEST(adjust_size_utf8, utf8_1) {
  std::string str("告警数量");
  for (size_t i = 0; i <= str.size(); i++) {
    fmt::string_view sv(str.data(), adjust_size_utf8(str, i));
    std::string tmp(
        check_string_utf8(std::string(sv.data(), sv.data() + sv.size())));
    ASSERT_EQ(sv.size(), tmp.size());
  }
}
