/**
 * Copyright 2024 Centreon
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

#include <absl/container/flat_hash_set.h>
#include <gtest/gtest.h>
#include <string>

#include "filter.hh"
#include "filter_rules.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

using namespace com::centreon::agent;
using namespace com::centreon::agent::filters;

TEST(filter_test, label_compare_to_value) {
  auto res =
      bp::parse("toto < 42.0u", label_compare_to_value_rule, bp::trace::on);

  ASSERT_TRUE(res);
  EXPECT_EQ(res->get_label(), "toto");
  EXPECT_EQ(res->get_value(), 42.0);
  EXPECT_EQ(res->get_unit(), "u");
  EXPECT_EQ(res->get_comparison(),
            label_compare_to_value::comparison::less_than);

  auto res2 =
      bp::parse("titi >= 100.5ms", label_compare_to_value_rule, bp::trace::on);

  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->get_label(), "titi");
  EXPECT_EQ(res2->get_value(), 100.5);
  EXPECT_EQ(res2->get_unit(), "ms");
  EXPECT_EQ(res2->get_comparison(),
            label_compare_to_value::comparison::greater_than_or_equal);

  auto res3 =
      bp::parse("foo == 10", label_compare_to_value_rule, bp::trace::on);

  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->get_label(), "foo");
  EXPECT_EQ(res3->get_value(), 10.0);
  EXPECT_EQ(res3->get_unit(), "");
  EXPECT_EQ(res3->get_comparison(), label_compare_to_value::comparison::equal);

  auto res4 =
      bp::parse("bar != 5.5kg", label_compare_to_value_rule, bp::trace::on);

  ASSERT_TRUE(res4);
  EXPECT_EQ(res4->get_label(), "bar");
  EXPECT_EQ(res4->get_value(), 5.5);
  EXPECT_EQ(res4->get_unit(), "kg");
  EXPECT_EQ(res4->get_comparison(),
            label_compare_to_value::comparison::not_equal);
}

TEST(filter_test, filter_in) {
  auto res = bp::parse("toto in (titi,'tutu'   , 'tata')", label_in_rule,
                       bp::trace::on);

  ASSERT_TRUE(res);
  EXPECT_EQ(res->get_label(), "toto");
  EXPECT_EQ(res->get_rule(), label_in::in_not::in);
  EXPECT_EQ(res->get_values().size(), 3);
  EXPECT_EQ(res->get_values().count("titi"), 1);
  EXPECT_EQ(res->get_values().count("tutu"), 1);
  EXPECT_EQ(res->get_values().count("tata"), 1);

  auto res2 =
      bp::parse("toto not_in (titi,tutu,tata)", label_in_rule, bp::trace::on);

  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->get_label(), "toto");
  EXPECT_EQ(res2->get_rule(), label_in::in_not::not_in);
  EXPECT_EQ(res2->get_values().size(), 3);
  EXPECT_EQ(res2->get_values().count("titi"), 1);
  EXPECT_EQ(res2->get_values().count("tutu"), 1);
  EXPECT_EQ(res2->get_values().count("tata"), 1);

  auto res3 = bp::parse("toto in ('titi')", label_in_rule, bp::trace::on);

  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->get_label(), "toto");
  EXPECT_EQ(res3->get_rule(), label_in::in_not::in);
  EXPECT_EQ(res3->get_values().size(), 1);
  EXPECT_EQ(res3->get_values().count("titi"), 1);

  auto res4 = bp::parse("toto not_in (titi)", label_in_rule, bp::trace::on);

  ASSERT_TRUE(res4);
  EXPECT_EQ(res4->get_label(), "toto");
  EXPECT_EQ(res4->get_rule(), label_in::in_not::not_in);
  EXPECT_EQ(res4->get_values().size(), 1);
  EXPECT_EQ(res4->get_values().count("titi"), 1);
}

TEST(filter_test, filter_combinator) {
  auto res = bp::parse("toto < 42.0u", filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res);

  std::cout << std::endl << std::endl << std::endl;

  auto res2 = bp::parse("toto < 43.0 && titi > 50", filter_combinator_rule,
                        bp::trace::on);

  EXPECT_TRUE(res2);

  std::cout << std::endl << std::endl << std::endl;

  auto res3 = bp::parse("toto < 84f && (titi > 53 || uu > 2)",
                        filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res3);

  auto res4 = bp::parse("toto < 84f && titi > 53 || uu > 2",
                        filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res4);

  auto res5 = bp::parse("", filter_combinator_rule);

  EXPECT_FALSE(res5);

  auto res6 = bp::parse("toto < 84f && (titi > 53 || uu > 2",
                        filter_combinator_rule, bp::trace::on);

  EXPECT_FALSE(res6);

  auto res7 = bp::parse("toto < 84f && titi > 53 || uu > 2)",
                        filter_combinator_rule, bp::trace::on);

  EXPECT_FALSE(res7);

  auto res8 = bp::parse("(toto < 84f && ((titi > 53 || uu > 2)))",
                        filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res8);

  auto res9 = bp::parse("(toto < 84f    and (  ( titi>53 or uu    >    2)))",
                        filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res9);

  auto res10 = bp::parse("foo == 10 && bar != 5.5kg", filter_combinator_rule,
                         bp::trace::on);

  EXPECT_TRUE(res10);

  auto res11 = bp::parse("foo == 10 && (bar != 5.5kg || baz < 3)",
                         filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res11);

  auto res12 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                         filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res12);

  auto res13 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'machin')",
      filter_combinator_rule, bp::trace::on);
  EXPECT_TRUE(res13);
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
        label_in* filt = static_cast<label_in*>(f);
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
                        filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res1);
  res1->apply_checker(checker_build);

  to_test values;
  EXPECT_FALSE(res1->check(values));

  auto res2 = bp::parse("toto < 43.0 && titi > 50", filter_combinator_rule,
                        bp::trace::on);

  ASSERT_TRUE(res2);
  res2->apply_checker(checker_build);

  EXPECT_FALSE(res2->check(values));

  auto res3 = bp::parse("toto < 43.0 && titi >= 50", filter_combinator_rule,
                        bp::trace::on);

  ASSERT_TRUE(res3);
  res3->apply_checker(checker_build);

  EXPECT_TRUE(res3->check(values));

  auto res4 = bp::parse("toto < 42.0 || titi > 50", filter_combinator_rule,
                        bp::trace::on);

  ASSERT_TRUE(res4);
  res4->apply_checker(checker_build);

  EXPECT_FALSE(res4->check(values));

  auto res5 = bp::parse("toto <= 42.0 && titi == 50", filter_combinator_rule,
                        bp::trace::on);

  ASSERT_TRUE(res5);
  res5->apply_checker(checker_build);

  EXPECT_TRUE(res5->check(values));

  auto res6 = bp::parse("foo == 10 && bar != 5.5kg", filter_combinator_rule,
                        bp::trace::on);

  ASSERT_TRUE(res6);
  res6->apply_checker(checker_build);

  EXPECT_FALSE(res6->check(values));

  auto res7 = bp::parse("foo == 10 && (bar != 5.5kg || baz < 3)",
                        filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res7);
  res7->apply_checker(checker_build);

  EXPECT_FALSE(res7->check(values));

  auto res8 = bp::parse("foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1))",
                        filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res8);
  res8->apply_checker(checker_build);

  EXPECT_FALSE(res8->check(values));

  auto res9 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'1')",
      filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res9);
  res9->apply_checker(checker_build);

  EXPECT_TRUE(res9->check(values));

  auto res10 = bp::parse(
      "foo == 11 && (bar != 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'2')",
      filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res10);
  res10->apply_checker(checker_build);

  EXPECT_FALSE(res10->check(values));

  auto res11 = bp::parse(
      "foo == 10 && (bar != 5.5kg || (baz < 3.1 && qux >  0.9)) || quux "
      "in(truc, "
      "'2')",
      filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res11);
  res11->apply_checker(checker_build);

  EXPECT_TRUE(res11->check(values));

  auto res12 = bp::parse(
      "foo == 10 && (bar = 5.5kg || (baz < 3 && qux >  1)) || quux in(truc, "
      "'5')",
      filter_combinator_rule, bp::trace::on);

  ASSERT_TRUE(res12);
  res12->apply_checker(checker_build);

  EXPECT_TRUE(res12->check(values));
}

#pragma GCC diagnostic pop
