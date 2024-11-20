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

#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cmath>

#include "perfdata.hh"

using namespace com::centreon;
using namespace com::centreon::common;

/**
 *  Check that the perfdata assignment operator works properly.
 */
TEST(PerfData, Assign) {
  // First object.
  perfdata p1;
  p1.critical(42.0);
  p1.critical_low(-456.032);
  p1.critical_mode(false);
  p1.max(76.3);
  p1.min(567.2);
  p1.name("foo");
  p1.unit("bar");
  p1.value(52189.912);
  p1.value_type(perfdata::counter);
  p1.warning(4548.0);
  p1.warning_low(42.42);
  p1.warning_mode(true);

  // Second object.
  perfdata p2;
  p2.critical(2345678.9672374);
  p2.critical_low(-3284523786.8923);
  p2.critical_mode(true);
  p2.max(834857.9023);
  p2.min(348.239479);
  p2.name("merethis");
  p2.unit("centreon");
  p2.value(8374598345.234);
  p2.value_type(perfdata::absolute);
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
  p1.value_type(perfdata::derive);
  p1.warning(3612.0);
  p1.warning_low(-987579.0);
  p1.warning_mode(false);

  // Check objects properties values.
  ASSERT_FALSE(fabs(p1.critical() - 9432.5f) > 0.00001f);
  ASSERT_TRUE(fabs(p1.critical_low() - 1000.0001f) < 0.0001f);
  ASSERT_FALSE(!p1.critical_mode());
  ASSERT_FALSE(fabs(p1.max() - 123.0f) > 0.00001f);
  ASSERT_TRUE(fabs(p1.min() - 843.876f) < 0.0001f);
  ASSERT_FALSE(p1.name() != "baz");
  ASSERT_FALSE(p1.unit() != "qux");
  ASSERT_TRUE(fabs(p1.value() - 3485.9f) < 0.0001f);
  ASSERT_FALSE(p1.value_type() != perfdata::derive);
  ASSERT_FALSE(fabs(p1.warning() - 3612.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p1.warning_low() + 987579.0f) > 0.01f);
  ASSERT_FALSE(p1.warning_mode());
  ASSERT_FALSE(fabs(p2.critical() - 42.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p2.critical_low() + 456.032f) > 0.00001f);
  ASSERT_FALSE(p2.critical_mode());
  ASSERT_FALSE(fabs(p2.max() - 76.3f) > 0.00001f);
  ASSERT_FALSE(fabs(p2.min() - 567.2f) > 0.00001f);
  ASSERT_FALSE(p2.name() != "foo");
  ASSERT_FALSE(p2.unit() != "bar");
  ASSERT_FALSE(fabs(p2.value() - 52189.912f) > 0.00001f);
  ASSERT_FALSE(p2.value_type() != perfdata::counter);
  ASSERT_FALSE(fabs(p2.warning() - 4548.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p2.warning_low() - 42.42f) > 0.00001f);
  ASSERT_FALSE(!p2.warning_mode());
}

/**
 *  Check that the perfdata copy constructor works properly.
 */
TEST(PerfData, CopyCtor) {
  // First object.
  perfdata p1;
  p1.critical(42.0);
  p1.critical_low(-456.032);
  p1.critical_mode(false);
  p1.max(76.3);
  p1.min(567.2);
  p1.name("foo");
  p1.unit("bar");
  p1.value(52189.912);
  p1.value_type(perfdata::counter);
  p1.warning(4548.0);
  p1.warning_low(42.42);
  p1.warning_mode(true);

  // Second object.
  perfdata p2(p1);

  // Change first object.
  p1.critical(9432.5);
  p1.critical_low(1000.0001);
  p1.critical_mode(true);
  p1.max(123.0);
  p1.min(843.876);
  p1.name("baz");
  p1.unit("qux");
  p1.value(3485.9);
  p1.value_type(perfdata::derive);
  p1.warning(3612.0);
  p1.warning_low(-987579.0);
  p1.warning_mode(false);

  // Check objects properties values.
  ASSERT_FALSE(fabs(p1.critical() - 9432.5f) > 0.00001f);
  ASSERT_FALSE(fabs(p1.critical_low() - 1000.0001f) > 0.00001f);
  ASSERT_FALSE(!p1.critical_mode());
  ASSERT_FALSE(fabs(p1.max() - 123.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p1.min() - 843.876f) > 0.00001f);
  ASSERT_FALSE(p1.name() != "baz");
  ASSERT_FALSE(p1.unit() != "qux");
  ASSERT_FALSE(fabs(p1.value() - 3485.9f) > 0.00001f);
  ASSERT_FALSE(p1.value_type() != perfdata::derive);
  ASSERT_FALSE(fabs(p1.warning() - 3612.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p1.warning_low() + 987579.0f) > 0.01f);
  ASSERT_FALSE(p1.warning_mode());
  ASSERT_FALSE(fabs(p2.critical() - 42.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p2.critical_low() + 456.032f) > 0.00001f);
  ASSERT_FALSE(p2.critical_mode());
  ASSERT_FALSE(fabs(p2.max() - 76.3f) > 0.00001f);
  ASSERT_FALSE(fabs(p2.min() - 567.2f) > 0.00001f);
  ASSERT_FALSE(p2.name() != "foo");
  ASSERT_FALSE(p2.unit() != "bar");
  ASSERT_FALSE(fabs(p2.value() - 52189.912f) > 0.00001f);
  ASSERT_FALSE(p2.value_type() != perfdata::counter);
  ASSERT_FALSE(fabs(p2.warning() - 4548.0f) > 0.00001f);
  ASSERT_FALSE(fabs(p2.warning_low() - 42.42f) > 0.00001f);
  ASSERT_FALSE(!p2.warning_mode());
}

/**
 *  Check that the perfdata object properly default constructs.
 *
 *  @return 0 on success.
 */
TEST(PerfData, DefaultCtor) {
  // Build object.
  perfdata p;

  // Check properties values.
  ASSERT_FALSE(!std::isnan(p.critical()));
  ASSERT_FALSE(!std::isnan(p.critical_low()));
  ASSERT_FALSE(p.critical_mode());
  ASSERT_FALSE(!std::isnan(p.max()));
  ASSERT_FALSE(!std::isnan(p.min()));
  ASSERT_FALSE(!p.name().empty());
  ASSERT_FALSE(!p.unit().empty());
  ASSERT_FALSE(!std::isnan(p.value()));
  ASSERT_FALSE(p.value_type() != perfdata::gauge);
  ASSERT_FALSE(!std::isnan(p.warning()));
  ASSERT_FALSE(!std::isnan(p.warning_low()));
  ASSERT_FALSE(p.warning_mode());
}

class PerfdataParser : public testing::Test {
 protected:
  static std::shared_ptr<spdlog::logger> _logger;
};

std::shared_ptr<spdlog::logger> PerfdataParser::_logger =
    spdlog::stdout_color_mt("perfdata_test");

// Given a misc::parser object
// When parse_perfdata() is called with a valid perfdata string
// Then perfdata are returned in a list
TEST_F(PerfdataParser, Simple1) {
  // Parse perfdata.
  std::list<common::perfdata> lst{common::perfdata::parse_perfdata(
      0, 0, "time=2.45698s;2.000000;5.000000;0.000000;10.000000", _logger)};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<perfdata>::const_iterator it(lst.begin());
  perfdata expected;
  expected.name("time");
  expected.value_type(perfdata::gauge);
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

TEST_F(PerfdataParser, Simple2) {
  // Parse perfdata.
  std::list<common::perfdata> list{common::perfdata::parse_perfdata(
      0, 0, "'ABCD12E'=18.00%;15:;10:;0;100", _logger)};

  // Assertions.
  ASSERT_EQ(list.size(), 1u);
  std::list<perfdata>::const_iterator it(list.begin());
  perfdata expected;
  expected.name("ABCD12E");
  expected.value_type(perfdata::gauge);
  expected.value(18.0);
  expected.unit("%");
  expected.warning(std::numeric_limits<double>::infinity());
  expected.warning_low(15.0);
  expected.critical(std::numeric_limits<double>::infinity());
  expected.critical_low(10.0);
  expected.min(0.0);
  expected.max(100.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(PerfdataParser, SeveralIdenticalMetrics) {
  // Parse perfdata.
  std::list<common::perfdata> list{common::perfdata::parse_perfdata(
      0, 0, "'et'=18.00%;15:;10:;0;100 other=15 et=13.00%", _logger)};

  // Assertions.
  ASSERT_EQ(list.size(), 2u);
  std::list<perfdata>::const_iterator it = list.begin();
  perfdata expected;
  expected.name("et");
  expected.value_type(perfdata::gauge);
  expected.value(18.0);
  expected.unit("%");
  expected.warning(std::numeric_limits<double>::infinity());
  expected.warning_low(15.0);
  expected.critical(std::numeric_limits<double>::infinity());
  expected.critical_low(10.0);
  expected.min(0.0);
  expected.max(100.0);
  ASSERT_TRUE(expected == *it);
  ++it;
  ASSERT_EQ(it->name(), std::string_view("other"));
  ASSERT_EQ(it->value(), 15);
  ASSERT_EQ(it->value_type(), perfdata::gauge);
}

TEST_F(PerfdataParser, ComplexSeveralIdenticalMetrics) {
  // Parse perfdata.
  std::list<common::perfdata> list{common::perfdata::parse_perfdata(
      0, 0, "'d[foo]'=18.00%;15:;10:;0;100 other=15 a[foo]=13.00%", _logger)};

  // Assertions.
  ASSERT_EQ(list.size(), 2u);
  std::list<perfdata>::const_iterator it = list.begin();
  perfdata expected;
  expected.name("foo");
  expected.value_type(perfdata::derive);
  expected.value(18.0);
  expected.unit("%");
  expected.warning(std::numeric_limits<double>::infinity());
  expected.warning_low(15.0);
  expected.critical(std::numeric_limits<double>::infinity());
  expected.critical_low(10.0);
  expected.min(0.0);
  expected.max(100.0);
  ASSERT_TRUE(expected == *it);
  ++it;
  ASSERT_EQ(it->name(), std::string_view("other"));
  ASSERT_EQ(it->value(), 15);
  ASSERT_EQ(it->value_type(), perfdata::gauge);
}

TEST_F(PerfdataParser, Complex1) {
  // Parse perfdata.
  std::list<perfdata> list{perfdata::parse_perfdata(
      0, 0,
      "time=2.45698s;;nan;;inf d[metric]=239765B/s;5;;-inf; "
      "infotraffic=18x;;;; a[foo]=1234;10;11: c[bar]=1234;~:10;20:30 "
      "baz=1234;@10:20; 'q u x'=9queries_per_second;@10:;@5:;0;100",
      _logger)};

  // Assertions.
  ASSERT_EQ(list.size(), 7u);
  std::list<perfdata>::const_iterator it(list.begin());
  perfdata expected;

  // #1.
  expected.name("time");
  expected.value_type(perfdata::gauge);
  expected.value(2.45698);
  expected.unit("s");
  expected.max(std::numeric_limits<double>::infinity());
  ASSERT_TRUE(expected == *it);
  ++it;

  // #2.
  expected = perfdata();
  expected.name("metric");
  expected.value_type(perfdata::derive);
  expected.value(239765);
  expected.unit("B/s");
  expected.warning(5.0);
  expected.warning_low(0.0);
  expected.min(-std::numeric_limits<double>::infinity());
  ASSERT_TRUE(expected == *it);
  ++it;

  // #3.
  expected = perfdata();
  expected.name("infotraffic");
  expected.value_type(perfdata::gauge);
  expected.value(18.0);
  expected.unit("x");
  ASSERT_TRUE(expected == *it);
  ++it;

  // #4.
  expected = perfdata();
  expected.name("foo");
  expected.value_type(perfdata::absolute);
  expected.value(1234.0);
  expected.warning(10.0);
  expected.warning_low(0.0);
  expected.critical(std::numeric_limits<double>::infinity());
  expected.critical_low(11.0);
  ASSERT_TRUE(expected == *it);
  ++it;

  // #5.
  expected = perfdata();
  expected.name("bar");
  expected.value_type(perfdata::counter);
  expected.value(1234.0);
  expected.warning(10.0);
  expected.warning_low(-std::numeric_limits<double>::infinity());
  expected.critical(30.0);
  expected.critical_low(20.0);
  ASSERT_TRUE(expected == *it);
  ++it;

  // #6.
  expected = perfdata();
  expected.name("baz");
  expected.value_type(perfdata::gauge);
  expected.value(1234.0);
  expected.warning(20.0);
  expected.warning_low(10.0);
  expected.warning_mode(true);
  ASSERT_TRUE(expected == *it);
  ++it;

  // #7.
  expected = perfdata();
  expected.name("q u x");
  expected.value_type(perfdata::gauge);
  expected.value(9.0);
  expected.unit("queries_per_second");
  expected.warning(std::numeric_limits<double>::infinity());
  expected.warning_low(10.0);
  expected.warning_mode(true);
  expected.critical(std::numeric_limits<double>::infinity());
  expected.critical_low(5.0);
  expected.critical_mode(true);
  expected.min(0.0);
  expected.max(100.0);
  ASSERT_TRUE(expected == *it);
}

// Given a misc::parser object
// When parse_perfdata() is called multiple time with valid strings
// Then the corresponding perfdata list is returned
TEST_F(PerfdataParser, Loop) {
  // Objects.
  std::list<perfdata> list;

  // Loop.
  for (uint32_t i(0); i < 10000; ++i) {
    // Parse perfdata string.
    list = common::perfdata::parse_perfdata(
        0, 0, "c[time]=2.45698s;2.000000;5.000000;0.000000;10.000000", _logger);

    // Assertions.
    ASSERT_EQ(list.size(), 1u);
    std::list<perfdata>::const_iterator it(list.begin());
    perfdata expected;
    expected.name("time");
    expected.value_type(perfdata::counter);
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
TEST_F(PerfdataParser, Incorrect1) {
  // Attempt to parse perfdata.
  auto list{common::perfdata::parse_perfdata(0, 0, "metric1= 10 metric2=42",
                                             _logger)};
  ASSERT_EQ(list.size(), 1u);
  ASSERT_EQ(list.back().name(), "metric2");
  ASSERT_EQ(list.back().value(), 42);
}

// Given a misc::parser object
// When parse_perfdata() is called with a metric without value but with unit
TEST_F(PerfdataParser, Incorrect2) {
  // Then
  auto list{common::perfdata::parse_perfdata(0, 0, "metric=kb/s", _logger)};
  ASSERT_TRUE(list.empty());
}

TEST_F(PerfdataParser, LabelWithSpaces) {
  // Parse perfdata.
  auto lst{common::perfdata::parse_perfdata(0, 0, "  'foo  bar   '=2s;2;5;;",
                                            _logger)};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<perfdata>::const_iterator it(lst.begin());
  perfdata expected;
  expected.name("foo  bar");
  expected.value_type(perfdata::gauge);
  expected.value(2);
  expected.unit("s");
  expected.warning(2.0);
  expected.warning_low(0.0);
  expected.critical(5.0);
  expected.critical_low(0.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(PerfdataParser, LabelWithSpacesMultiline) {
  // Parse perfdata.
  auto lst{common::perfdata::parse_perfdata(0, 0, "  'foo  bar   '=2s;2;5;;",
                                            _logger)};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<perfdata>::const_iterator it(lst.begin());
  perfdata expected;
  expected.name("foo  bar");
  expected.value_type(perfdata::gauge);
  expected.value(2);
  expected.unit("s");
  expected.warning(2.0);
  expected.warning_low(0.0);
  expected.critical(5.0);
  expected.critical_low(0.0);
  ASSERT_TRUE(expected == *it);
}

TEST_F(PerfdataParser, Complex2) {
  // Parse perfdata.
  auto list{perfdata::parse_perfdata(
      0, 0,
      "'  \n time'=2,45698s;;nan;;inf d[metric]=239765B/s;5;;-inf; "
      "g[test]=8x;;;;"
      " infotraffic=18,6x;;;; a[foo]=1234,17;10;11: "
      "c[bar]=1234,147;~:10;20:30",
      _logger)};

  // Assertions.
  ASSERT_EQ(list.size(), 6u);
  std::list<perfdata>::const_iterator it(list.begin());
  perfdata expected;

  // #1.
  expected.name("time");
  expected.value_type(perfdata::gauge);
  expected.value(2.45698);
  expected.unit("s");
  expected.max(std::numeric_limits<double>::infinity());
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #2.
  expected = perfdata();
  expected.name("metric");
  expected.value_type(perfdata::derive);
  expected.value(239765);
  expected.unit("B/s");
  expected.warning(5.0);
  expected.warning_low(0.0);
  expected.min(-std::numeric_limits<double>::infinity());
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #3.
  expected = perfdata();
  expected.name("test");
  expected.value_type(perfdata::gauge);
  expected.value(8);
  expected.unit("x");
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #4.
  expected = perfdata();
  expected.name("infotraffic");
  expected.value_type(perfdata::gauge);
  expected.value(18.6);
  expected.unit("x");
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #5.
  expected = perfdata();
  expected.name("foo");
  expected.value_type(perfdata::absolute);
  expected.value(1234.17);
  expected.warning(10.0);
  expected.warning_low(0.0);
  expected.critical(std::numeric_limits<double>::infinity());
  expected.critical_low(11.0);
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;

  // #6.
  expected = perfdata();
  expected.name("bar");
  expected.value_type(perfdata::counter);
  expected.value(1234.147);
  expected.warning(10.0);
  expected.warning_low(-std::numeric_limits<double>::infinity());
  expected.critical(30.0);
  expected.critical_low(20.0);
  ASSERT_TRUE(expected == *it);
  ASSERT_FALSE(expected != *it);
  ++it;
}

// Given a misc::parser object
// When parse_perfdata() is called with a valid perfdata string
// Then perfdata are returned in a list
TEST_F(PerfdataParser, SimpleWithR) {
  auto lst{common::perfdata::parse_perfdata(0, 0, "'total'=5;;;0;\r", _logger)};

  // Assertions.
  ASSERT_EQ(lst.size(), 1u);
  std::list<perfdata>::const_iterator it(lst.begin());
  perfdata expected;
  expected.name("total");
  expected.value_type(perfdata::gauge);
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
TEST_F(PerfdataParser, BadMetric) {
  auto lst{common::perfdata::parse_perfdata(0, 0, "user1=1 user2=2 =1 user3=3",
                                            _logger)};

  // Assertions.
  ASSERT_EQ(lst.size(), 3u);
  int i = 1;
  for (auto& p : lst) {
    ASSERT_EQ(p.name(), fmt::format("user{}", i));
    ASSERT_EQ(p.value(), static_cast<double>(i));
    ++i;
  }
}

TEST_F(PerfdataParser, BadMetric1) {
  auto lst{common::perfdata::parse_perfdata(
      0, 0, "user1=1 user2=2 user4= user3=3", _logger)};

  // Assertions.
  ASSERT_EQ(lst.size(), 3u);
  int i = 1;
  for (auto& p : lst) {
    ASSERT_EQ(p.name(), fmt::format("user{}", i));
    ASSERT_EQ(p.value(), static_cast<double>(i));
    ++i;
  }
}

TEST_F(PerfdataParser, ExtractPerfdataBrackets) {
  std::string perfdata(
      "'xx[aa a aa]'=2;3;7;1;9 '[a aa]'=12;25;50;0;118 'aa a]'=28;13;54;0;80");
  auto lst{common::perfdata::parse_perfdata(0, 0, perfdata.c_str(), _logger)};
  auto it = lst.begin();
  ASSERT_NE(it, lst.end());
  ASSERT_EQ(it->name(), "xx[aa a aa]");
  ++it;
  ASSERT_NE(it, lst.end());
  ASSERT_EQ(it->name(), "[a aa]");
  ++it;
  ASSERT_NE(it, lst.end());
  ASSERT_EQ(it->name(), "aa a]");
}
