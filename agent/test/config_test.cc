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

#include "check_exec.hh"
#include "config.hh"
#include "scheduler.hh"

using namespace com::centreon::agent;

extern std::shared_ptr<asio::io_context> g_io_context;

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

TEST(config, encryption_default_no) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"(
{   
    "host":"127.0.0.1",
    "endpoint":"host1.domain2:4317",
    "port":2500,
    "compression": true,
    "ca_common_name":"toto",
    "token":"token1"
})";
  f.close();

  config conf(_json_config_path);  // Declare and initialize conf
  ASSERT_EQ(conf.get_token(), "token1");
  ASSERT_EQ(conf.get_ca_name(), "toto");
  ASSERT_FALSE(conf.use_encryption());
  ASSERT_TRUE(conf.get_security_mode() ==
              com::centreon::common::grpc::grpc_config::NONE);
}

TEST(config, token) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"(
{   
    "host":"127.0.0.1",
    "endpoint":"host1.domain2:4317",
    "port":2500,
    "encryption":"full",
    "compression": true,
    "ca_common_name":"toto",
    "token":"token1"
})";
  f.close();

  config conf(_json_config_path);  // Declare and initialize conf
  ASSERT_EQ(conf.get_token(), "token1");
  ASSERT_EQ(conf.get_ca_name(), "toto");
  ASSERT_TRUE(conf.use_encryption());
  ASSERT_TRUE(conf.get_security_mode() ==
              com::centreon::common::grpc::grpc_config::TLS_SECURE);
}

TEST(config, reversed_grpc_streaming_token) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"(
{   
    "host":"127.0.0.1",
    "endpoint":"host1.domain2:4317",
    "port":2500,
    "encryption":"insecure",
    "compression": true,
    "reversed_grpc_streaming":true,
    "ca_common_name":"toto",
    "token":"token1"
})";
  f.close();

  config conf(_json_config_path);  // Declare and initialize conf
  ASSERT_TRUE(conf.get_trusted_tokens()->contains("token1"));
  ASSERT_TRUE(conf.use_encryption());
  ASSERT_TRUE(conf.get_security_mode() ==
              com::centreon::common::grpc::grpc_config::TLS_INSECURE);
}
TEST(config, old_fields) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"(
{   
    "host":"127.0.0.1",
    "endpoint":"host1.domain2:4317",
    "port":2500,
    "encryption":"full",
    "compression": true,
    "reversed_grpc_streaming":true,
    "ca_name":"toto",
    "ca_certificate":"/path/to/ca.crt",
    "token":"token1"
})";
  f.close();

  config conf(_json_config_path);  // Declare and initialize conf

  ASSERT_EQ(conf.get_ca_certificate_file(), "/path/to/ca.crt");
  ASSERT_EQ(conf.get_ca_name(), "toto");
}

TEST(config, new_fields) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"(
{   
    "host":"127.0.0.1",
    "endpoint":"host1.domain2:4317",
    "port":2500,
    "encryption":"full",
    "compression": true,
    "reversed_grpc_streaming":true,
    "ca_common_name":"toto",
    "ca":"/path/to/ca.crt",
    "token":"token1"
})";
  f.close();

  config conf(_json_config_path);  // Declare and initialize conf

  ASSERT_EQ(conf.get_ca_certificate_file(), "/path/to/ca.crt");
  ASSERT_EQ(conf.get_ca_name(), "toto");
}

TEST(config, custom_checks) {
  ::remove(_json_config_path.c_str());
  std::ofstream f(_json_config_path);
  f << R"(
{   
    "host":"127.0.0.1",
    "endpoint":"host1.domain2:4317",
    "port":2500,
    "encryption":"no",
    "token":"token1",
    "custom_check_file": "./tests/custom_check.ini"
})";
  f.close();

  config conf(_json_config_path);  // Declare and initialize conf

  ASSERT_EQ(conf.get_path_to_custom_checks(), "./tests/custom_check.ini");
  conf.read_custom_checks();

  const auto& custom_checks = conf.get_custom_checks();
  ASSERT_EQ(custom_checks.size(), 2);
  ASSERT_EQ(custom_checks.at("check_echo"), "/usr/bin/echo \"$ARG2$ $ARG1$\"");
  ASSERT_EQ(custom_checks.at("custom_check_2"),
            "/path/to/custom_check_2 -c /arg=<value>");
}

static const std::string _custom_checks_ini_path =
    std::filesystem::temp_directory_path() / "config_test_custom_check.ini";

// write the json also the custom file .ini
static void write_custom_checks_config(const std::string& ini_content) {
  ::remove(_json_config_path.c_str());
  std::ofstream json_f(_json_config_path);
  json_f << R"({"host":"127.0.0.1","endpoint":"host1.domain2:4317",)"
         << R"("custom_check_file":")" << _custom_checks_ini_path << R"("})";
  json_f.close();
  std::ofstream ini_f(_custom_checks_ini_path);
  ini_f << ini_content;
}

TEST(config, custom_checks_reload) {
  write_custom_checks_config(
      "[custom_checks]\n; a comment\ncheck_echo=/usr/bin/echo one\n");

  config conf(_json_config_path);
  ASSERT_TRUE(conf.read_custom_checks());
  ASSERT_EQ(conf.get_custom_checks().size(), 1);
  ASSERT_EQ(conf.get_custom_checks().at("check_echo"), "/usr/bin/echo one");

  {
    std::ofstream ini_f(_custom_checks_ini_path);
    ini_f << "check_echo=/usr/bin/echo two\ncheck_new=/usr/bin/true\n";
  }
  ASSERT_TRUE(conf.read_custom_checks());
  ASSERT_EQ(conf.get_custom_checks().size(), 2);
  ASSERT_EQ(conf.get_custom_checks().at("check_echo"), "/usr/bin/echo two");
  ASSERT_EQ(conf.get_custom_checks().at("check_new"), "/usr/bin/true");

  // file removed => previous custom checks are kept
  ::remove(_custom_checks_ini_path.c_str());
  ASSERT_FALSE(conf.read_custom_checks());
  ASSERT_EQ(conf.get_custom_checks().size(), 2);
  ASSERT_EQ(conf.get_custom_checks().at("check_echo"), "/usr/bin/echo two");

  // malformed line => whole file rejected, previous custom checks are kept
  {
    std::ofstream ini_f(_custom_checks_ini_path);
    ini_f << "check_echo=/usr/bin/echo three\nthis is not an assignment\n";
  }
  ASSERT_FALSE(conf.read_custom_checks());
  ASSERT_EQ(conf.get_custom_checks().at("check_echo"), "/usr/bin/echo two");

  // empty name or command => rejected too
  {
    std::ofstream ini_f(_custom_checks_ini_path);
    ini_f << "check_echo=\n";
  }
  ASSERT_FALSE(conf.read_custom_checks());
  ASSERT_EQ(conf.get_custom_checks().at("check_echo"), "/usr/bin/echo two");

  // corrected file => refreshed
  {
    std::ofstream ini_f(_custom_checks_ini_path);
    ini_f << "check_echo=/usr/bin/echo four\n";
  }
  ASSERT_TRUE(conf.read_custom_checks());
  ASSERT_EQ(conf.get_custom_checks().size(), 1);
  ASSERT_EQ(conf.get_custom_checks().at("check_echo"), "/usr/bin/echo four");
}

/**
 * @brief a check built after config::reload_custom_checks() must use the new
 * command line whereas checks built before keep the old one
 */
TEST(config, custom_check_command_refreshed_after_reload) {
  write_custom_checks_config("check_echo=/usr/bin/echo one\n");
  config::load(_json_config_path);

  Service serv;
  serv.set_service_description("serv");
  serv.set_command_name("command");
  serv.set_command_line(R"({"check":"custom","args":{"name":"check_echo"}})");

  // the resolved custom command is stored in check_exec process args
  auto build_check = [&serv]() {
    return std::dynamic_pointer_cast<check_exec>(
        scheduler::default_check_builder(
            g_io_context, spdlog::default_logger(), {}, serv,
            engine_to_agent_request_ptr(),
            []([[maybe_unused]] const std::shared_ptr<check>& caller,
               [[maybe_unused]] int status,
               [[maybe_unused]] const std::list<
                   com::centreon::common::perfdata>& perfdata,
               [[maybe_unused]] const std::list<std::string>& outputs) {},
            std::make_shared<checks_statistics>(), nullptr));
  };

  const std::vector<std::string> arg_one{"one"};
  const std::vector<std::string> arg_two{"two"};

  std::shared_ptr<check_exec> before_reload = build_check();
  ASSERT_TRUE(before_reload);
  ASSERT_EQ(before_reload->get_process_args()->get_exe_path(), "/usr/bin/echo");
  ASSERT_EQ(before_reload->get_process_args()->get_args(), arg_one);

  {
    std::ofstream ini_f(_custom_checks_ini_path);
    ini_f << "check_echo=/usr/bin/echo two\n";
  }
  config::reload_custom_checks();

  // already built checks keep their resolved command line
  ASSERT_EQ(before_reload->get_process_args()->get_args(), arg_one);
  // rebuilt checks use the refreshed one
  std::shared_ptr<check_exec> after_reload = build_check();
  ASSERT_TRUE(after_reload);
  ASSERT_EQ(after_reload->get_process_args()->get_exe_path(), "/usr/bin/echo");
  ASSERT_EQ(after_reload->get_process_args()->get_args(), arg_two);

  ::remove(_custom_checks_ini_path.c_str());
}