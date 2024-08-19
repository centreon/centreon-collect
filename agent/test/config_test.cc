/**
 * Copyright 2024 Centreon
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

#include <gtest/gtest.h>
#include <filesystem>

#include "config.hh"

using namespace com::centreon::agent;

static const std::string _json_config_path =
    std::filesystem::temp_directory_path() / "config_test.json";

TEST(config, bad_format) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << "g,lezjrgerg";
  f.close();
  ASSERT_THROW(config conf(_json_config_path), std::exception);
}

TEST(config, no_endpoint) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"({"encryption":false})";
  f.close();
  ASSERT_THROW(config conf(_json_config_path), std::exception);
}

TEST(config, bad_endpoint) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"({"endpoint":"taratata"})";
  f.close();
  ASSERT_THROW(config conf(_json_config_path), std::exception);
}

TEST(config, good_endpoint) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"({"endpoint":"host1.domain2:4317"})";
  f.close();
  ASSERT_NO_THROW(config conf(_json_config_path));
}

TEST(config, bad_log_level) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"({"endpoint":"host1.domain2:4317","log_level":"erergeg"})";
  f.close();
  ASSERT_THROW(config conf(_json_config_path), std::exception);
}
