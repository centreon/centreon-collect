/*
 * Copyright 2020-2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/commands/environment.hh"
#include "com/centreon/engine/string.hh"

#include "gtest/gtest.h"

using namespace com::centreon::engine::commands;

static std::vector<absl::string_view> to_vector(char** data) {
  std::vector<absl::string_view> retval;
  if (data == nullptr)
    return retval;
  int i = 0;
  while (*(data + i) != nullptr) {
    retval.emplace_back(*(data + i));
    i++;
  }
  return retval;
}

TEST(env_utils, add_line) {
  environment* env = new environment();
  env->add("foo");
  env->add("bar");
  auto v = to_vector(env->data());
  ASSERT_EQ("foo", v[0]);
  ASSERT_EQ("bar", v[1]);
  ASSERT_TRUE(v.size() == 2);
}

TEST(env_utils, add_line_string) {
  environment* env = new environment();
  const std::string& s1{"foo"};
  const std::string& s2{"bar"};
  env->add(s1);
  env->add(s2);
  auto v = to_vector(env->data());
  ASSERT_EQ("foo", v[0]);
  ASSERT_EQ("bar", v[1]);
  ASSERT_TRUE(v.size() == 2);
}

TEST(env_utils, add_line_empty) {
  environment* env = new environment();
  env->add(nullptr);
  auto v = to_vector(env->data());
  ASSERT_TRUE(v.size() == 0);
}

TEST(env_utils, add_name_value_empty) {
  environment* env = new environment();
  env->add(nullptr, nullptr);
  auto v = to_vector(env->data());
  ASSERT_TRUE(v.size() == 0);
}

TEST(env_utils, add_name_value) {
  environment* env = new environment();
  env->add("foo", "bar");
  env->add("toto", "");
  auto v = to_vector(env->data());
  ASSERT_EQ("foo=bar", v[0]);
  ASSERT_EQ("toto=", v[1]);
  ASSERT_TRUE(v.size() == 2);
}

TEST(env_utils, add_name_value_string) {
  environment* env = new environment();
  const std::string& s1{"foo"};
  const std::string& s2{"bar"};
  const std::string& s3{"toto"};
  const std::string& s4{""};
  env->add(s1, s2);
  env->add(s3, s4);
  auto v = to_vector(env->data());
  ASSERT_EQ("foo=bar", v[0]);
  ASSERT_EQ("toto=", v[1]);
  ASSERT_TRUE(v.size() == 2);
}
