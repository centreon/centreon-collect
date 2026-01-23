/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include "threshold.hh"

using namespace com::centreon;
using namespace com::centreon::common;

/**
 *  Check that the threshold assignment operator works properly.
 */
TEST(thresholdtest, init) {
  // First object.
  threshold t1("2.3:7.8");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_EQ(t1.get_low(), 2.3);
  ASSERT_EQ(t1.get_high(), 7.8);
  ASSERT_FALSE(t1.is_inclusive());
  ASSERT_FALSE(t1.is_triggered(7.7));
  ASSERT_TRUE(t1.is_triggered(2.2));

  // strict boundaries
  ASSERT_FALSE(t1.is_triggered(2.3));
  ASSERT_FALSE(t1.is_triggered(7.8));
}
TEST(thresholdtest, init_inclusive) {
  // First object.
  threshold t1("@2.3:7.8");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_EQ(t1.get_low(), 2.3);
  ASSERT_EQ(t1.get_high(), 7.8);
  ASSERT_TRUE(t1.is_inclusive());
  ASSERT_TRUE(t1.is_triggered(7.7));
  ASSERT_FALSE(t1.is_triggered(2.2));

  // inclusive boundaries
  ASSERT_TRUE(t1.is_triggered(2.3));
  ASSERT_TRUE(t1.is_triggered(7.8));
}

TEST(thresholdtest, higher_strict_double) {
  // First object.
  threshold t1("2.3");

  ASSERT_EQ(t1.get_error(), 0);
  ASSERT_EQ(t1.get_high(), 2.3);
  ASSERT_TRUE(std::isnan(t1.get_low()));

  ASSERT_TRUE(t1.is_triggered(7.7));
  ASSERT_FALSE(t1.is_triggered(2.2));
  ASSERT_FALSE(t1.is_triggered(2.3));
  ASSERT_TRUE(t1.is_triggered(2.31));
}

TEST(thresholdtest, higher_strict_int) {
  // First object.
  threshold t1("2");

  ASSERT_EQ(t1.get_error(), 0);
  ASSERT_EQ(t1.get_high(), 2);
  ASSERT_TRUE(std::isnan(t1.get_low()));

  ASSERT_TRUE(t1.is_triggered(7.7));
  ASSERT_FALSE(t1.is_triggered(2));
  ASSERT_TRUE(t1.is_triggered(2.1));
}

TEST(thresholdtest, lowerstrict) {
  // First object.
  threshold t1("2.3:");

  ASSERT_EQ(t1.get_error(), 0);
  ASSERT_TRUE(std::isnan(t1.get_high()));
  ASSERT_EQ(t1.get_low(), 2.3);

  ASSERT_FALSE(t1.is_triggered(2.3));
  ASSERT_TRUE(t1.is_triggered(2.29));
}

TEST(thresholdtest, negative) {
  // First object.
  threshold t1("-2.3:8");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_EQ(t1.get_low(), -2.3);
  ASSERT_EQ(t1.get_high(), 8.0);

  ASSERT_FALSE(t1.is_triggered(7.7));
  ASSERT_TRUE(t1.is_triggered(-2.4));
}

TEST(thresholdtest, negative2) {
  // First object.
  threshold t1("-7.3:-1.1");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_EQ(t1.get_low(), -7.3);
  ASSERT_EQ(t1.get_high(), -1.1);

  ASSERT_FALSE(t1.is_triggered(-1.7));
  ASSERT_TRUE(t1.is_triggered(0.0));
}
TEST(thresholdtest, test_float_parsing) {
  // First object.
  threshold t1("12.");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_TRUE(std::isnan(t1.get_low()));
  ASSERT_EQ(t1.get_high(), 12.0);

  ASSERT_FALSE(t1.is_triggered(11.9));
  ASSERT_TRUE(t1.is_triggered(12.1));
}

TEST(thresholdtest, unit) {
  // First object.
  threshold t1("12:13");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_EQ(t1.get_low(), 12.0);
  ASSERT_EQ(t1.get_high(), 13.0);

  t1.unit_multiplier(1000.0);

  ASSERT_EQ(t1.get_low(), 12000.0);
  ASSERT_EQ(t1.get_high(), 13000.0);

  ASSERT_FALSE(t1.is_triggered(12500.0));
  ASSERT_TRUE(t1.is_triggered(13001.0));

  threshold t2("120:130");
  ASSERT_EQ(t2.get_error(), 0);

  ASSERT_EQ(t2.get_low(), 120.0);
  ASSERT_EQ(t2.get_high(), 130.0);
  t2.unit_multiplier(1.0 / 100.0);

  ASSERT_EQ(t2.get_low(), 1.2);
  ASSERT_EQ(t2.get_high(), 1.3);
}

TEST(thresholdtest, disable_value) {
  // First object.
  threshold t1("0");

  ASSERT_EQ(t1.get_error(), 0);

  ASSERT_TRUE(std::isnan(t1.get_low()));
  ASSERT_EQ(t1.get_high(), 0);

  t1.disable();

  ASSERT_FALSE(t1.is_triggered(1.0));
  ASSERT_FALSE(t1.is_triggered(-1.0));
}

TEST(thresholdtest, error_code) {
  // Invalid/edge cases should set error to 1.
  const char* cases[] = {
      "@-:3",   "3:-", "@:12",    "@a:b",       "12-34",
      "@:",     ":",   "12:3abc", "@-1.2:5foo", "1:2:3",
      "@4:5:6", "@",   " @1:2",   "@1 :2",      "@1: 2",
  };

  for (const char* c : cases) {
    threshold t(c);
    ASSERT_EQ(t.get_error(), 1) << "Case should be invalid: " << c;
  }
}
