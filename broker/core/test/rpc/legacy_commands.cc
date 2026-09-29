/**
 * Copyright 2026 Centreon
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

#include "broker/core/brokerrpc/legacy_commands.hh"

#include <gtest/gtest.h>

using namespace com::centreon::broker;
namespace lc = legacy_commands;

TEST(LegacyCommands, SplitArgsKeepsTheRestInTheLastField) {
  lc::fields f = lc::split_args("a;b;c;d;e", 3);
  ASSERT_EQ(f.size(), 3u);
  EXPECT_EQ(f[2], "c;d;e");
  EXPECT_EQ(lc::split_args("a;b", 3).size(), 2u);
  EXPECT_TRUE(lc::split_args("", 3).empty());
}

TEST(LegacyCommands, Acknowledgement) {
  AcknowledgementRequest req;
  ASSERT_TRUE(lc::acknowledgement(
                  "host_1;service_1;2;1;0;admin;down for a; while", true, &req)
                  .ok());
  EXPECT_EQ(req.host_name(), "host_1");
  EXPECT_EQ(req.service_desc(), "service_1");
  EXPECT_EQ(req.type(), AcknowledgementRequest::STICKY);
  EXPECT_TRUE(req.notify());
  EXPECT_FALSE(req.persistent());
  EXPECT_EQ(req.ack_author(), "admin");
  EXPECT_EQ(req.ack_data(), "down for a; while");

  AcknowledgementRequest hreq;
  ASSERT_TRUE(lc::acknowledgement("host_1;1;0;1;admin;ack", false, &hreq).ok());
  EXPECT_EQ(hreq.type(), AcknowledgementRequest::NORMAL);
  EXPECT_TRUE(hreq.service_desc().empty());
  EXPECT_TRUE(hreq.persistent());

  grpc::Status st = lc::acknowledgement("host_1;1;0;1;admin", false, &hreq);
  EXPECT_EQ(st.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_NE(st.error_message().find("comment"), std::string::npos);
  st = lc::acknowledgement("host_1;x;0;1;admin;c", false, &hreq);
  EXPECT_EQ(st.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_NE(st.error_message().find("type"), std::string::npos);
}

TEST(LegacyCommands, Comments) {
  HostCommentRequest h;
  ASSERT_TRUE(lc::host_comment("host_1;1;admin;a;b;c", &h).ok());
  EXPECT_EQ(h.host().host_name(), "host_1");
  EXPECT_TRUE(h.persistent());
  EXPECT_EQ(h.user(), "admin");
  EXPECT_EQ(h.comment_data(), "a;b;c");

  ServiceCommentRequest s;
  ASSERT_TRUE(lc::service_comment("host_1;service_1;0;bob;hello", &s).ok());
  EXPECT_EQ(s.service().host_name(), "host_1");
  EXPECT_EQ(s.service().description(), "service_1");
  EXPECT_FALSE(s.persistent());
  EXPECT_EQ(s.comment_data(), "hello");

  CommentIdentifier id;
  ASSERT_TRUE(lc::comment_identifier("42", &id).ok());
  EXPECT_EQ(id.internal_id(), 42u);
  EXPECT_EQ(lc::comment_identifier("abc", &id).error_code(),
            grpc::StatusCode::INVALID_ARGUMENT);
}

TEST(LegacyCommands, Downtimes) {
  ScheduleDowntimeRequest r;
  ASSERT_TRUE(
      lc::schedule_downtime(
          "host_1;service_1;1000;2000;1;0;3600;admin;maint;window", true, &r)
          .ok());
  EXPECT_EQ(r.type(), ScheduleDowntimeRequest::SERVICE);
  EXPECT_EQ(r.host_name(), "host_1");
  EXPECT_EQ(r.service_description(), "service_1");
  EXPECT_EQ(r.start_time(), 1000);
  EXPECT_EQ(r.end_time(), 2000);
  EXPECT_TRUE(r.fixed());
  EXPECT_EQ(r.triggered_by(), 0u);
  EXPECT_EQ(r.duration(), 3600u);
  EXPECT_EQ(r.author(), "admin");
  EXPECT_EQ(r.comment_data(), "maint;window");

  ScheduleDowntimeRequest hr;
  ASSERT_TRUE(
      lc::schedule_downtime("host_1;1000;2000;0;7;60;admin;c", false, &hr)
          .ok());
  EXPECT_EQ(hr.type(), ScheduleDowntimeRequest::HOST);
  EXPECT_FALSE(hr.fixed());
  EXPECT_EQ(hr.triggered_by(), 7u);
  EXPECT_EQ(hr.host_case(), ScheduleDowntimeRequest::kHostName);

  DowntimeIdentifier id;
  ASSERT_TRUE(lc::downtime_identifier("12", &id).ok());
  EXPECT_EQ(id.downtime_id(), 12u);
}

TEST(LegacyCommands, NotifierSettings) {
  HostNotificationNumberRequest n;
  ASSERT_TRUE(lc::host_notification_number("host_1;3", &n).ok());
  EXPECT_EQ(n.number(), 3u);
  ServiceNotificationNumberRequest sn;
  ASSERT_TRUE(lc::service_notification_number("host_1;service_1;0", &sn).ok());
  EXPECT_EQ(sn.service().description(), "service_1");

  HostCustomNotificationRequest c;
  ASSERT_TRUE(lc::host_custom_notification("host_1;5;admin;hey;you", &c).ok());
  EXPECT_TRUE(c.broadcast());
  EXPECT_FALSE(c.forced());
  EXPECT_TRUE(c.increment());
  EXPECT_EQ(c.comment(), "hey;you");
  ServiceCustomNotificationRequest sc;
  ASSERT_TRUE(
      lc::service_custom_notification("host_1;service_1;2;admin;x", &sc).ok());
  EXPECT_TRUE(sc.forced());
  EXPECT_FALSE(sc.broadcast());

  HostNotificationPeriodRequest p;
  ASSERT_TRUE(lc::host_notification_period("host_1;24x7", &p).ok());
  EXPECT_EQ(p.timeperiod(), "24x7");
  ContactNotificationPeriodRequest cp;
  ASSERT_TRUE(lc::contact_notification_period("John_Doe;workhours", &cp).ok());
  EXPECT_EQ(cp.contact().name(), "John_Doe");
  EXPECT_EQ(cp.timeperiod(), "workhours");
  EXPECT_EQ(lc::contact_notification_period("John_Doe", &cp).error_code(),
            grpc::StatusCode::INVALID_ARGUMENT);
}

TEST(LegacyCommands, Identifiers) {
  HostIdentifier h;
  ASSERT_TRUE(lc::host_identifier("host_1", &h).ok());
  EXPECT_EQ(h.host_name(), "host_1");
  EXPECT_EQ(lc::host_identifier("", &h).error_code(),
            grpc::StatusCode::INVALID_ARGUMENT);
  ServiceIdentifier s;
  ASSERT_TRUE(lc::service_identifier("host_1;service_1", &s).ok());
  EXPECT_EQ(s.description(), "service_1");
  EXPECT_EQ(lc::service_identifier("host_1", &s).error_code(),
            grpc::StatusCode::INVALID_ARGUMENT);
  ContactgroupIdentifier g;
  ASSERT_TRUE(lc::contactgroup_identifier("admins", &g).ok());
  EXPECT_EQ(g.name(), "admins");
}
