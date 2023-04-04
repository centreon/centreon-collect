/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
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

#include <gtest/gtest.h>

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::cache;

extern std::shared_ptr<asio::io_context> g_io_context;

class global_cache_test : public testing::Test {
  static void SetUpTestSuite() {
    log_v2::core()->set_level(spdlog::level::trace);
    srand(time(nullptr));
  }
};

TEST(global_cache_test, CanBeMoved) {
  global_cache::unload();
  ::remove("/tmp/cache_test");
  global_cache::pointer obj =
      global_cache::load("/tmp/cache_test", g_io_context, 0x100000);

  obj->set_metric_info(55, 1, "metric_name", "metric_unit", 1.48987, 897654.45);

  {
    global_cache::lock l;
    const metric_info* infos = obj->get_metric_info(55);

    ASSERT_NE(infos, nullptr);
    ASSERT_EQ(*infos->name, "metric_name");
    ASSERT_EQ(*infos->unit, "metric_unit");
    ASSERT_EQ(infos->min, 1.48987);
    ASSERT_EQ(infos->max, 897654.45);
  }
  const void* mapping_begin = obj->get_address();
  obj.reset();

  global_cache::unload();

  obj = global_cache::load("/tmp/cache_test", g_io_context, 0x100000,
                           ((const uint8_t*)mapping_begin));

  {
    global_cache::lock l;
    const metric_info* infos = obj->get_metric_info(55);

    ASSERT_NE(infos, nullptr);
    ASSERT_EQ(*infos->name, "metric_name");
    ASSERT_EQ(*infos->unit, "metric_unit");
    ASSERT_EQ(infos->min, 1.48987);
    ASSERT_EQ(infos->max, 897654.45);
  }
  obj.reset();

  global_cache::unload();

  obj = global_cache::load("/tmp/cache_test", g_io_context, 0x100000,
                           ((const uint8_t*)mapping_begin) - 4096);
  {
    global_cache::lock l;
    const metric_info* infos = obj->get_metric_info(55);

    ASSERT_NE(infos, nullptr);
    ASSERT_EQ(*infos->name, "metric_name");
    ASSERT_EQ(*infos->unit, "metric_unit");
    ASSERT_EQ(infos->min, 1.48987);
    ASSERT_EQ(infos->max, 897654.45);
  }
}
