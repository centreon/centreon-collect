/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/misc/uuid.hh"

#include <gtest/gtest.h>

#include <cctype>

using namespace com::centreon::broker;

/* The remote API returns generated ids in JSON and reads them back from a URL
 * path: no terminator nor any other character outside the uuid format. */
TEST(MiscUuid, ToStringIs36PrintableCharacters) {
  for (int i = 0; i < 100; ++i) {
    std::string id = misc::Uuid().to_string();
    ASSERT_EQ(id.size(), 36u);
    EXPECT_EQ(id.find('\0'), std::string::npos);
    for (size_t j = 0; j < id.size(); ++j) {
      if (j == 8 || j == 13 || j == 18 || j == 23)
        EXPECT_EQ(id[j], '-') << id;
      else
        EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(id[j]))) << id;
    }
  }
}

TEST(MiscUuid, TwoIdsDiffer) {
  EXPECT_NE(misc::Uuid().to_string(), misc::Uuid().to_string());
}
