/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
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
#include "com/centreon/timestamp.hh"
#include <gtest/gtest.h>
#include <iostream>

using namespace com::centreon;

TEST(CLibTimestamp, Add) {
  timestamp t1(1, 42);
  timestamp t2(2, 24);

  timestamp t3(t1 + t2);
  ASSERT_EQ(t3.to_useconds(), 3000066);

  timestamp t4(-1, -24);
  timestamp t5(t2 + t4);
  ASSERT_EQ(t5.to_useconds(), 1000000);

  timestamp t6(1, 42);
  t6 += t2;
  ASSERT_EQ(t6.to_useconds(), 3000066);

  timestamp t7(2, 24);
  t7 += t4;
  ASSERT_EQ(t7.to_useconds(), 1000000);
}

TEST(CLibTimestamp, AddMs) {
  timestamp t1(1, 42);
  t1.add_mseconds(2000);
  ASSERT_EQ(t1.to_mseconds(), 3000);

  timestamp t2(1, 42);
  t2.add_mseconds(-1000);
  ASSERT_EQ(t2.to_mseconds(), 0);
}

TEST(CLibTimestamp, AddS) {
  timestamp t1(1, 42);
  t1.add_seconds(2);
  ASSERT_EQ(t1.to_seconds(), 3);

  timestamp t2(1, 42);
  t2.add_seconds(-1);
  ASSERT_EQ(t2.to_seconds(), 0);
}

TEST(CLibTimestamp, AddUs) {
  timestamp t1(1, 42);
  t1.add_useconds(2000000);
  ASSERT_EQ(t1.to_useconds(), 3000042);

  timestamp t2(1, 42);
  t2.add_useconds(-1000000);
  ASSERT_EQ(t2.to_useconds(), 42);
}

TEST(CLibTimestamp, Clear) {
  timestamp t(42, 24);
  t.clear();
  ASSERT_EQ(t, timestamp());
}

TEST(CLibTimestamp, Ctor) {
  timestamp t1;
  ASSERT_FALSE(t1.to_mseconds());
  ASSERT_FALSE(t1.to_seconds());
  ASSERT_FALSE(t1.to_useconds());

  timestamp t2(42);
  ASSERT_EQ(t2.to_seconds(), 42);

  timestamp t3(42, 24);
  ASSERT_EQ(t3.to_useconds(), 42000024);
}

TEST(CLibTimestamp, Copy) {
  timestamp t1(42, 24);

  timestamp t2(t1);
  ASSERT_TRUE(t1 == t2);

  timestamp t3 = t1;
  ASSERT_TRUE(t1 == t3);
}

TEST(CLibTimestamp, Equal) {
  timestamp t1(42, 24);

  timestamp t2(42, 24);
  ASSERT_TRUE(t1 == t2);

  timestamp t3(41, 1000024);
  ASSERT_TRUE(t3 == t2);

  timestamp t4(43, -1000000 + 24);
  ASSERT_TRUE(t4 == t2);
}

TEST(CLibTimestamp, GT) {
  timestamp t1(2, 0);

  timestamp t2(1, 0);
  ASSERT_TRUE(t1 > t2);

  timestamp t3(3, -1000);
  ASSERT_TRUE(t3 > t2);

  timestamp t4(1, -1);
  ASSERT_TRUE(t2 > t4);

  timestamp t5(-1, 0);
  ASSERT_TRUE(t1 > t5);
}

TEST(CLibTimestamp, GE) {
  timestamp t1(2, 0);

  timestamp t2(1, 0);
  ASSERT_TRUE(t1 >= t2);

  timestamp t3(3, -1000);
  ASSERT_TRUE(t3 >= t2);

  timestamp t4(1, -1);
  ASSERT_TRUE(t2 >= t4);

  timestamp t5(-1, 0);
  ASSERT_TRUE(t1 >= t5);

  timestamp t6(0, 1000000);
  ASSERT_TRUE(t6 >= t2);

  timestamp t7(1, 0);
  ASSERT_TRUE(t7 >= t2);
}

TEST(CLibTimestamp, LT) {
  timestamp t1(2, 0);

  timestamp t2(1, 0);
  ASSERT_FALSE(t1 < t2);

  timestamp t3(3, -1000);
  ASSERT_FALSE(t3 < t2);

  timestamp t4(1, -1);
  ASSERT_FALSE(t2 < t4);

  timestamp t5(-1, 0);
  ASSERT_FALSE(t1 < t5);
}

TEST(CLibTimestamp, LE) {
  timestamp t1(2, 0);

  timestamp t2(1, 0);
  ASSERT_FALSE(t1 <= t2);

  timestamp t3(3, -1000);
  ASSERT_FALSE(t3 <= t2);

  timestamp t4(1, -1);
  ASSERT_FALSE(t2 <= t4);

  timestamp t5(-1, 0);
  ASSERT_FALSE(t1 <= t5);

  timestamp t6(0, 1000000);
  ASSERT_TRUE(t6 <= t2);

  timestamp t7(1, 0);
  ASSERT_TRUE(t7 <= t2);
}

TEST(CLibTimestamp, NE) {
  timestamp t1(42, 24);
  timestamp t2(42, 42);

  ASSERT_FALSE(t1 == t2);
}

TEST(CLibTimestamp, Sub) {
  timestamp t1(1, 42);
  timestamp t2(2, 24);

  timestamp t3(t2 - t1);
  ASSERT_EQ(t3.to_useconds(), 999982);

  timestamp t4(-1, -24);
  timestamp t5(t2 - t4);
  ASSERT_EQ(t5.to_useconds(), 3000048);

  timestamp t6(2, 24);
  t6 -= t1;
  ASSERT_EQ(t6.to_useconds(), 999982);

  timestamp t7(2, 24);
  t7 -= t4;
  ASSERT_EQ(t7.to_useconds(), 3000048);
}

TEST(CLibTimestamp, SubMs) {
  timestamp t1(2, 42);
  t1.sub_mseconds(1000);
  ASSERT_EQ(t1.to_mseconds(), 1000);

  timestamp t2(1, 42);
  t2.sub_mseconds(-1000);
  ASSERT_EQ(t2.to_mseconds(), 2000);
}

TEST(CLibTimestamp, SubS) {
  timestamp t1(2, 42);
  t1.sub_seconds(1);
  ASSERT_EQ(t1.to_seconds(), 1);

  timestamp t2(1, 42);
  t2.sub_seconds(-1);
  ASSERT_EQ(t2.to_seconds(), 2);
}

TEST(CLibTimestamp, SubUs) {
  timestamp t1(2, 42);
  t1.sub_useconds(1000000);
  ASSERT_EQ(t1.to_useconds(), 1000042);

  timestamp t2(1, 42);
  t2.sub_useconds(-1000000);
  ASSERT_EQ(t2.to_useconds(), 2000042);
}

TEST(CLibTimestamp, ToMs) {
  timestamp t1(1, 42);
  ASSERT_EQ(t1.to_mseconds(), 1000);

  timestamp t2(-1, 0);
  ASSERT_EQ(t2.to_mseconds(), -1000);

  timestamp t3(0, -42);
  ASSERT_EQ(t3.to_mseconds(), -1);

  timestamp t4(-1, -42);
  ASSERT_EQ(t4.to_mseconds(), -1001);

  timestamp t5(1, -42);
  ASSERT_EQ(t5.to_mseconds(), 999);
}

TEST(CLibTimestamp, ToS) {
  timestamp t1(1, 42);
  ASSERT_EQ(t1.to_seconds(), 1);

  timestamp t2(-1, 0);
  ASSERT_EQ(t2.to_seconds(), -1);

  timestamp t3(0, -42);
  ASSERT_EQ(t3.to_seconds(), -1);

  timestamp t4(-1, -42);
  ASSERT_EQ(t4.to_seconds(), -2);

  timestamp t5(1, -42);
  ASSERT_EQ(t5.to_seconds(), 0);
}

TEST(CLibTimestamp, ToUs) {
  timestamp t1(1, 42);
  ASSERT_EQ(t1.to_useconds(), 1000042);

  timestamp t2(-1, 0);
  ASSERT_EQ(t2.to_useconds(), -1000000);

  timestamp t3(0, -42);
  ASSERT_EQ(t3.to_useconds(), -42);

  timestamp t4(-1, -42);
  ASSERT_EQ(t4.to_useconds(), -1000042);

  timestamp t5(1, -42);
  ASSERT_EQ(t5.to_useconds(), 999958);
}
