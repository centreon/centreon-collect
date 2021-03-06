/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/neb/service_group.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker;

TEST(ServiceGroup, Constructor) {
  neb::service_group sg("test service_group",
                     3,
                     5,
                     true);

  ASSERT_EQ(sg.name, "test service_group");
  ASSERT_EQ(sg.poller_id, 3);
  ASSERT_EQ(sg.id, 5);
  ASSERT_EQ(sg.enabled, true);
  
}