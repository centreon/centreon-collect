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
 * Given a string encoded in CP-1252 containing 0x9B (›), a valid CP-1252 char
 * Then the check_string_utf8 function converts it as CP-1252.
 */
TEST(string_check_utf8, cp1252_single_right_angle_quote) {
  std::string txt("Contr\xf4le \x9b l\xe9galit\xe9");
  ASSERT_EQ(check_string_utf8(txt), "Contrôle › légalité");
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

TEST(is_utf8, valid) {
  ASSERT_TRUE(is_utf8(""));
  ASSERT_TRUE(is_utf8("ascii"));
  ASSERT_TRUE(is_utf8("Contrôle Légalité"));
  ASSERT_TRUE(is_utf8("Москва 東京 📁"));
}

TEST(is_utf8, invalid) {
  // cp1252 é
  ASSERT_FALSE(is_utf8("l\xe9galit\xe9"));
  // truncated sequence
  ASSERT_FALSE(is_utf8("contr\xc3"));
  // overlong '/'
  ASSERT_FALSE(is_utf8("\xc0\xaf"));
  // surrogate U+D800
  ASSERT_FALSE(is_utf8("\xed\xa0\x80"));
  // above U+10FFFF
  ASSERT_FALSE(is_utf8("\xf4\x90\x80\x80"));
}

/*
 * Given UTF-8 strings with 2, 3 and 4 bytes sequences
 * Then utf8_to_wstring and wstring_to_utf8 convert them back and forth.
 */
TEST(utf8_to_wstring, round_trip) {
  for (std::string_view str :
       {"", "ascii", "Contrôle Légalité", "S=Côte-d'Or, C=FR", "Zażółć",
        "Москва", "東京", "📁 ✅"}) {
    ASSERT_EQ(wstring_to_utf8(utf8_to_wstring(str)), str);
  }
  ASSERT_EQ(utf8_to_wstring("Contrôle Légalité"), L"Contrôle Légalité");
  ASSERT_EQ(utf8_to_wstring("東京"), L"東京");
  ASSERT_EQ(utf8_to_wstring("📁"), L"📁");
  ASSERT_EQ(wstring_to_utf8(L"Contrôle Légalité"), "Contrôle Légalité");
  ASSERT_EQ(wstring_to_utf8(L"📁"), "📁");
}

/*
 * Given an invalid UTF-8 string
 * Then each invalid byte is replaced by U+FFFD
 */
TEST(utf8_to_wstring, invalid) {
  ASSERT_EQ(utf8_to_wstring("l\xe9galit\xe9"), L"l\xfffdgalit\xfffd");
  ASSERT_EQ(utf8_to_wstring("contr\xc3"), L"contr\xfffd");
  ASSERT_EQ(utf8_to_wstring("\xc0\xaf"), L"\xfffd\xfffd");
  ASSERT_EQ(utf8_to_wstring("\xed\xa0\x80"), L"\xfffd\xfffd\xfffd");
}

/*
 * Given a wide string with an unpaired surrogate
 * Then it is replaced by U+FFFD
 */
TEST(wstring_to_utf8, unpaired_surrogate) {
  std::wstring str{L'a', static_cast<wchar_t>(0xd800), L'b'};
  ASSERT_EQ(wstring_to_utf8(str),
            "a\xef\xbf\xbd"
            "b");
}

/*
 * Given a valid UTF-8 string
 * Then ensure_utf8 returns it unchanged
 * Given a string in a legacy encoding
 * Then ensure_utf8 returns a valid UTF-8 string
 */
TEST(ensure_utf8, nominal) {
  ASSERT_EQ(ensure_utf8("Contrôle Légalité"), "Contrôle Légalité");
  ASSERT_EQ(ensure_utf8("東京"), "東京");
  ASSERT_TRUE(is_utf8(ensure_utf8("C=FR, S=C\xf4te-d'Or")));
  ASSERT_TRUE(is_utf8(ensure_utf8("\x81\x8d\x8f\x90\x9d")));
#ifndef _WIN32
  // on Windows, result depends on the ANSI code page
  ASSERT_EQ(ensure_utf8("C=FR, S=C\xf4te-d'Or"), "C=FR, S=Côte-d'Or");
#endif
}
