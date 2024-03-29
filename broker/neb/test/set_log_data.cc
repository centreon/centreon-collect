/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/neb/set_log_data.hh"
#include <gtest/gtest.h>
#include "com/centreon/broker/neb/log_entry.hh"

using namespace com::centreon::broker;

// Engine stubs
uint64_t com::centreon::engine::get_host_id(__attribute__((__unused__))
                                            std::string const& name) {
  return 1;
}

uint64_t com::centreon::engine::get_service_id(__attribute__((__unused__))
                                               std::string const& host,
                                               __attribute__((__unused__))
                                               std::string const& svc) {
  return 1;
}

TEST(SetLogData, Default) {
  // Log entry.
  neb::log_entry le;

  // Parse a service alert line.
  neb::set_log_data(le,
                    "EXTERNAL COMMAND: "
                    "SCHEDULE_FORCED_SVC_CHECK;MyHost;MyService;1428930446");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "");
  ASSERT_FALSE(le.log_type != 0);  // Default.
  ASSERT_FALSE(le.msg_type != 5);  // Default.
  ASSERT_FALSE(le.output !=
               "EXTERNAL COMMAND: "
               "SCHEDULE_FORCED_SVC_CHECK;MyHost;MyService;1428930446");
  ASSERT_FALSE(le.retry != 0);
  ASSERT_FALSE(le.service_description != "");
  ASSERT_FALSE(le.status != 0);
}

TEST(SetLogData, DefaultPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a service alert line.
  ASSERT_TRUE(neb::set_pb_log_data(
      le,
      "EXTERNAL COMMAND: "
      "SCHEDULE_FORCED_SVC_CHECK;MyHost;MyService;1428930446"));

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "");
  ASSERT_FALSE(le.obj().type() != 0);      // Default.
  ASSERT_FALSE(le.obj().msg_type() != 5);  // Default.
  ASSERT_FALSE(le.obj().output() !=
               "EXTERNAL COMMAND: "
               "SCHEDULE_FORCED_SVC_CHECK;MyHost;MyService;1428930446");
  ASSERT_FALSE(le.obj().retry() != 0);
  ASSERT_FALSE(le.obj().service_description() != "");
  ASSERT_FALSE(le.obj().status() != 0);
}

/**
 *  Check that a host alert log is properly parsed.
 *
 *  @return 0 on success.
 */
TEST(SetLogData, HostAlert) {
  // Log entry.
  neb::log_entry le;

  // Parse a host alert line.
  neb::set_log_data(
      le, "HOST ALERT: myserver;UNREACHABLE;HARD;4;Time to live exceeded");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 1);  // HARD
  ASSERT_FALSE(le.msg_type != 1);  // HOST ALERT
  ASSERT_FALSE(le.output != "Time to live exceeded");
  ASSERT_FALSE(le.retry != 4);
  ASSERT_FALSE(le.status != 2);  // UNREACHABLE
}

/**
 *  Check that a host alert log is properly parsed.
 *
 *  @return 0 on success.
 */
TEST(SetLogData, HostAlertPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a host alert line.
  neb::set_pb_log_data(
      le, "HOST ALERT: myserver;UNREACHABLE;HARD;4;Time to live exceeded");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 1);      // HARD
  ASSERT_FALSE(le.obj().msg_type() != 1);  // HOST ALERT
  ASSERT_FALSE(le.obj().output() != "Time to live exceeded");
  ASSERT_FALSE(le.obj().retry() != 4);
  ASSERT_FALSE(le.obj().status() != 2);  // UNREACHABLE
}

/**
 *  Check that a host alert log is properly parsed.
 *
 *  @return 0 on success.
 */
TEST(SetLogData, HostAlertPbError) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a host alert line.
  ASSERT_FALSE(neb::set_pb_log_data(le, "HOST ALERT: myserver;UNREACHABLE;"));
}

/**
 *  Check that a initial host state log is properly parsed.
 */
TEST(SetLogData, InititalHostState) {
  try {
    // #1
    {
      neb::log_entry le;
      neb::set_log_data(le, "INITIAL HOST STATE: myserver;UP;HARD;1;PING OK");
      ASSERT_FALSE(le.host_name != "myserver");
      ASSERT_FALSE(le.log_type != 1);  // HARD
      ASSERT_FALSE(le.msg_type != 9);  // INITIAL HOST STATE
      ASSERT_FALSE(le.output != "PING OK");
      ASSERT_FALSE(le.retry != 1);
      ASSERT_FALSE(le.status != 0);  // UP
    }

    // #2
    {
      neb::log_entry le;
      neb::set_log_data(le, "INITIAL HOST STATE: SERVER007;UNKNOWN;SOFT;2;");
      ASSERT_FALSE(le.host_name != "SERVER007");
      ASSERT_FALSE(le.log_type != 0);  // SOFT
      ASSERT_FALSE(le.msg_type != 9);  // INITIAL HOST STATE
      ASSERT_FALSE(le.output != "");
      ASSERT_FALSE(le.retry != 2);
      ASSERT_FALSE(le.status != 3);  // UNKNOWN
    }
  } catch (...) {
    ASSERT_TRUE(false);
  }
}

/**
 *  Check that a initial host state log is properly parsed.
 */
TEST(SetLogData, InititalHostStatePb) {
  // #1
  {
    neb::pb_log_entry le;
    neb::set_pb_log_data(le, "INITIAL HOST STATE: myserver;UP;HARD;1;PING OK");
    ASSERT_FALSE(le.obj().host_name() != "myserver");
    ASSERT_FALSE(le.obj().type() != 1);      // HARD
    ASSERT_FALSE(le.obj().msg_type() != 9);  // INITIAL HOST STATE
    ASSERT_FALSE(le.obj().output() != "PING OK");
    ASSERT_FALSE(le.obj().retry() != 1);
    ASSERT_FALSE(le.obj().status() != 0);  // UP
  }

  // #2
  {
    neb::pb_log_entry le;
    neb::set_pb_log_data(le, "INITIAL HOST STATE: SERVER007;UNKNOWN;SOFT;2;");
    ASSERT_FALSE(le.obj().host_name() != "SERVER007");
    ASSERT_FALSE(le.obj().type() != 0);      // SOFT
    ASSERT_FALSE(le.obj().msg_type() != 9);  // INITIAL HOST STATE
    ASSERT_FALSE(le.obj().output() != "");
    ASSERT_FALSE(le.obj().retry() != 2);
    ASSERT_FALSE(le.obj().status() != 3);  // UNKNOWN
  }
}

/**
 *  Check that an initial service state log is properly parsed.
 */
TEST(SetLogData, InititalServiceState) {
  // Log entry.
  neb::log_entry le;

  // Parse an initial service state line.
  neb::set_log_data(
      le,
      "INITIAL SERVICE STATE: myserver;myservice;UNKNOWN;SOFT;1;ERROR when "
      "getting SNMP version");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 0);  // SOFT
  ASSERT_FALSE(le.msg_type != 8);  // INITIAL SERVICE STATE
  ASSERT_FALSE(le.output != "ERROR when getting SNMP version");
  ASSERT_FALSE(le.retry != 1);
  ASSERT_FALSE(le.service_description != "myservice");
  ASSERT_FALSE(le.status != 3);  // UNKNOWN
}

/**
 *  Check that an initial service state log is properly parsed.
 */
TEST(SetLogData, InititalServiceStatePb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse an initial service state line.
  neb::set_pb_log_data(
      le,
      "INITIAL SERVICE STATE: myserver;myservice;UNKNOWN;SOFT;1;ERROR when "
      "getting SNMP version");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 0);      // SOFT
  ASSERT_FALSE(le.obj().msg_type() != 8);  // INITIAL SERVICE STATE
  ASSERT_FALSE(le.obj().output() != "ERROR when getting SNMP version");
  ASSERT_FALSE(le.obj().retry() != 1);
  ASSERT_FALSE(le.obj().service_description() != "myservice");
  ASSERT_FALSE(le.obj().status() != 3);  // UNKNOWN
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, ServiceALert) {
  // Log entry.
  neb::log_entry le;

  // Parse a service alert line.
  neb::set_log_data(le,
                    "SERVICE ALERT: myserver;myservice;WARNING;SOFT;3;CPU 84%");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 0);
  ASSERT_FALSE(le.msg_type != 0);
  ASSERT_FALSE(le.output != "CPU 84%");
  ASSERT_FALSE(le.retry != 3);
  ASSERT_FALSE(le.service_description != "myservice");
  ASSERT_FALSE(le.status != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, ServiceALertPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a service alert line.
  neb::set_pb_log_data(
      le, "SERVICE ALERT: myserver;myservice;WARNING;SOFT;3;CPU 84%");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 0);
  ASSERT_FALSE(le.obj().msg_type() != 0);
  ASSERT_FALSE(le.obj().output() != "CPU 84%");
  ASSERT_FALSE(le.obj().retry() != 3);
  ASSERT_FALSE(le.obj().service_description() != "myservice");
  ASSERT_FALSE(le.obj().status() != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, HostEventHandler) {
  // Log entry.
  neb::log_entry le;

  // Parse a service alert line.
  neb::set_log_data(le, "HOST EVENT HANDLER: myserver;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 0);
  ASSERT_FALSE(le.msg_type != LogEntry_MsgType_HOST_EVENT_HANDLER);
  ASSERT_FALSE(le.output != "coucou");
  ASSERT_FALSE(le.retry != 3);
  ASSERT_FALSE(le.status != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, HostEventHandlerPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a service alert line.
  neb::set_pb_log_data(le,
                       "HOST EVENT HANDLER: myserver;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 0);
  ASSERT_FALSE(le.obj().msg_type() != LogEntry_MsgType_HOST_EVENT_HANDLER);
  ASSERT_FALSE(le.obj().output() != "coucou");
  ASSERT_FALSE(le.obj().retry() != 3);
  ASSERT_FALSE(le.obj().status() != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, ServiceEventHandler) {
  // Log entry.
  neb::log_entry le;

  // Parse a service alert line.
  neb::set_log_data(
      le, "SERVICE EVENT HANDLER: myserver;myservice;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 0);
  ASSERT_FALSE(le.msg_type != LogEntry_MsgType_SERVICE_EVENT_HANDLER);
  ASSERT_FALSE(le.output != "coucou");
  ASSERT_FALSE(le.retry != 3);
  ASSERT_FALSE(le.service_description != "myservice");
  ASSERT_FALSE(le.status != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, ServiceEventHandlerPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a service alert line.
  neb::set_pb_log_data(
      le, "SERVICE EVENT HANDLER: myserver;myservice;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 0);
  ASSERT_FALSE(le.obj().msg_type() != LogEntry_MsgType_SERVICE_EVENT_HANDLER);
  ASSERT_FALSE(le.obj().output() != "coucou");
  ASSERT_FALSE(le.obj().retry() != 3);
  ASSERT_FALSE(le.obj().service_description() != "myservice");
  ASSERT_FALSE(le.obj().status() != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, GlobalHostEventHandler) {
  // Log entry.
  neb::log_entry le;

  // Parse a service alert line.
  neb::set_log_data(
      le, "GLOBAL HOST EVENT HANDLER: myserver;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 0);
  ASSERT_FALSE(le.msg_type != LogEntry_MsgType_GLOBAL_HOST_EVENT_HANDLER);
  ASSERT_FALSE(le.output != "coucou");
  ASSERT_FALSE(le.retry != 3);
  ASSERT_FALSE(le.status != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, GlobalHostEventHandlerPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a service alert line.
  neb::set_pb_log_data(
      le, "GLOBAL HOST EVENT HANDLER: myserver;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 0);
  ASSERT_FALSE(le.obj().msg_type() !=
               LogEntry_MsgType_GLOBAL_HOST_EVENT_HANDLER);
  ASSERT_FALSE(le.obj().output() != "coucou");
  ASSERT_FALSE(le.obj().retry() != 3);
  ASSERT_FALSE(le.obj().status() != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, GlobalServiceEventHandler) {
  // Log entry.
  neb::log_entry le;

  // Parse a service alert line.
  neb::set_log_data(
      le,
      "GLOBAL SERVICE EVENT HANDLER: myserver;myservice;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.host_name != "myserver");
  ASSERT_FALSE(le.log_type != 0);
  ASSERT_FALSE(le.msg_type != LogEntry_MsgType_GLOBAL_SERVICE_EVENT_HANDLER);
  ASSERT_FALSE(le.output != "coucou");
  ASSERT_FALSE(le.retry != 3);
  ASSERT_FALSE(le.service_description != "myservice");
  ASSERT_FALSE(le.status != 1);
}

/**
 *  Check that a service alert log is properly parsed.
 */
TEST(SetLogData, GlobalServiceEventHandlerPb) {
  // Log entry.
  neb::pb_log_entry le;

  // Parse a service alert line.
  neb::set_pb_log_data(
      le,
      "GLOBAL SERVICE EVENT HANDLER: myserver;myservice;WARNING;SOFT;3;coucou");

  // Check that it was properly parsed.
  ASSERT_FALSE(le.obj().host_name() != "myserver");
  ASSERT_FALSE(le.obj().type() != 0);
  ASSERT_FALSE(le.obj().msg_type() !=
               LogEntry_MsgType_GLOBAL_SERVICE_EVENT_HANDLER);
  ASSERT_FALSE(le.obj().output() != "coucou");
  ASSERT_FALSE(le.obj().retry() != 3);
  ASSERT_FALSE(le.obj().service_description() != "myservice");
  ASSERT_FALSE(le.obj().status() != 1);
}
