/**
 * Copyright 2025 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
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

#include <absl/synchronization/mutex.h>
#include <gtest/gtest.h>

#include <boost/flyweight.hpp>
#include "absl_flyweight_factory.hh"

using namespace com::centreon::common;

struct tag1 {};
struct tag2 {};

using string_flyweight_no_lock =
    boost::flyweight<std::string, absl_factory<false>>;

/**
 * @brief an no thread safe absl_flyweight, we expect that identical strings
 * point to the same buffer
 *
 */
TEST(absl_flyweight, nolock) {
  unsigned ii;
  std::vector<string_flyweight_no_lock> test1;
  for (ii = 0; ii < 100; ++ii) {
    test1.emplace_back("toto");
  }
  std::vector<string_flyweight_no_lock> test2;
  for (ii = 0; ii < 100; ++ii) {
    test2.emplace_back("titi");
  }

  const char* first = (*test1.begin())->c_str();
  ASSERT_EQ(std::string_view(first), "toto");
  for (const auto& must_be_equal : test1) {
    ASSERT_EQ(must_be_equal->c_str(), first);
  }

  first = (*test2.begin())->c_str();
  ASSERT_EQ(std::string_view(first), "titi");
  for (const auto& must_be_equal : test2) {
    ASSERT_EQ(must_be_equal->c_str(), first);
  }
}

using string_flyweight_lock = boost::flyweight<std::string, absl_factory<true>>;

/**
 * @brief Given an thread safe absl_flyweight, we expect that identical strings
 * created by several threads point to the same buffer
 *
 */
TEST(absl_flyweight, concurency) {
  std::vector<string_flyweight_lock> test1;
  absl::Mutex test1_m;
  std::vector<string_flyweight_lock> test2;
  absl::Mutex test2_m;
  std::vector<string_flyweight_lock> test3;
  absl::Mutex test3_m;

  std::vector<std::thread> threads;

  for (unsigned th_index = 0; th_index < 20; ++th_index) {
    threads.emplace_back([&]() {
      for (unsigned action_index = 0; action_index < 10000; ++action_index) {
        switch (rand() % 6) {
          case 0: {
            absl::MutexLock l(&test1_m);
            test1.emplace_back("toto");
            break;
          }
          case 1: {
            absl::MutexLock l(&test2_m);
            test2.emplace_back("titi");
            break;
          }
          case 2: {
            absl::MutexLock l(&test3_m);
            test3.emplace_back("tata");
            break;
          }
          case 3: {
            absl::MutexLock l(&test1_m);
            if (!test1.empty()) {
              test1.erase(test1.begin() + rand() % test1.size());
            }
            break;
          }
          case 4: {
            absl::MutexLock l(&test2_m);
            if (!test2.empty()) {
              test2.erase(test2.begin() + rand() % test2.size());
            }
            break;
          }
          case 5: {
            absl::MutexLock l(&test3_m);
            if (!test3.empty()) {
              test3.erase(test3.begin() + rand() % test3.size());
            }
            break;
          }
        }
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  test1.emplace_back("toto");
  test2.emplace_back("titi");
  test3.emplace_back("tata");

  const char* first = (*test1.begin())->c_str();
  ASSERT_EQ(std::string_view(first), "toto");
  for (const auto& must_be_equal : test1) {
    ASSERT_EQ(must_be_equal->c_str(), first);
  }

  first = (*test2.begin())->c_str();
  ASSERT_EQ(std::string_view(first), "titi");
  for (const auto& must_be_equal : test2) {
    ASSERT_EQ(must_be_equal->c_str(), first);
  }
  first = (*test3.begin())->c_str();
  ASSERT_EQ(std::string_view(first), "tata");
  for (const auto& must_be_equal : test3) {
    ASSERT_EQ(must_be_equal->c_str(), first);
  }
}
