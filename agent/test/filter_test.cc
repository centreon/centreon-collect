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

#include <gtest/gtest.h>

#include "filter.hh"
#include "filter_rules.hh"

using namespace com::centreon::agent::filters;

TEST(filter_test, label_compare_to_value) {
  auto res =
      bp::parse("toto < 42.0u", label_compare_to_value_rule, bp::trace::on);

  ASSERT_TRUE(res);
  EXPECT_EQ(res->get_label(), "toto");
  EXPECT_EQ(res->get_value(), 42.0);
  EXPECT_EQ(res->get_unit(), "u");
  EXPECT_EQ(res->get_comparison(),
            label_compare_to_value::comparison::greater_than);
}

TEST(filter_test, filter_combinator) {
  auto res = bp::parse("toto < 42.0u", filter_combinator_rule, bp::trace::on);

  EXPECT_TRUE(res);

  std::cout << std::endl << std::endl << std::endl;

  auto res2 = bp::parse("toto < 43.0 && titi > 50", filter_combinator_rule,
                        bp::trace::on);

  auto& res2_value = *res2;
  EXPECT_TRUE(res2);

  std::cout << std::endl << std::endl << std::endl;

  auto res3 = bp::parse("toto < 84f && (titi > 53 || uu > 2)",
                        filter_combinator_rule, bp::trace::on);

  auto& res3_value = *res3;
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
}

TEST(filter_test, filter_check_values) {}