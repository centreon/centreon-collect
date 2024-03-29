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
#include "com/centreon/broker/misc/filesystem.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker::misc::filesystem;

TEST(Filesystem, Mkpath) {
  ASSERT_TRUE(mkpath("/tmp/foo/bar/incredible"));
  ASSERT_TRUE(dir_exists("/tmp/foo/bar"));
  ASSERT_FALSE(dir_exists("/tmp/foo/incredible"));
}

TEST(Filesystem, DirContent) {
  system(
      "rm -fr /tmp/foo && mkdir -p /tmp/foo/bar/incredible "
      "&& touch /tmp/foo/a /tmp/foo/ar /tmp/foo/ca /tmp/foo/bar/aa "
      "/tmp/foo/bar/bb /tmp/foo/bar/incredible/cc");
  std::list<std::string> lst{dir_content("/tmp/foo", true)};
  ASSERT_EQ(lst.size(), 6u);
  std::list<std::string> lst1{dir_content("/tmp/foo", false)};
  ASSERT_EQ(lst1.size(), 3u);
  std::list<std::string> lst2{dir_content_with_filter("/tmp/foo", "a*")};
  ASSERT_EQ(lst2.size(), 2u);
}
