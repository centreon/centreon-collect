/**
 * Copyright 2017 - 2024 Centreon (https://www.centreon.com/)
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

#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "common/engine_conf/parser.hh"
#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/state_helper.hh"

#include "common/log_v2/log_v2.hh"

#define CONFIG_PATH "./tests/config0/"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace rapidjson;
namespace fs = std::filesystem;

static void RmConf() {
  std::filesystem::remove_all("/tmp/etc/centreon-engine/config0");
}

static void CreateConf() {
  if (!fs::exists("/tmp/etc/centreon-engine/config0/")) {
    fs::create_directories("/tmp/etc/centreon-engine/config0/");
  }

  constexpr const char* cmd1 =
      "for i in " COMMON_CFG_TEST
      "/config0/*.cfg ; do cp $i /tmp/etc/centreon-engine/config0/ ; done";
  system(cmd1);
}

class Pb_Expand : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    com::centreon::common::log_v2::log_v2::load("expand-tests");
    CreateConf();
  }
  static void TearDownTestSuite() {
    RmConf();
    com::centreon::common::log_v2::log_v2::unload();
    std::cout << "Directories deleted: " << std::endl;
  }
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(Pb_Expand, host) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);

  absl::flat_hash_map<std::string, configuration::Hostgroup*> m_hostgroups;
  for (auto& hg : *pb_config.mutable_hostgroups()) {
    m_hostgroups.emplace(hg.hostgroup_name(), &hg);
  }
  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }
  if (!doc1.HasMember("hosts") || !doc1["hosts"].IsArray() ||
      doc1["hosts"].Empty()) {
    throw std::runtime_error("Missing or invalid 'hosts' field.");
  }
  if (!doc1["hosts"][0].HasMember("hostId")) {
    throw std::runtime_error("Missing 'hostId' field.");
  }
  if (!doc1["hosts"][0].HasMember("hostName")) {
    throw std::runtime_error("Missing 'hostName' field.");
  }
  if (!doc1["hosts"][0].HasMember("customvariables") ||
      !doc1["hosts"][0]["customvariables"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'customvariables' field.");
  }
  if (!doc1.HasMember("hostgroups") || !doc1["hostgroups"].IsArray() ||
      doc1["hostgroups"].Size() <= 1) {
    throw std::runtime_error("Missing or invalid 'hostgroups' field.");
  }
  if (!doc1["hostgroups"][1].HasMember("hostgroupId")) {
    throw std::runtime_error("Missing 'hostgroupId' field.");
  }
  if (!doc1["hostgroups"][1].HasMember("hostgroupName")) {
    throw std::runtime_error("Missing 'hostgroupName' field.");
  }
  if (!doc1["hostgroups"][1].HasMember("members") ||
      !doc1["hostgroups"][1]["members"].HasMember("data") ||
      !doc1["hostgroups"][1]["members"]["data"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'members' or 'data' field.");
  }

  ASSERT_EQ(std::string(doc1["hosts"][0]["hostId"].GetString()), "1");
  ASSERT_EQ(std::string(doc1["hosts"][0]["hostName"].GetString()), "host_1");

  bool found = false;
  for (const auto& item : doc1["hosts"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("KEY3") &&
        item["value"].GetString() == std::string("VAL3") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Custom variable KEY3 with value VAL3 and isSent true not found.";

  found = false;
  for (const auto& item : doc1["hosts"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("KEY2") &&
        item["value"].GetString() == std::string("VAL2") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Custom variable KEY2 with value VAL2 and isSent true not found.";

  found = false;
  for (const auto& item : doc1["hosts"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("KEY1") &&
        item["value"].GetString() == std::string("VAL1") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Custom variable KEY1 with value VAL1 and isSent true not found.";

  found = false;
  for (const auto& item : doc1["hosts"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("SNMPCOMMUNITY") &&
        item["value"].GetString() == std::string("public") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "Custom variable SNMPCOMMUNITY with value public and "
                        "isSent true not found.";

  found = false;
  for (const auto& item : doc1["hosts"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("SNMPVERSION") &&
        item["value"].GetString() == std::string("2c") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Custom variable SNMPVERSION with value 2c and isSent true not found.";

  ASSERT_EQ(doc1["hostgroups"][1]["hostgroupId"].GetInt(), 2);
  ASSERT_EQ(std::string(doc1["hostgroups"][1]["hostgroupName"].GetString()),
            "hostgroup_2");

  found = false;
  // host expand add host to corresponding hostgroup
  for (const auto& item : doc1["hostgroups"][1]["members"]["data"].GetArray()) {
    if (item.GetString() == std::string("host_4") ||
        item.GetString() == std::string("host_5") ||
        item.GetString() == std::string("host_1")) {
      found = true;
    } else {
      found = false;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Hostgroup members data does not match expected values.";
}

TEST_F(Pb_Expand, service) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);
  absl::flat_hash_map<std::string, configuration::Host> m_host;
  for (auto& h : pb_config.hosts()) {
    m_host.emplace(h.host_name(), h);
  }

  absl::flat_hash_map<std::string, Servicegroup*> m_servicegroups;
  for (auto& sg : *pb_config.mutable_servicegroups())
    m_servicegroups.emplace(sg.servicegroup_name(), &sg);

  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }
  if (!doc1.HasMember("services") || !doc1["services"].IsArray() ||
      doc1["services"].Empty()) {
    throw std::runtime_error("Missing or invalid 'services' field.");
  }
  if (!doc1["services"][0].HasMember("serviceId")) {
    throw std::runtime_error("Missing 'serviceId' field.");
  }
  if (!doc1["services"][0].HasMember("hostId")) {
    throw std::runtime_error("Missing 'hostId' field.");
  }
  if (!doc1["services"][0].HasMember("customvariables") ||
      !doc1["services"][0]["customvariables"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'customvariables' field.");
  }
  if (!doc1["services"][1].HasMember("contactgroups") ||
      !doc1["services"][1]["contactgroups"].HasMember("data") ||
      !doc1["services"][1]["contactgroups"]["data"].IsArray()) {
    throw std::runtime_error(
        "Missing or invalid 'contactgroups' or 'data' field.");
  }
  if (!doc1["services"][1].HasMember("contacts") ||
      !doc1["services"][1]["contacts"].HasMember("data") ||
      !doc1["services"][1]["contacts"]["data"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'contacts' or 'data' field.");
  }
  if (!doc1.HasMember("servicegroups") || !doc1["servicegroups"].IsArray() ||
      doc1["servicegroups"].Empty()) {
    throw std::runtime_error("Missing or invalid 'servicegroups' field.");
  }
  if (!doc1["servicegroups"][0].HasMember("members") ||
      !doc1["servicegroups"][0]["members"].HasMember("data") ||
      !doc1["servicegroups"][0]["members"]["data"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'members' or 'data' field.");
  }
  if (!doc1["services"][1].HasMember("timezone")) {
    throw std::runtime_error("Missing 'timezone' field.");
  }
  if (!doc1["services"][1].HasMember("notificationPeriod")) {
    throw std::runtime_error("Missing 'notificationPeriod' field.");
  }
  if (!doc1["services"][1].HasMember("notificationInterval")) {
    throw std::runtime_error("Missing 'notificationInterval' field.");
  }

  ASSERT_EQ(std::string(doc1["services"][0]["serviceId"].GetString()), "1");
  ASSERT_EQ(std::string(doc1["services"][0]["hostId"].GetString()), "1");

  bool found = false;
  for (const auto& item : doc1["services"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("SNMPCOMMUNITY") &&
        item["value"].GetString() == std::string("public") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "Custom variable SNMPCOMMUNITY with value public and "
                        "isSent true not found.";

  found = false;
  for (const auto& item : doc1["services"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("KEY_SERV1_1") &&
        item["value"].GetString() == std::string("VAL_SERV1") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "Custom variable KEY_SERV1_1 with value VAL_SERV1 and "
                        "isSent true not found.";

  ASSERT_EQ(std::string(doc1["services"][1]["serviceId"].GetString()), "2");
  ASSERT_EQ(std::string(doc1["services"][1]["hostId"].GetString()), "1");

  found = false;
  for (const auto& item :
       doc1["services"][1]["contactgroups"]["data"].GetArray()) {
    if (item.GetString() == std::string("contactgroup_2")) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "contactgroups members data does not match expected values.";

  found = false;

  for (const auto& item : doc1["services"][1]["contacts"]["data"].GetArray()) {
    if (item.GetString() == std::string("U1")) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "contactgroups members data does not match expected values.";

  ASSERT_TRUE(!doc1["services"][1]["contactgroups"]["additive"].GetBool());
  ASSERT_TRUE(!doc1["services"][1]["contacts"]["additive"].GetBool());
  ASSERT_EQ(doc1["services"][1]["notificationInterval"].GetInt(), 8);

  ASSERT_EQ(std::string(doc1["services"][1]["notificationPeriod"].GetString()),
            "none");
  ASSERT_EQ(std::string(doc1["services"][1]["timezone"].GetString()), "GMT+01");

  // service expand add service to corresponding SERVICEGROUP

  found = false;
  // host expand add host to corresponding hostgroup
  for (const auto& item :
       doc1["servicegroups"][0]["members"]["data"].GetArray()) {
    if (item["first"].GetString() == std::string("host_1") &&
        item["second"].GetString() == std::string("service_1")) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "servicegroups members data does not match expected values.";
}

TEST_F(Pb_Expand, contact) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);

  absl::flat_hash_map<std::string, configuration::Contactgroup*>
      m_contactgroups;
  for (auto& cg : *pb_config.mutable_contactgroups()) {
    m_contactgroups[cg.contactgroup_name()] = &cg;
  }

  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }

  if (!doc1.HasMember("contacts") || !doc1["contacts"].IsArray() ||
      doc1["contacts"].Empty()) {
    throw std::runtime_error("Missing or invalid 'contacts' field.");
  }
  if (!doc1["contacts"][0].HasMember("contactName")) {
    throw std::runtime_error("Missing 'contactName' field.");
  }
  if (!doc1["contacts"][0].HasMember("customvariables") ||
      !doc1["contacts"][0]["customvariables"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'customvariables' field.");
  }
  if (!doc1.HasMember("contactgroups") || !doc1["contactgroups"].IsArray() ||
      doc1["contactgroups"].Empty()) {
    throw std::runtime_error("Missing or invalid 'contactgroups' field.");
  }
  if (!doc1["contactgroups"][0].HasMember("members") ||
      !doc1["contactgroups"][0]["members"].HasMember("data") ||
      !doc1["contactgroups"][0]["members"]["data"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'members' or 'data' field.");
  }

  ASSERT_EQ(std::string(doc1["contacts"][0]["contactName"].GetString()),
            "John_Doe");

  bool found = false;
  for (const auto& item : doc1["contacts"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("SNMPCOMMUNITY") &&
        item["value"].GetString() == std::string("public") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "Custom variable SNMPCOMMUNITY with value public and "
                        "isSent true not found.";
  found = false;
  for (const auto& item :
       doc1["contactgroups"][0]["members"]["data"].GetArray()) {
    if (item.GetString() == std::string("John_Doe") ||
        item.GetString() == std::string("U2") ||
        item.GetString() == std::string("U3") ||
        item.GetString() == std::string("U4")) {
      found = true;
    } else {
      found = false;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "contactgroups members data does not match expected values.";
}

TEST_F(Pb_Expand, contactgroup) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);
  absl::flat_hash_map<std::string, configuration::Contactgroup*>
      m_contactgroups;
  for (auto& cg : *pb_config.mutable_contactgroups()) {
    m_contactgroups[cg.contactgroup_name()] = &cg;
  }
  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }

  if (!doc1["contactgroups"][0].HasMember("contactgroupName")) {
    throw std::runtime_error("Missing 'contactgroupName' field.");
  }
  if (!doc1["contactgroups"][0].HasMember("contactgroupMembers") ||
      !doc1["contactgroups"][0]["contactgroupMembers"].HasMember("data") ||
      !doc1["contactgroups"][0]["contactgroupMembers"]["data"].IsArray()) {
    throw std::runtime_error(
        "Missing or invalid 'contactgroupMembers' or 'data' field.");
  }
  if (!doc1["contactgroups"][0].HasMember("members") ||
      !doc1["contactgroups"][0]["members"].HasMember("data") ||
      !doc1["contactgroups"][0]["members"]["data"].IsArray()) {
    throw std::runtime_error("Missing or invalid 'members' or 'data' field.");
  }

  ASSERT_EQ(
      std::string(doc1["contactgroups"][0]["contactgroupName"].GetString()),
      "contactgroup_1");

  ASSERT_TRUE(doc1["contactgroups"][0]["contactgroupMembers"]["data"].Empty());

  bool found = false;
  for (const auto& item :
       doc1["contactgroups"][0]["members"]["data"].GetArray()) {
    if (item.GetString() == std::string("John_Doe") ||
        item.GetString() == std::string("U2") ||
        item.GetString() == std::string("U3") ||
        item.GetString() == std::string("U4")) {
      found = true;
    } else {
      found = false;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "contactgroups members data does not match expected values.";
}

TEST_F(Pb_Expand, serviceescalation) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);

  absl::flat_hash_map<std::string, configuration::Hostgroup*> m_hostgroups;
  for (auto& hg : *pb_config.mutable_hostgroups()) {
    m_hostgroups.emplace(hg.hostgroup_name(), &hg);
  }

  absl::flat_hash_map<std::string, configuration::Servicegroup*>
      m_servicegroups;
  for (auto& sg : *pb_config.mutable_servicegroups())
    m_servicegroups.emplace(sg.servicegroup_name(), &sg);

  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }

  if (!doc1.HasMember("serviceescalations") ||
      !doc1["serviceescalations"].IsArray() ||
      doc1["serviceescalations"].Empty()) {
    throw std::runtime_error("Missing or invalid 'serviceescalations' field.");
  }
  if (!doc1["serviceescalations"][0].HasMember("hosts") ||
      !doc1["serviceescalations"][0]["hosts"].HasMember("data") ||
      !doc1["serviceescalations"][0]["hosts"]["data"].IsArray()) {
    throw std::runtime_error("Missing 'hosts' field in serviceescalation.");
  }
  if (!doc1["serviceescalations"][0].HasMember("serviceDescription") ||
      !doc1["serviceescalations"][0]["serviceDescription"].HasMember("data") ||
      !doc1["serviceescalations"][0]["serviceDescription"]["data"].IsArray()) {
    throw std::runtime_error(
        "Missing 'serviceDescription' field in serviceescalation.");
  }

  bool found = false;
  for (const auto& item : doc1["serviceescalations"].GetArray()) {
    if (item["hosts"]["data"][0].GetString() == std::string("host_3") &&
        item["serviceDescription"]["data"][0].GetString() ==
            std::string("service_11")) {
      ASSERT_TRUE(item["hostgroups"]["data"].Empty()) << "Hostgroups not empty";
      ASSERT_TRUE(item["servicegroups"]["data"].Empty())
          << "Servicegroups not empty";
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Service escalation with host_3 and service_11 not found.";

  found = false;
  for (const auto& item : doc1["serviceescalations"].GetArray()) {
    if (item["hosts"]["data"][0].GetString() == std::string("host_3") &&
        item["serviceDescription"]["data"][0].GetString() ==
            std::string("service_12")) {
      ASSERT_TRUE(item["hostgroups"]["data"].Empty()) << "Hostgroups not empty";
      ASSERT_TRUE(item["servicegroups"]["data"].Empty())
          << "Servicegroups not empty";
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Service escalation with host_3 and service_12 not found.";
}

TEST_F(Pb_Expand, hostescalation) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);

  absl::flat_hash_map<std::string, configuration::Hostgroup*> m_hostgroups;
  for (auto& hg : *pb_config.mutable_hostgroups()) {
    m_hostgroups.emplace(hg.hostgroup_name(), &hg);
  }
  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }

  if (!doc1.HasMember("hostescalations") ||
      !doc1["hostescalations"].IsArray() || doc1["hostescalations"].Empty()) {
    throw std::runtime_error("Missing or invalid 'hostescalations' field.");
  }
  if (!doc1["hostescalations"][0].HasMember("hosts") ||
      !doc1["hostescalations"][0]["hosts"].HasMember("data") ||
      !doc1["hostescalations"][0]["hosts"]["data"].IsArray()) {
    throw std::runtime_error("Missing 'hosts' field in hostescalations.");
  }

  bool found = false;
  for (const auto& item : doc1["hostescalations"].GetArray()) {
    if (item["hosts"]["data"][0].GetString() == std::string("host_3")) {
      ASSERT_TRUE(item["hostgroups"]["data"].Empty()) << "Hostgroups not empty";
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "Host escalation with host_3 not found.";

  found = false;
  for (const auto& item : doc1["hostescalations"].GetArray()) {
    if (item["hosts"]["data"][0].GetString() == std::string("host_2")) {
      ASSERT_TRUE(item["hostgroups"]["data"].Empty()) << "Hostgroups not empty";
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "Host escalation with host_2 not found.";
}

TEST_F(Pb_Expand, anomalydetection) {
  configuration::State pb_config;
  configuration::state_helper state_hlp(&pb_config);
  configuration::error_cnt err;
  configuration::parser p;

  p.parse("/tmp/etc/centreon-engine/config0/centengine.cfg", &pb_config, err);
  state_hlp.expand(err);

  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  std::string json_output;
  auto status = google::protobuf::util::MessageToJsonString(
      pb_config, &json_output, options);

  rapidjson::Document doc1;

  if (doc1.Parse(json_output.c_str()).HasParseError()) {
    throw std::runtime_error("Error parsing JSON.");
  }

  if (!doc1.HasMember("anomalydetections") ||
      !doc1["anomalydetections"].IsArray() ||
      doc1["anomalydetections"].Empty()) {
    throw std::runtime_error("Missing or invalid 'anomalydetections' field.");
  }
  if (!doc1["anomalydetections"][0].HasMember("customvariables") ||
      !doc1["anomalydetections"][0]["customvariables"].IsArray()) {
    throw std::runtime_error(
        "Missing or invalid 'customvariables' field in anomalydetections.");
  }

  bool found = false;
  for (const auto& item :
       doc1["anomalydetections"][0]["customvariables"].GetArray()) {
    if (item["name"].GetString() == std::string("KEY1") &&
        item["value"].GetString() == std::string("_VAL01") &&
        item["isSent"].GetBool() == true) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found)
      << "Custom variable KEY1 with value _VAL01 and isSent true not found.";
}
