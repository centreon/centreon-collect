/**
* Copyright 2011-2020 Centreon
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

#include "com/centreon/clib/version.hh"
#include <gtest/gtest.h>

using namespace com::centreon::clib;

TEST(ClibVersion, Major) {
  ASSERT_EQ(version::get_major(), version::major);
  ASSERT_EQ(CENTREON_CLIB_VERSION_MAJOR, version::major);
}

TEST(ClibVersion, Minor) {
  ASSERT_EQ(version::get_minor(), version::minor);
  ASSERT_EQ(CENTREON_CLIB_VERSION_MINOR, version::minor);
}

TEST(ClibVersion, Patch) {
  ASSERT_EQ(version::get_patch(), version::patch);
  ASSERT_EQ(CENTREON_CLIB_VERSION_PATCH, version::patch);
}

TEST(ClibVersion, String) {
  ASSERT_STREQ(version::get_string(), version::string);
  ASSERT_STREQ(CENTREON_CLIB_VERSION_STRING, version::string);
}
