/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/parser.hh"
#include <gtest/gtest.h>
#include "com/centreon/common/file.hh"

using namespace com::centreon::engine::configuration;
using namespace com::centreon::common;

class TestParser : public ::testing::Test {
 public:
  //  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TestParser, build_test_file_test) {
  std::filesystem::path centengine_cfg("/tmp/centengine.cfg");
  std::string centengine_cfg_content =
      "# A comment\n"
      "cfg_file=/etc/centreon-engine/first.cfg\n"
      "bar=foo\n"
      "resource_file=/etc/centreon-engine/resource.cfg\n"
      "cfg_file=/etc/centreon-engine/second.cfg\n"
      "foo=bar\n";
  std::ofstream f(centengine_cfg);
  f << centengine_cfg_content;
  f.close();
  std::filesystem::path test_centengine_cfg("/tmp/test_centengine.cfg");
  std::error_code ec;

  parser::build_test_file(centengine_cfg, test_centengine_cfg, ec);
  ASSERT_FALSE(ec);

  std::string content;
  ASSERT_NO_THROW(content = read_file_content(test_centengine_cfg));
  ASSERT_EQ(content, std::string("# A comment\n"
                                 "cfg_file=/tmp/first.cfg\n"
                                 "bar=foo\n"
                                 "resource_file=/tmp/resource.cfg\n"
                                 "cfg_file=/tmp/second.cfg\n"
                                 "foo=bar\n"));
}
