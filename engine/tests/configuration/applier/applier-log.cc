/**
 * Copyright 2022-2023 Centreon (https://www.centreon.com/)
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

#include <com/centreon/engine/configuration/applier/hostescalation.hh>
#include <com/centreon/engine/configuration/parser.hh>
#include <fstream>

#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class ApplierLog : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(ApplierLog, logV2Enabled) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_v2_enabled(), true);

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_v2_enabled=0" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);
  std::remove("/tmp/test-config.cfg");

  ASSERT_EQ(st.log_v2_enabled(), false);
}

TEST_F(ApplierLog, logLegacyEnabled) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_legacy_enabled(), true);

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_legacy_enabled=0" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);
  std::remove("/tmp/test-config.cfg");

  ASSERT_EQ(st.log_legacy_enabled(), false);
}

TEST_F(ApplierLog, logV2Logger) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_v2_logger(), "file");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_v2_logger=syslog" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);
  std::remove("/tmp/test-config.cfg");

  ASSERT_EQ(st.log_v2_logger(), "syslog");
}

TEST_F(ApplierLog, logLevelFunctions) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_functions(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_functions=trace" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_functions(), "trace");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_functions=tracerrrr" << std::endl;
  ofs.close();

  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
}

TEST_F(ApplierLog, logLevelConfig) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_config(), "info");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_config=debug" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_config(), "debug");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_config=tracerrrr" << std::endl;
  ofs.close();

  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");

  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_config")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_config(), "debug");
}

TEST_F(ApplierLog, logLevelEvents) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_events(), "info");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_events=warning" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_events(), "warning");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_events=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_events")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_events(), "warning");
}

TEST_F(ApplierLog, logLevelChecks) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_checks(), "info");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_checks=error" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_checks(), "error");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_checks=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_checks")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_checks(), "error");
}

TEST_F(ApplierLog, logLevelNotifications) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_notifications(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_notifications=off" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_notifications(), "off");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_notifications=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_notifications")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_notifications(), "off");
}

TEST_F(ApplierLog, logLevelEventBroker) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_eventbroker(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_eventbroker=critical" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_eventbroker(), "critical");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_eventbroker=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_eventbroker")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_eventbroker(), "critical");
}

TEST_F(ApplierLog, logLevelExternalCommand) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_external_command(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_external_command=trace" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_external_command(), "trace");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_external_command=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_external_command")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_external_command(), "trace");
}

TEST_F(ApplierLog, logLevelCommands) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_commands(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_commands=debug" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_commands(), "debug");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_commands=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_commands")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_commands(), "debug");
}

TEST_F(ApplierLog, logLevelDowntimes) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_downtimes(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_downtimes=warning" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_downtimes(), "warning");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_downtimes=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_downtimes")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_downtimes(), "warning");
}

TEST_F(ApplierLog, logLevelComments) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_comments(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_comments=error" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_comments(), "error");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_comments=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_comments")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_comments(), "error");
}

TEST_F(ApplierLog, logLevelMacros) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_macros(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_macros=critical" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_macros(), "critical");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_macros=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_macros")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_macros(), "critical");
}

TEST_F(ApplierLog, logLevelProcess) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_process(), "info");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_process=off" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_process(), "off");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_process=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_process")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_process(), "off");
}

TEST_F(ApplierLog, logLevelRuntime) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_level_runtime(), "error");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_level_runtime=off" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);

  ASSERT_EQ(st.log_level_runtime(), "off");

  ofs.open("/tmp/test-config.cfg");
  ofs << "log_level_runtime=tracerrrr" << std::endl;
  ofs.close();
  ASSERT_THROW(parser.parse("/tmp/test-config.cfg", st), std::exception);
  std::remove("/tmp/test-config.cfg");
  // testing::internal::CaptureStdout();
  // parser.parse("/tmp/test-config.cfg", st);
  // std::remove("/tmp/test-config.cfg");

  // std::string out{testing::internal::GetCapturedStdout()};
  // std::cout << out << std::endl;
  // size_t step1{
  //     out.find("[config] [error] error wrong level setted for "
  //              "log_level_runtime")};
  // ASSERT_NE(step1, std::string::npos);
  // ASSERT_EQ(st.log_level_runtime(), "off");
}

TEST_F(ApplierLog, logFile) {
  configuration::parser parser;
  configuration::state st;

  ASSERT_EQ(st.log_file(), DEFAULT_LOG_FILE);

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=/tmp/centengine.log" << std::endl;
  ofs.close();

  parser.parse("/tmp/test-config.cfg", st);
  std::remove("/tmp/test-config.cfg");

  ASSERT_EQ(st.log_file(), "/tmp/centengine.log");
}
