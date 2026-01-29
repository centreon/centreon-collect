/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/event_script/stream.hh"
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include "bbdo/storage.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "neb.pb.h"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

using pb_service_status =
    io::protobuf<ServiceStatus, make_type(io::neb, neb::de_pb_service_status)>;
using pb_adaptive_service_status =
    io::protobuf<AdaptiveServiceStatus,
                 make_type(io::neb, neb::de_pb_adaptive_service_status)>;

class event_script_stream : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    io::events& e(io::events::instance());

    e.register_event(make_type(io::neb, neb::de_pb_service_status),
                     "ServiceStatus", &pb_service_status::operations,
                     "services");

    std::ofstream script("/tmp/event_script.sh");
    script << "#!/bin/sh" << std::endl;
    script << "echo $@ >> /tmp/script.log" << std::endl;
  }
};

TEST_F(event_script_stream, correct_args) {
  event_script::stream stream("/bin/sh /tmp/event_script.sh",
                              std::chrono::minutes(1),
                              std::chrono::seconds(10));

  std::shared_ptr<pb_service_status> event =
      std::make_shared<pb_service_status>();
  event->mut_obj().set_host_id(5);
  event->mut_obj().set_service_id(10);
  event->mut_obj().set_output("check output");

  ::remove("/tmp/script.log");

  stream.write(event);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  rapidjson::Document doc =
      com::centreon::common::rapidjson_helper::read_from_file(
          "/tmp/script.log");

  com::centreon::common::rapidjson_helper json(doc);

  ASSERT_EQ(json.get_int("hostId"), 5);
  ASSERT_EQ(json.get_int("serviceId"), 10);
  ASSERT_EQ(json.get_string("output"), std::string_view("check output"));
}

TEST_F(event_script_stream, two_same_events) {
  event_script::stream stream("/bin/sh /tmp/event_script.sh",
                              std::chrono::minutes(1),
                              std::chrono::seconds(10));

  std::shared_ptr<pb_service_status> event =
      std::make_shared<pb_service_status>();
  event->mut_obj().set_host_id(5);
  event->mut_obj().set_service_id(10);
  event->mut_obj().set_output("check output");

  ::remove("/tmp/script.log");

  stream.write(event);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  stream.write(event);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // there should be only one line in log file
  ASSERT_EQ(std::filesystem::file_size("/tmp/script.log"), 56);
}

TEST_F(event_script_stream, two_differents_events) {
  event_script::stream stream("/bin/sh /tmp/event_script.sh",
                              std::chrono::minutes(1),
                              std::chrono::seconds(10));

  std::shared_ptr<pb_service_status> event =
      std::make_shared<pb_service_status>();
  event->mut_obj().set_host_id(5);
  event->mut_obj().set_service_id(10);
  event->mut_obj().set_output("check output");

  ::remove("/tmp/script.log");

  stream.write(event);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  std::shared_ptr<pb_service_status> event2 =
      std::make_shared<pb_service_status>();
  event2->mut_obj().set_host_id(6);
  event2->mut_obj().set_service_id(10);
  event2->mut_obj().set_output("check output");
  stream.write(event2);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // there should be only one line in log file
  ASSERT_EQ(std::filesystem::file_size("/tmp/script.log"), 112);
}

TEST_F(event_script_stream, two_same_events_but_with_time_interval) {
  event_script::stream stream("/bin/sh /tmp/event_script.sh",
                              std::chrono::seconds(10),
                              std::chrono::seconds(10));

  std::shared_ptr<pb_service_status> event =
      std::make_shared<pb_service_status>();
  event->mut_obj().set_host_id(5);
  event->mut_obj().set_service_id(10);
  event->mut_obj().set_output("check output");

  ::remove("/tmp/script.log");

  stream.write(event);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::seconds(15));

  std::shared_ptr<pb_service_status> event2 =
      std::make_shared<pb_service_status>();
  event2->mut_obj().set_host_id(5);
  event2->mut_obj().set_service_id(10);
  event2->mut_obj().set_output("check output");
  stream.write(event2);

  // let time to process to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // there should be only one line in log file
  ASSERT_EQ(std::filesystem::file_size("/tmp/script.log"), 56);
}
