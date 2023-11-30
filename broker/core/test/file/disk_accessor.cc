/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/broker/file/disk_accessor.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker;

class DiskAccessor : public ::testing::Test {
 public:
  void SetUp() override { file::disk_accessor::load(30u); }
  void TearDown() override { file::disk_accessor::unload(); }
};

TEST_F(DiskAccessor, AddRemove) {
  file::disk_accessor::fd f =
      file::disk_accessor::instance().fopen("/tmp/add_remove_1.txt", "w");
  const char* txt = "hello\nbonjour\nbye\ncheers\n";
  file::disk_accessor::instance().fwrite(txt, 1, sizeof(txt), f);
  ASSERT_EQ(sizeof(txt), file::disk_accessor::instance().current_size());
  file::disk_accessor::instance().fclose(f);
  file::disk_accessor::instance().remove("/tmp/add_remove_1.txt");
  ASSERT_EQ(0u, file::disk_accessor::instance().current_size());
}
