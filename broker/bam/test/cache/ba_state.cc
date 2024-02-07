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
#include "com/centreon/broker/bam/ba_best.hh"
#include "com/centreon/broker/bam/ba_worst.hh"
#include "com/centreon/broker/bam/kpi_service.hh"

using namespace com::centreon::broker;

// TEST(BaStateTest, BaWorstWithServiceKpi) {
//   std::shared_ptr<bam::ba> test_ba{
//       std::make_shared<bam::ba_worst>(1, 5, 13, true)};
//   test_ba->set_name("test-ba");
//   test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);
//
//     auto s = std::make_shared<bam::kpi_service>(1, 2, 3, 1, "service");
//     s->set_downtimed(false);
//     s->set_impact_critical(100);
//     s->set_impact_unknown(0);
//     s->set_impact_warning(75);
//     s->set_state_hard(bam::state_ok);
//     s->set_state_type(1);
//
//     test_ba->add_impact(s);
//     s->add_parent(test_ba);
//     BaState state;
//     test_ba->internal_save_current_state(&state);
//     std::string output;
//     google::protobuf::util::MessageToJsonString(state, &output);
//     ASSERT_EQ(output, std::string_view(""));
// }
//
// TEST(BaStateTest, BaBestWithServiceKpi) {
//   std::shared_ptr<bam::ba> test_ba{
//       std::make_shared<bam::ba_best>(1, 5, 13, true)};
//   test_ba->set_name("test-ba");
//   test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);
//
//     auto s = std::make_shared<bam::kpi_service>(1, 2, 3, 1, "service");
//     s->set_downtimed(false);
//     s->set_impact_critical(100);
//     s->set_impact_unknown(0);
//     s->set_impact_warning(75);
//     s->set_state_hard(bam::state_ok);
//     s->set_state_type(1);
//
//     test_ba->add_impact(s);
//     s->add_parent(test_ba);
//     BaState state;
//     test_ba->internal_save_current_state(&state);
//     std::string output;
//     google::protobuf::util::MessageToJsonString(state, &output);
//     ASSERT_EQ(output, std::string_view(""));
// }
