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

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cmath>
#include <list>

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/common/perfdata.hh"

using namespace com::centreon::broker;

/**
 *  Check that the perfdata assignment operator works properly.
 */
TEST(UnifiedSqlPerfdata, Assign) {
  // First object.
  common::perfdata p1;
  p1.critical(42.0);
  p1.critical_low(-456.032);
  p1.critical_mode(false);
  p1.max(76.3);
  p1.min(567.2);
  p1.name("foo");
  p1.unit("bar");
  p1.value(52189.912);
  p1.value_type(common::perfdata::counter);
  p1.warning(4548.0);
  p1.warning_low(42.42);
  p1.warning_mode(true);

  // Second object.
  common::perfdata p2;
  p2.critical(2345678.9672374);
  p2.critical_low(-3284523786.8923);
  p2.critical_mode(true);
  p2.max(834857.9023);
  p2.min(348.239479);
  p2.name("merethis");
  p2.unit("centreon");
  p2.value(8374598345.234);
  p2.value_type(common::perfdata::absolute);
  p2.warning(0.823745784);
  p2.warning_low(NAN);
  p2.warning_mode(false);

  // Assignment.
  p2 = p1;

  // Change first object.
  p1.critical(9432.5);
  p1.critical_low(1000.0001);
  p1.critical_mode(true);
  p1.max(123.0);
  p1.min(843.876);
  p1.name("baz");
  p1.unit("qux");
  p1.value(3485.9);
  p1.value_type(common::perfdata::derive);
  p1.warning(3612.0);
  p1.warning_low(-987579.0);
  p1.warning_mode(false);

  // Check objects properties values.
  ASSERT_FALSE(fabs(p1.critical() - 9432.5) > 0.00001);
  ASSERT_FALSE(fabs(p1.critical_low() - 1000.0001) > 0.00001);
  ASSERT_FALSE(!p1.critical_mode());
  ASSERT_FALSE(fabs(p1.max() - 123.0) > 0.00001);
  ASSERT_FALSE(fabs(p1.min() - 843.876) > 0.00001);
  ASSERT_FALSE(p1.name() != "baz");
  ASSERT_FALSE(p1.unit() != "qux");
  ASSERT_FALSE(fabs(p1.value() - 3485.9) > 0.00001);
  ASSERT_FALSE(p1.value_type() != common::perfdata::derive);
  ASSERT_FALSE(fabs(p1.warning() - 3612.0) > 0.00001);
  ASSERT_FALSE(fabs(p1.warning_low() + 987579.0) > 0.01);
  ASSERT_FALSE(p1.warning_mode());
  ASSERT_FALSE(fabs(p2.critical() - 42.0) > 0.00001);
  ASSERT_FALSE(fabs(p2.critical_low() + 456.032) > 0.00001);
  ASSERT_FALSE(p2.critical_mode());
  ASSERT_FALSE(fabs(p2.max() - 76.3) > 0.00001);
  ASSERT_FALSE(fabs(p2.min() - 567.2) > 0.00001);
  ASSERT_FALSE(p2.name() != "foo");
  ASSERT_FALSE(p2.unit() != "bar");
  ASSERT_FALSE(fabs(p2.value() - 52189.912) > 0.00001);
  ASSERT_FALSE(p2.value_type() != common::perfdata::counter);
  ASSERT_FALSE(fabs(p2.warning() - 4548.0) > 0.00001);
  ASSERT_FALSE(fabs(p2.warning_low() - 42.42) > 0.00001);
  ASSERT_FALSE(!p2.warning_mode());
}

/**
 *  Check that the perfdata copy constructor works properly.
 */
TEST(UnifiedSqlPerfdata, CopyCtor) {
  // First object.
  common::perfdata p1;
  p1.critical(42.0);
  p1.critical_low(-456.032);
  p1.critical_mode(false);
  p1.max(76.3);
  p1.min(567.2);
  p1.name("foo");
  p1.unit("bar");
  p1.value(52189.912);
  p1.value_type(common::perfdata::counter);
  p1.warning(4548.0);
  p1.warning_low(42.42);
  p1.warning_mode(true);

  // Second object.
  common::perfdata p2(p1);

  // Change first object.
  p1.critical(9432.5);
  p1.critical_low(1000.0001);
  p1.critical_mode(true);
  p1.max(123.0);
  p1.min(843.876);
  p1.name("baz");
  p1.unit("qux");
  p1.value(3485.9);
  p1.value_type(common::perfdata::derive);
  p1.warning(3612.0);
  p1.warning_low(-987579.0);
  p1.warning_mode(false);

  // Check objects properties values.
  ASSERT_FALSE(fabs(p1.critical() - 9432.5) > 0.00001);
  ASSERT_FALSE(fabs(p1.critical_low() - 1000.0001) > 0.00001);
  ASSERT_FALSE(!p1.critical_mode());
  ASSERT_FALSE(fabs(p1.max() - 123.0) > 0.00001);
  ASSERT_FALSE(fabs(p1.min() - 843.876) > 0.00001);
  ASSERT_FALSE(p1.name() != "baz");
  ASSERT_FALSE(p1.unit() != "qux");
  ASSERT_FALSE(fabs(p1.value() - 3485.9) > 0.00001);
  ASSERT_FALSE(p1.value_type() != common::perfdata::derive);
  ASSERT_FALSE(fabs(p1.warning() - 3612.0) > 0.00001);
  ASSERT_FALSE(fabs(p1.warning_low() + 987579.0) > 0.01);
  ASSERT_FALSE(p1.warning_mode());
  ASSERT_FALSE(fabs(p2.critical() - 42.0) > 0.00001);
  ASSERT_FALSE(fabs(p2.critical_low() + 456.032) > 0.00001);
  ASSERT_FALSE(p2.critical_mode());
  ASSERT_FALSE(fabs(p2.max() - 76.3) > 0.00001);
  ASSERT_FALSE(fabs(p2.min() - 567.2) > 0.00001);
  ASSERT_FALSE(p2.name() != "foo");
  ASSERT_FALSE(p2.unit() != "bar");
  ASSERT_FALSE(fabs(p2.value() - 52189.912) > 0.00001);
  ASSERT_FALSE(p2.value_type() != common::perfdata::counter);
  ASSERT_FALSE(fabs(p2.warning() - 4548.0) > 0.00001);
  ASSERT_FALSE(fabs(p2.warning_low() - 42.42) > 0.00001);
  ASSERT_FALSE(!p2.warning_mode());
}

/**
 *  Check that the perfdata object properly default constructs.
 *
 *  @return 0 on success.
 */
TEST(UnifiedSqlPerfdata, DefaultCtor) {
  // Build object.
  common::perfdata p;

  // Check properties values.
  ASSERT_FALSE(!std::isnan(p.critical()));
  ASSERT_FALSE(!std::isnan(p.critical_low()));
  ASSERT_FALSE(p.critical_mode());
  ASSERT_FALSE(!std::isnan(p.max()));
  ASSERT_FALSE(!std::isnan(p.min()));
  ASSERT_FALSE(!p.name().empty());
  ASSERT_FALSE(!p.unit().empty());
  ASSERT_FALSE(!std::isnan(p.value()));
  ASSERT_FALSE(p.value_type() != common::perfdata::gauge);
  ASSERT_FALSE(!std::isnan(p.warning()));
  ASSERT_FALSE(!std::isnan(p.warning_low()));
  ASSERT_FALSE(p.warning_mode());
}

class UnifiedSqlParserParsePerfdata : public testing::Test {
 public:
  void SetUp() override {
    config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
  }
  void TearDown() override { config::applier::deinit(); };
};

// Given a misc::parser object
// When parse_perfdata() is called with a valid perfdata string
// Then perfdata are returned in a list
TEST_F(UnifiedSqlParserParsePerfdata, Simple1) {
  auto lst{common::perfdata::parse_perfdata(
      0, 0, "time=2.45698s;2.000000;5.000000;0.000000;10.000000")};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<common::perfdata>::const_iterator it(lst.begin());
  common::perfdata expected;
  expected.name("time");
  expected.value_type(common::perfdata::gauge);
  expected.value(2.45698);
  expected.unit("s");
  expected.warning(2.0);
  expected.warning_low(0.0);
  expected.critical(5.0);
  expected.critical_low(0.0);
  expected.min(0.0);
  expected.max(10.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(UnifiedSqlParserParsePerfdata, Simple2) {
  auto list{
      common::perfdata::parse_perfdata(0, 0, "'ABCD12E'=18.00%;15:;10:;0;100")};

  // Assertions.
  ASSERT_EQ(list.size(), 1u);
  std::list<common::perfdata>::const_iterator it(list.begin());
  common::perfdata expected;
  expected.name("ABCD12E");
  expected.value_type(common::perfdata::gauge);
  expected.value(18.0);
  expected.unit("%");
  expected.warning(std::numeric_limits<float>::infinity());
  expected.warning_low(15.0);
  expected.critical(std::numeric_limits<float>::infinity());
  expected.critical_low(10.0);
  expected.min(0.0);
  expected.max(100.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(UnifiedSqlParserParsePerfdata, Complex1) {
  auto list{common::perfdata::parse_perfdata(
      0, 0,
      "time=2.45698s;;nan;;inf d[metric]=239765B/s;5;;-inf; "
      "infotraffic=18x;;;; a[foo]=1234;10;11: c[bar]=1234;~:10;20:30 "
      "baz=1234;@10:20; 'q u x'=9queries_per_second;@10:;@5:;0;100")};

  // Assertions.
  ASSERT_EQ(list.size(), 7u);
  std::list<common::perfdata>::const_iterator it(list.begin());
  common::perfdata expected;

  // #1.
  expected.name("time");
  expected.value_type(common::perfdata::gauge);
  expected.value(2.45698);
  expected.unit("s");
  expected.max(std::numeric_limits<float>::infinity());
  ASSERT_TRUE(expected == *it);
  ++it;

  // #2.
  expected = common::perfdata();
  expected.name("metric");
  expected.value_type(common::perfdata::derive);
  expected.value(239765);
  expected.unit("B/s");
  expected.warning(5.0);
  expected.warning_low(0.0);
  expected.min(-std::numeric_limits<float>::infinity());
  ASSERT_TRUE(expected == *it);
  ++it;

  // #3.
  expected = common::perfdata();
  expected.name("infotraffic");
  expected.value_type(common::perfdata::gauge);
  expected.value(18.0);
  expected.unit("x");
  ASSERT_TRUE(expected == *it);
  ++it;

  // #4.
  expected = common::perfdata();
  expected.name("foo");
  expected.value_type(common::perfdata::absolute);
  expected.value(1234.0);
  expected.warning(10.0);
  expected.warning_low(0.0);
  expected.critical(std::numeric_limits<float>::infinity());
  expected.critical_low(11.0);
  ASSERT_TRUE(expected == *it);
  ++it;

  // #5.
  expected = common::perfdata();
  expected.name("bar");
  expected.value_type(common::perfdata::counter);
  expected.value(1234.0);
  expected.warning(10.0);
  expected.warning_low(-std::numeric_limits<float>::infinity());
  expected.critical(30.0);
  expected.critical_low(20.0);
  ASSERT_TRUE(expected == *it);
  ++it;

  // #6.
  expected = common::perfdata();
  expected.name("baz");
  expected.value_type(common::perfdata::gauge);
  expected.value(1234.0);
  expected.warning(20.0);
  expected.warning_low(10.0);
  expected.warning_mode(true);
  ASSERT_TRUE(expected == *it);
  ++it;

  // #7.
  expected = common::perfdata();
  expected.name("q u x");
  expected.value_type(common::perfdata::gauge);
  expected.value(9.0);
  expected.unit("queries_per_second");
  expected.warning(std::numeric_limits<float>::infinity());
  expected.warning_low(10.0);
  expected.warning_mode(true);
  expected.critical(std::numeric_limits<float>::infinity());
  expected.critical_low(5.0);
  expected.critical_mode(true);
  expected.min(0.0);
  expected.max(100.0);
  ASSERT_TRUE(expected == *it);
}

// Given a misc::parser object
// When parse_perfdata() is called multiple time with valid strings
// Then the corresponding perfdata list is returned
TEST_F(UnifiedSqlParserParsePerfdata, Loop) {
  std::list<common::perfdata> list;

  // Loop.
  for (uint32_t i(0); i < 10000; ++i) {
    // Parse perfdata string.
    list = common::perfdata::parse_perfdata(
        0, 0, "c[time]=2.45698s;2.000000;5.000000;0.000000;10.000000");

    // Assertions.
    ASSERT_EQ(list.size(), 1u);
    std::list<common::perfdata>::const_iterator it(list.begin());
    common::perfdata expected;
    expected.name("time");
    expected.value_type(common::perfdata::counter);
    expected.value(2.45698);
    expected.unit("s");
    expected.warning(2.0);
    expected.warning_low(0.0);
    expected.critical(5.0);
    expected.critical_low(0.0);
    expected.min(0.0);
    expected.max(10.0);
    ASSERT_TRUE(expected == *it);
    ++it;
  }
}

// Given a misc::parser object
// When parse_perfdata() is called with an invalid string
// Then it throws a unified_sql::exceptions::perfdata
TEST_F(UnifiedSqlParserParsePerfdata, Incorrect1) {
  // Attempt to parse perfdata.
  auto list{common::perfdata::parse_perfdata(0, 0, "metric1= 10 metric2=42")};
  ASSERT_EQ(list.size(), 1u);
  ASSERT_EQ(list.back().name(), "metric2");
  ASSERT_EQ(list.back().value(), 42);
}

// Given a misc::parser object
// When parse_perfdata() is called with a metric without value but with unit
// Then it throws a unified_sql::exceptions::perfdata
TEST_F(UnifiedSqlParserParsePerfdata, Incorrect2) {
  auto list{common::perfdata::parse_perfdata(0, 0, "metric=kb/s")};
  ASSERT_TRUE(list.empty());
}

TEST_F(UnifiedSqlParserParsePerfdata, LabelWithSpaces) {
  auto lst{common::perfdata::parse_perfdata(0, 0, "  'foo  bar   '=2s;2;5;;")};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<common::perfdata>::const_iterator it(lst.begin());
  common::perfdata expected;
  expected.name("foo  bar");
  expected.value_type(common::perfdata::gauge);
  expected.value(2);
  expected.unit("s");
  expected.warning(2.0);
  expected.warning_low(0.0);
  expected.critical(5.0);
  expected.critical_low(0.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(UnifiedSqlParserParsePerfdata, LabelWithSpacesMultiline) {
  auto lst{common::perfdata::parse_perfdata(0, 0, "  'foo  bar   '=2s;2;5;;")};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<common::perfdata>::const_iterator it(lst.begin());
  common::perfdata expected;
  expected.name("foo  bar");
  expected.value_type(common::perfdata::gauge);
  expected.value(2);
  expected.unit("s");
  expected.warning(2.0);
  expected.warning_low(0.0);
  expected.critical(5.0);
  expected.critical_low(0.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(UnifiedSqlParserParsePerfdata, Complex2) {
  auto list{common::perfdata::parse_perfdata(
      0, 0,
      "'  \n time'=2,45698s;;nan;;inf d[metric]=239765B/s;5;;-inf; "
      "g[test]=8x;;;;"
      " infotraffic=18,6x;;;; a[foo]=1234,17;10;11: "
      "c[bar]=1234,147;~:10;20:30")};

  // Assertions.
  ASSERT_EQ(list.size(), 6u);
  std::list<common::perfdata>::const_iterator it(list.begin());
  common::perfdata expected;

  // #1.
  expected.name("time");
  expected.value_type(common::perfdata::gauge);
  expected.value(2.45698);
  expected.unit("s");
  expected.max(std::numeric_limits<float>::infinity());
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #2.
  expected = common::perfdata();
  expected.name("metric");
  expected.value_type(common::perfdata::derive);
  expected.value(239765);
  expected.unit("B/s");
  expected.warning(5.0);
  expected.warning_low(0.0);
  expected.min(-std::numeric_limits<float>::infinity());
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #3.
  expected = common::perfdata();
  expected.name("test");
  expected.value_type(common::perfdata::gauge);
  expected.value(8);
  expected.unit("x");
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #4.
  expected = common::perfdata();
  expected.name("infotraffic");
  expected.value_type(common::perfdata::gauge);
  expected.value(18.6);
  expected.unit("x");
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #5.
  expected = common::perfdata();
  expected.name("foo");
  expected.value_type(common::perfdata::absolute);
  expected.value(1234.17);
  expected.warning(10.0);
  expected.warning_low(0.0);
  expected.critical(std::numeric_limits<float>::infinity());
  expected.critical_low(11.0);
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #6.
  expected = common::perfdata();
  expected.name("bar");
  expected.value_type(common::perfdata::counter);
  expected.value(1234.147);
  expected.warning(10.0);
  expected.warning_low(-std::numeric_limits<float>::infinity());
  expected.critical(30.0);
  expected.critical_low(20.0);
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;
}

// Given a misc::parser object
// When parse_perfdata() is called with a valid perfdata string
// Then perfdata are returned in a list
TEST_F(UnifiedSqlParserParsePerfdata, SimpleWithR) {
  auto lst{common::perfdata::parse_perfdata(0, 0, "'total'=5;;;0;\r")};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<common::perfdata>::const_iterator it(lst.begin());
  common::perfdata expected;
  expected.name("total");
  expected.value_type(common::perfdata::gauge);
  expected.value(5);
  expected.unit("");
  expected.warning(NAN);
  expected.warning_low(NAN);
  expected.critical(NAN);
  expected.critical_low(NAN);
  expected.min(0.0);
  expected.max(NAN);
  ASSERT_TRUE(expected == *it);
}

// Given a misc::parser object
// When parse_perfdata() is called with a valid perfdata string
// Then perfdata are returned in a list
TEST_F(UnifiedSqlParserParsePerfdata, BadMetric) {
  auto lst{
      common::perfdata::parse_perfdata(0, 0, "user1=1 user2=2 =1 user3=3")};

  // Assertions.
  ASSERT_EQ(lst.size(), 3u);
  int i = 1;
  for (auto& p : lst) {
    ASSERT_EQ(p.name(), fmt::format("user{}", i));
    ASSERT_EQ(p.value(), static_cast<float>(i));
    ++i;
  }
}

TEST_F(UnifiedSqlParserParsePerfdata, BadMetric1) {
  auto lst{
      common::perfdata::parse_perfdata(0, 0, "user1=1 user2=2 user4= user3=3")};

  // Assertions.
  ASSERT_EQ(lst.size(), 3u);
  int i = 1;
  for (auto& p : lst) {
    ASSERT_EQ(p.name(), fmt::format("user{}", i));
    ASSERT_EQ(p.value(), static_cast<float>(i));
    ++i;
  }
}
