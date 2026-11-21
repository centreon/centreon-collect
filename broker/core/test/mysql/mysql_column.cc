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
#include "com/centreon/broker/sql/mysql_column.hh"
#include <gtest/gtest.h>
#include <mysql.h>

using namespace com::centreon::broker::database;

/* A value set on a row previously set to NULL must not stay NULL: unified_sql
 * merges an adaptive status into a pending bulk row whose downtime depth was
 * bound to NULL. */
TEST(MysqlColumn, ValueAfterNullI32) {
  mysql_column col;
  col.set_type(MYSQL_TYPE_LONG);
  col.set_null_i32(0);
  ASSERT_FALSE(col.get_value_i32(0).has_value());
  col.set_value_i32(0, 0);
  ASSERT_TRUE(col.get_value_i32(0).has_value());
  ASSERT_EQ(*col.get_value_i32(0), 0);
}

TEST(MysqlColumn, ValueAfterNullStr) {
  mysql_column col;
  col.set_type(MYSQL_TYPE_STRING);
  col.set_value_str(0, "foo");
  col.set_null_str(0);
  ASSERT_FALSE(col.get_value_str(0).has_value());
  col.set_value_str(0, "bar");
  ASSERT_TRUE(col.get_value_str(0).has_value());
  ASSERT_EQ(*col.get_value_str(0), "bar");
}
