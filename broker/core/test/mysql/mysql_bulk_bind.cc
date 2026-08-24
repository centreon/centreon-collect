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
#include "com/centreon/broker/sql/mysql_bulk_bind.hh"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace com::centreon::broker::database;

/* Fills one row of a bind with two columns: an int and a string. */
static void fill_row(mysql_bulk_bind& b, const std::string& str) {
  b.set_value_as_i32(0, 12);
  b.set_value_as_str(1, str);
  b.next_row();
}

/*
 * Given a bind with two columns, an int and a string
 * When rows are added
 * Then its estimated size grows by the int, the string, its length prefix and
 * an indicator per value
 */
TEST(MysqlBulkBind, DataSizeGrowsWithRows) {
  mysql_bulk_bind b(2, 10, spdlog::default_logger());
  ASSERT_EQ(b.data_size(), 0u);
  fill_row(b, "abcd");
  const size_t row_size = (1 + sizeof(int32_t)) + (1 + 4 + 9);
  ASSERT_EQ(b.data_size(), row_size);
  b.set_null_i32(0);
  b.set_null_str(1);
  b.next_row();
  ASSERT_EQ(b.data_size(), row_size + 2);
}

/*
 * Given a bind with a size limit
 * When rows make its size reach the limit
 * Then the callback is called once, at the end of the row reaching it, and
 * the bind tells its limit is reached
 */
TEST(MysqlBulkBind, SizeLimitCallbackCalledOnce) {
  mysql_bulk_bind b(2, 10, spdlog::default_logger());
  const size_t row_size = (1 + sizeof(int32_t)) + (1 + 4 + 9);
  int called = 0;
  b.set_size_limit(row_size * 3, [&called] { ++called; });

  fill_row(b, "abcd");
  fill_row(b, "abcd");
  ASSERT_EQ(called, 0);
  ASSERT_FALSE(b.size_limit_reached());

  /* The values of the third row are set: the limit is not checked before the
   * row is complete. */
  b.set_value_as_i32(0, 12);
  b.set_value_as_str(1, "abcd");
  ASSERT_EQ(called, 0);
  b.next_row();
  ASSERT_EQ(called, 1);
  ASSERT_TRUE(b.size_limit_reached());

  fill_row(b, "abcd");
  ASSERT_EQ(called, 1);
}

/*
 * Given a bind without size limit
 * When many rows are added
 * Then its limit is never reached
 */
TEST(MysqlBulkBind, NoSizeLimit) {
  mysql_bulk_bind b(2, 10, spdlog::default_logger());
  for (int i = 0; i < 1000; ++i)
    fill_row(b, std::string(1000, 'x'));
  ASSERT_GT(b.data_size(), 1000000u);
  ASSERT_FALSE(b.size_limit_reached());
}
