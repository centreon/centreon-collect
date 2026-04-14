/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/http_tsdb/column.hh"
#include <gtest/gtest.h>
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

TEST(http_tsdb_column_test, Simple) {
  http_tsdb::column col("host", "test", true, http_tsdb::column::string);

  ASSERT_EQ(col.get_name(), "host");
  ASSERT_EQ(col.get_value(), "test");
  ASSERT_EQ(col.get_type(), http_tsdb::column::string);
  ASSERT_EQ(col.is_tag(), true);
}

TEST(http_tsdb_column_test, DefaultCtor) {
  http_tsdb::column col;

  ASSERT_EQ(col.get_name(), "");
  ASSERT_EQ(col.get_value(), "");
  ASSERT_EQ(col.get_type(), http_tsdb::column::number);
  ASSERT_EQ(col.is_tag(), false);
}

TEST(http_tsdb_column_test, CopyCtor) {
  http_tsdb::column col("host", "test", true, http_tsdb::column::string);
  http_tsdb::column col2{col};

  ASSERT_EQ(col2.get_name(), "host");
  ASSERT_EQ(col2.get_value(), "test");
  ASSERT_EQ(col2.get_type(), http_tsdb::column::string);
  ASSERT_EQ(col2.is_tag(), true);
}

TEST(http_tsdb_column_test, Assign) {
  http_tsdb::column col("host", "test", true, http_tsdb::column::string);
  http_tsdb::column col2;

  col2 = col;

  ASSERT_EQ(col2.get_name(), "host");
  ASSERT_EQ(col2.get_value(), "test");
  ASSERT_EQ(col2.get_type(), http_tsdb::column::string);
  ASSERT_EQ(col2.is_tag(), true);
}

TEST(http_tsdb_column_test, ParseType) {
  ASSERT_EQ(http_tsdb::column::parse_type("string"), http_tsdb::column::string);
  ASSERT_EQ(http_tsdb::column::parse_type("number"), http_tsdb::column::number);
  ASSERT_THROW(http_tsdb::column::parse_type("other"), msg_fmt);
}
