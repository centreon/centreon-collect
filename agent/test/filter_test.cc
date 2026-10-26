/**
 * Copyright 2025 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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
#include <rapidjson/document.h>

#include "filter.hh"
#include "filter_rules.cc"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

using namespace com::centreon::agent;
using namespace com::centreon::agent::filters;
namespace bp = boost::parser;

TEST(filter_test, label_compare_to_value) {
  auto res =
      bp::parse("toto < 42.0u", label_compare_to_value_rule, bp::trace::off);

  ASSERT_TRUE(res);
  EXPECT_EQ(res->get_label(), "toto");
  EXPECT_EQ(res->get_value(), 42.0);
  EXPECT_EQ(res->get_unit(), "u");
  EXPECT_EQ(res->get_comparison(),
            label_compare_to_value::comparison::less_than);

  auto res2 =
      bp::parse("titi >= 100.5ms", label_compare_to_value_rule, bp::trace::off);

  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->get_label(), "titi");
  EXPECT_EQ(res2->get_value(), 100.5);
  EXPECT_EQ(res2->get_unit(), "ms");
  EXPECT_EQ(res2->get_comparison(),
            label_compare_to_value::comparison::greater_than_or_equal);

  auto res3 =
      bp::parse("foo == 10", label_compare_to_value_rule, bp::trace::off);

  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->get_label(), "foo");
  EXPECT_EQ(res3->get_value(), 10.0);
  EXPECT_EQ(res3->get_unit(), "");
  EXPECT_EQ(res3->get_comparison(), label_compare_to_value::comparison::equal);

  auto res4 =
      bp::parse("bar != 5.5kg", label_compare_to_value_rule, bp::trace::off);

  ASSERT_TRUE(res4);
  EXPECT_EQ(res4->get_label(), "bar");
  EXPECT_EQ(res4->get_value(), 5.5);
  EXPECT_EQ(res4->get_unit(), "kg");
  EXPECT_EQ(res4->get_comparison(),
            label_compare_to_value::comparison::not_equal);
}

TEST(filter_test, label_compare_to_string) {
  auto res = bp::parse("toto == 'hello'", label_compare_to_string_rule,
                       bp::trace::off);

  ASSERT_TRUE(res);
  EXPECT_EQ(res->get_label(), "toto");
  EXPECT_EQ(res->get_value(), "hello");
  EXPECT_EQ(res->get_comparison(), string_comparison::equal);

  auto res2 = bp::parse("titi != 'world'", label_compare_to_string_rule,
                        bp::trace::off);

  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->get_label(), "titi");
  EXPECT_EQ(res2->get_value(), "world");
  EXPECT_EQ(res2->get_comparison(), string_comparison::not_equal);

  auto res3 =
      bp::parse("foo == 'bar'", label_compare_to_string_rule, bp::trace::off);

  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->get_label(), "foo");
  EXPECT_EQ(res3->get_value(), "bar");
  EXPECT_EQ(res3->get_comparison(), string_comparison::equal);

  auto res4 =
      bp::parse("baz != 'qux'", label_compare_to_string_rule, bp::trace::off);

  ASSERT_TRUE(res4);
  EXPECT_EQ(res4->get_label(), "baz");
  EXPECT_EQ(res4->get_value(), "qux");
  EXPECT_EQ(res4->get_comparison(), string_comparison::not_equal);

  auto res5 = bp::parse("quux == 'corge'", label_compare_to_string_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res5);
  EXPECT_EQ(res5->get_label(), "quux");
  EXPECT_EQ(res5->get_value(), L"corge");
  EXPECT_EQ(res5->get_comparison(), string_comparison::equal);

  auto res6 = bp::parse("grault != 'garply'", label_compare_to_string_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res6);
  EXPECT_EQ(res6->get_label(), "grault");
  EXPECT_EQ(res6->get_value(), L"garply");
  EXPECT_EQ(res6->get_comparison(), string_comparison::not_equal);

  // values are UTF-8, they must be decoded and not widened byte by byte
  auto res7 = bp::parse("provider == 'Tâche planifiée'",
                        label_compare_to_string_rule_w, bp::trace::off);

  ASSERT_TRUE(res7);
  EXPECT_EQ(res7->get_label(), "provider");
  EXPECT_EQ(res7->get_value(), L"Tâche planifiée");
}

TEST(filter_test, filter_in) {
  auto res = bp::parse("toto in (titi,'tutu'   , 'tata')", label_in_rule,
                       bp::trace::off);

  ASSERT_TRUE(res);
  EXPECT_EQ(res->get_label(), "toto");
  EXPECT_EQ(res->get_rule(), in_not::in);
  EXPECT_EQ(res->get_values().size(), 3);
  EXPECT_EQ(res->get_values().count("titi"), 1);
  EXPECT_EQ(res->get_values().count("tutu"), 1);
  EXPECT_EQ(res->get_values().count("tata"), 1);

  auto res2 =
      bp::parse("toto not_in (titi,tutu,tata)", label_in_rule, bp::trace::off);

  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->get_label(), "toto");
  EXPECT_EQ(res2->get_rule(), in_not::not_in);
  EXPECT_EQ(res2->get_values().size(), 3);
  EXPECT_EQ(res2->get_values().count("titi"), 1);
  EXPECT_EQ(res2->get_values().count("tutu"), 1);
  EXPECT_EQ(res2->get_values().count("tata"), 1);

  auto res3 = bp::parse("toto in ('titi')", label_in_rule, bp::trace::off);

  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->get_label(), "toto");
  EXPECT_EQ(res3->get_rule(), in_not::in);
  EXPECT_EQ(res3->get_values().size(), 1);
  EXPECT_EQ(res3->get_values().count("titi"), 1);

  auto res4 = bp::parse("toto not_in (titi)", label_in_rule, bp::trace::off);

  ASSERT_TRUE(res4);
  EXPECT_EQ(res4->get_label(), "toto");
  EXPECT_EQ(res4->get_rule(), in_not::not_in);
  EXPECT_EQ(res4->get_values().size(), 1);
  EXPECT_EQ(res4->get_values().count("titi"), 1);

  auto res5 = bp::parse("toto not_in (titi)", label_in_rule_w, bp::trace::off);

  ASSERT_TRUE(res5);
  EXPECT_EQ(res5->get_label(), "toto");
  EXPECT_EQ(res5->get_rule(), filters::in_not::not_in);
  EXPECT_EQ(res5->get_values().size(), 1);
  EXPECT_EQ(res5->get_values().count(L"titi"), 1);

  // values are UTF-8, they must be decoded and not widened byte by byte
  auto res6 = bp::parse("provider in ('Tâche', 'Contrôle 東京')",
                        label_in_rule_w, bp::trace::off);

  ASSERT_TRUE(res6);
  EXPECT_EQ(res6->get_values().size(), 2);
  EXPECT_EQ(res6->get_values().count(L"Tâche"), 1);
  EXPECT_EQ(res6->get_values().count(L"Contrôle 東京"), 1);
}

TEST(filter_test, quoted_value_with_backslashes) {
  // a backslash that escapes nothing is a literal backslash
  for (std::string_view filter_str :
       {R"(filename = 'C:\Windows\System32\svchost.exe')",
        R"(filename = 'C:\\Windows\\System32\\svchost.exe')"}) {
    auto res =
        bp::parse(filter_str, label_compare_to_string_rule, bp::trace::off);
    ASSERT_TRUE(res) << filter_str;
    EXPECT_EQ(res->get_value(), R"(C:\Windows\System32\svchost.exe)")
        << filter_str;

    auto res_w =
        bp::parse(filter_str, label_compare_to_string_rule_w, bp::trace::off);
    ASSERT_TRUE(res_w) << filter_str;
    EXPECT_EQ(res_w->get_value(), LR"(C:\Windows\System32\svchost.exe)")
        << filter_str;
  }

  auto escaped_quote = bp::parse(R"(name = 'it\'s')",
                                 label_compare_to_string_rule, bp::trace::off);
  ASSERT_TRUE(escaped_quote);
  EXPECT_EQ(escaped_quote->get_value(), "it's");

  // \' is an escaped quote, so a trailing backslash must be escaped
  auto trailing_backslash = bp::parse(
      R"(path = 'C:\\')", label_compare_to_string_rule, bp::trace::off);
  ASSERT_TRUE(trailing_backslash);
  EXPECT_EQ(trailing_backslash->get_value(), R"(C:\)");

  auto res_in = bp::parse(
      R"(folder in ('\Folder\To\Task', "\\Other\\Task", "say \"hi\""))",
      label_in_rule, bp::trace::off);
  ASSERT_TRUE(res_in);
  EXPECT_EQ(res_in->get_values().size(), 3);
  EXPECT_EQ(res_in->get_values().count(R"(\Folder\To\Task)"), 1);
  EXPECT_EQ(res_in->get_values().count(R"(\Other\Task)"), 1);
  EXPECT_EQ(res_in->get_values().count(R"(say "hi")"), 1);

  auto res_in_w = bp::parse(R"(folder in ('\Folder\To\Task'))", label_in_rule_w,
                            bp::trace::off);
  ASSERT_TRUE(res_in_w);
  EXPECT_EQ(res_in_w->get_values().count(LR"(\Folder\To\Task)"), 1);
}

TEST(filter_test, filter_macro_backslash_escaped_once_or_twice) {
  std::string dumps[2];
  int idx = 0;
  // filter macro escaped once and twice (legacy form), engine copies it as is
  // in the json command line
  for (std::string_view macro :
       {R"(filename = 'C:\\Windows\\System32\\svchost.exe' && )"
        R"(folder in ('\\Folder\\To\\Task'))",
        R"(filename = 'C:\\\\Windows\\\\System32\\\\svchost.exe' && )"
        R"(folder in ('\\\\Folder\\\\To\\\\Task'))"}) {
    rapidjson::Document args;
    args.Parse(absl::StrCat(R"({"filter": ")", macro, R"("})").c_str());
    ASSERT_FALSE(args.HasParseError()) << macro;

    filters::filter_combinator res;
    ASSERT_TRUE(filter::create_filter(args["filter"].GetString(),
                                      spdlog::default_logger(), &res))
        << macro;
    std::ostringstream dump;
    dump << res;
    dumps[idx++] = dump.str();
  }
  EXPECT_EQ(dumps[0], dumps[1]);
  EXPECT_NE(dumps[0].find(R"(C:\Windows\System32\svchost.exe)"),
            std::string::npos)
      << dumps[0];
  EXPECT_NE(dumps[0].find(R"(\Folder\To\Task)"), std::string::npos) << dumps[0];
}

TEST(filter_test, filter_combinator) {
  auto res = bp::parse("toto < 42.0u", filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res);

  auto res2 = bp::parse("toto < 43.0 && titi > 50", filter_combinator_rule,
                        bp::trace::off);

  EXPECT_TRUE(res2);

  auto res3 = bp::parse("toto < 84f && (titi > 53 || uu > 2)",
                        filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res3);

  auto res4 = bp::parse("toto < 84f && titi > 53 || uu > 2",
                        filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res4);

  auto res5 = bp::parse("", filter_combinator_rule);

  EXPECT_FALSE(res5);

  auto res6 = bp::parse("toto < 84f && (titi > 53 || uu > 2",
                        filter_combinator_rule, bp::trace::off);

  EXPECT_FALSE(res6);

  auto res7 = bp::parse("toto < 84f && titi > 53 || uu > 2)",
                        filter_combinator_rule, bp::trace::off);

  EXPECT_FALSE(res7);

  auto res8 = bp::parse("(toto < 84f && ((titi > 53 || uu > 2)))",
                        filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res8);

  auto res9 = bp::parse("(toto < 84f    and (  ( titi>53 or uu    >    2)))",
                        filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res9);

  auto res10 = bp::parse("foo == 10 && bar != 5.5kg", filter_combinator_rule,
                         bp::trace::off);

  EXPECT_TRUE(res10);

  auto res11 = bp::parse("foo == 10 && (bar != 5.5kg || baz < 3)",
                         filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res11);

  auto res12 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                         filter_combinator_rule, bp::trace::off);

  EXPECT_TRUE(res12);

  auto res13 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'machin')",
      filter_combinator_rule, bp::trace::off);
  EXPECT_TRUE(res13);

  auto res14 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'machin')",
      filter_combinator_rule_w, bp::trace::off);
  EXPECT_TRUE(res14);
}

TEST(filter_test, filter_check_values) {
  // Add tests for checking values
  struct to_test : public testable {
    absl::flat_hash_map<std::string, double> values = {
        {"toto", 42.0},  {"titi", 50.0}, {"uu", 2.0},  {"truc", 1.0},
        {"machin", 2.0}, {"foo", 10.0},  {"bar", 5.5}, {"baz", 3.0},
        {"qux", 1.0},    {"quux", 1.0}};
  };

  auto checker_build = [](filter* f) {
    switch (f->get_type()) {
      case filter::filter_type::label_compare_to_value: {
        label_compare_to_value* filt = static_cast<label_compare_to_value*>(f);
        filt->set_checker_from_getter(
            [label = filt->get_label()](const testable& t) -> double {
              const auto& tt = static_cast<const to_test&>(t);
              auto it = tt.values.find(label);
              if (it == tt.values.end()) {
                return 0.0;
              }
              return it->second;
            });
        break;
      }
      case filter::filter_type::label_in: {
        label_in<char>* filt = static_cast<label_in<char>*>(f);
        filt->set_checker_from_getter(
            [label = filt->get_label()](const testable& t) -> std::string {
              const auto& tt = static_cast<const to_test&>(t);
              auto it = tt.values.find(label);
              if (it == tt.values.end()) {
                return "";
              }
              return std::to_string((int)it->second);
            });
        break;
      }
      default:
        break;
    }
  };

  auto res1 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                        filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res1);
  res1->apply_checker(checker_build);

  to_test values;
  EXPECT_FALSE(res1->check(values));

  auto res2 = bp::parse("toto < 43.0 && titi > 50", filter_combinator_rule,
                        bp::trace::off);

  ASSERT_TRUE(res2);
  res2->apply_checker(checker_build);

  EXPECT_FALSE(res2->check(values));

  auto res3 = bp::parse("toto < 43.0 && titi >= 50", filter_combinator_rule,
                        bp::trace::off);

  ASSERT_TRUE(res3);
  res3->apply_checker(checker_build);

  EXPECT_TRUE(res3->check(values));

  auto res4 = bp::parse("toto < 42.0 || titi > 50", filter_combinator_rule,
                        bp::trace::off);

  ASSERT_TRUE(res4);
  res4->apply_checker(checker_build);

  EXPECT_FALSE(res4->check(values));

  auto res5 = bp::parse("toto <= 42.0 && titi == 50", filter_combinator_rule,
                        bp::trace::off);

  ASSERT_TRUE(res5);
  res5->apply_checker(checker_build);

  EXPECT_TRUE(res5->check(values));

  auto res6 = bp::parse("foo == 10 && bar != 5.5kg", filter_combinator_rule,
                        bp::trace::off);

  ASSERT_TRUE(res6);
  res6->apply_checker(checker_build);

  EXPECT_FALSE(res6->check(values));

  auto res7 = bp::parse("foo == 10 && (bar != 5.5kg || baz < 3)",
                        filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res7);
  res7->apply_checker(checker_build);

  EXPECT_FALSE(res7->check(values));

  auto res8 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                        filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res8);
  res8->apply_checker(checker_build);

  EXPECT_FALSE(res8->check(values));

  auto res9 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'1')",
      filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res9);
  res9->apply_checker(checker_build);

  EXPECT_TRUE(res9->check(values));

  auto res10 = bp::parse(
      "foo == 11 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'2')",
      filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res10);
  res10->apply_checker(checker_build);

  EXPECT_FALSE(res10->check(values));

  auto res11 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3.1 && qux >  0.9)) || quux "
      "in(truc, "
      "'2')",
      filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res11);
  res11->apply_checker(checker_build);

  EXPECT_TRUE(res11->check(values));

  auto res12 = bp::parse(
      "foo == 10 && (bar = 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'5')",
      filter_combinator_rule, bp::trace::off);

  ASSERT_TRUE(res12);
  res12->apply_checker(checker_build);

  EXPECT_TRUE(res12->check(values));
}

TEST(filter_test, filter_check_values_w) {
  // Add tests for checking values
  struct to_test : public testable {
    absl::flat_hash_map<std::wstring, double> values = {
        {L"toto", 42.0},  {L"titi", 50.0}, {L"uu", 2.0},  {L"truc", 1.0},
        {L"machin", 2.0}, {L"foo", 10.0},  {L"bar", 5.5}, {L"baz", 3.0},
        {L"qux", 1.0},    {L"quux", 1.0}};
  };

  auto checker_build = [](filter* f) {
    switch (f->get_type()) {
      case filter::filter_type::label_compare_to_value: {
        label_compare_to_value* filt = static_cast<label_compare_to_value*>(f);
        filt->set_checker_from_getter(
            [label = std::wstring(filt->get_label().begin(),
                                  filt->get_label().end())](
                const testable& t) -> double {
              const auto& tt = static_cast<const to_test&>(t);
              auto it = tt.values.find(label);
              if (it == tt.values.end()) {
                return 0.0;
              }
              return it->second;
            });
        break;
      }
      case filter::filter_type::label_in: {
        label_in<wchar_t>* filt = static_cast<label_in<wchar_t>*>(f);
        filt->set_checker_from_getter(
            [label = std::wstring(filt->get_label().begin(),
                                  filt->get_label().end())](
                const testable& t) -> std::wstring {
              const auto& tt = static_cast<const to_test&>(t);
              auto it = tt.values.find(label);
              if (it == tt.values.end()) {
                return L"";
              }
              return std::to_wstring((int)it->second);
            });
        break;
      }
      default:
        break;
    }
  };

  auto res1 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                        filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res1);
  res1->apply_checker(checker_build);

  to_test values;
  EXPECT_FALSE(res1->check(values));

  auto res2 = bp::parse("toto < 43.0 && titi > 50", filter_combinator_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res2);
  res2->apply_checker(checker_build);

  EXPECT_FALSE(res2->check(values));

  auto res3 = bp::parse("toto < 43.0 && titi >= 50", filter_combinator_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res3);
  res3->apply_checker(checker_build);

  EXPECT_TRUE(res3->check(values));

  auto res4 = bp::parse("toto < 42.0 || titi > 50", filter_combinator_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res4);
  res4->apply_checker(checker_build);

  EXPECT_FALSE(res4->check(values));

  auto res5 = bp::parse("toto <= 42.0 && titi == 50", filter_combinator_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res5);
  res5->apply_checker(checker_build);

  EXPECT_TRUE(res5->check(values));

  auto res6 = bp::parse("foo == 10 && bar != 5.5kg", filter_combinator_rule_w,
                        bp::trace::off);

  ASSERT_TRUE(res6);
  res6->apply_checker(checker_build);

  EXPECT_FALSE(res6->check(values));

  auto res7 = bp::parse("foo == 10 && (bar != 5.5kg || baz < 3)",
                        filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res7);
  res7->apply_checker(checker_build);

  EXPECT_FALSE(res7->check(values));

  auto res8 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                        filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res8);
  res8->apply_checker(checker_build);

  EXPECT_FALSE(res8->check(values));

  auto res9 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'1')",
      filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res9);
  res9->apply_checker(checker_build);

  EXPECT_TRUE(res9->check(values));

  auto res10 = bp::parse(
      "foo == 11 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'2')",
      filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res10);
  res10->apply_checker(checker_build);

  EXPECT_FALSE(res10->check(values));

  auto res11 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3.1 && qux >  0.9)) || quux "
      "in(truc, "
      "'2')",
      filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res11);
  res11->apply_checker(checker_build);

  EXPECT_TRUE(res11->check(values));

  auto res12 = bp::parse(
      "foo == 10 && (bar = 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'5')",
      filter_combinator_rule_w, bp::trace::off);

  ASSERT_TRUE(res12);
  res12->apply_checker(checker_build);

  EXPECT_TRUE(res12->check(values));

  filters::filter_combinator res13;
  ASSERT_TRUE(filter::create_filter(
      "foo == 10 && (bar = 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'5')",
      spdlog::default_logger(), &res13, true, false));

  res13.apply_checker(checker_build);

  EXPECT_TRUE(res13.check(values));
}

TEST(filter_test, filter_with_error) {
  filters::filter_combinator res;
  EXPECT_FALSE(
      filter::create_filter("turlutut(u", spdlog::default_logger(), &res));
}

#pragma GCC diagnostic pop
