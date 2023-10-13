/**
 * Copyright 2011 - 2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/broker/config/parser.hh"
#include <gtest/gtest.h>
#include "broker/core/misc/misc.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace nlohmann;

/**
 *  Check that 'input' and 'output' are properly parsed by the
 *  configuration parser.
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST(parser, endpoint) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);

  // Data.
  std::string data{
      "\n{"
      "  \"centreonBroker\": {\n"
      "    \"input\": {\n"
      "      \"name\": \"CentreonInput\",\n"
      "      \"type\": \"tcp\",\n"
      "      \"port\": \"5668\",\n"
      "      \"protocol\": \"ndo\",\n"
      "      \"compression\": \"yes\"\n"
      "    },\n"
      "    \"output\": [\n"
      "      {\n"
      "        \"name\": \"CentreonDatabase\",\n"
      "        \"type\": \"sql\",\n"
      "        \"db_type\": \"mysql\",\n"
      "        \"db_host\": \"localhost\",\n"
      "        \"db_socket\": \"/var/lib/mysql/mysql.sock\",\n"
      "        \"db_port\": \"3306\",\n"
      "        \"db_user\": \"centreon\",\n"
      "        \"db_password\": \"merethis\",\n"
      "        \"db_name\": \"centreon_storage\",\n"
      "        \"failover\": \"CentreonRetention\",\n"
      "        \"secondary_failover\": [\n"
      "          \"CentreonSecondaryFailover1\",\n"
      "          \"CentreonSecondaryFailover2\"\n"
      "        ],\n"
      "        \"buffering_timeout\": \"10\",\n"
      "        \"read_timeout\": \"5\",\n"
      "        \"retry_interval\": \"300\"\n"
      "      },\n"
      "      {\n"
      "        \"name\": \"CentreonRetention\",\n"
      "        \"type\": \"ipv4\",\n"
      "        \"path\": \"retention.dat\",\n"
      "        \"protocol\": \"ndo\"\n"
      "      },\n"
      "      {\n"
      "        \"name\": \"CentreonSecondaryFailover1\",\n"
      "        \"type\": \"ipv4\",\n"
      "        \"path\": \"retention.dat\",\n"
      "        \"protocol\": \"ndo\"\n"
      "      },\n"
      "      {\n"
      "        \"name\": \"CentreonSecondaryFailover2\",\n"
      "        \"type\": \"ipv4\",\n"
      "        \"path\": \"retention.dat\",\n"
      "        \"protocol\": \"ndo\"\n"
      "      }\n"
      "    ]\n"
      "  }\n"
      "}\n"};

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  config::state s{p.parse(config_file)};

  // Remove temporary file.
  ::remove(config_file.c_str());

  // Check against expected result.
  ASSERT_EQ(s.endpoints().size(), 5u);

  // Check input #1.
  std::list<config::endpoint>::const_iterator it(s.endpoints().begin());
  config::endpoint input1(*(it++));
  ASSERT_EQ(input1.name, "CentreonInput");
  ASSERT_EQ(input1.type, "tcp");
  ASSERT_EQ(input1.params["port"], "5668");
  ASSERT_EQ(input1.params["protocol"], "ndo");
  ASSERT_EQ(input1.params["compression"], "yes");

  // Check output #1.
  config::endpoint output1(*(it++));
  ASSERT_EQ(output1.name, "CentreonDatabase");
  ASSERT_EQ(output1.type, "sql");
  ASSERT_EQ(output1.failovers.size(), 1u);
  ASSERT_EQ(output1.failovers.front(), "CentreonRetention");
  ASSERT_EQ(output1.buffering_timeout, 10);
  ASSERT_EQ(output1.read_timeout, 5);
  ASSERT_EQ(output1.retry_interval, 300u);
  ASSERT_EQ(output1.params["db_type"], "mysql");
  ASSERT_EQ(output1.params["db_host"], "localhost");
  ASSERT_EQ(output1.params["db_socket"], "/var/lib/mysql/mysql.sock");
  ASSERT_EQ(output1.params["db_port"], "3306");
  ASSERT_EQ(output1.params["db_user"], "centreon");
  ASSERT_EQ(output1.params["db_password"], "merethis");
  ASSERT_EQ(output1.params["db_name"], "centreon_storage");

  // Check output #2.
  config::endpoint output2(*(it++));
  ASSERT_EQ(output2.name, "CentreonRetention");
  ASSERT_EQ(output2.type, "ipv4");
  ASSERT_EQ(output2.params["path"], "retention.dat");
  ASSERT_EQ(output2.params["protocol"], "ndo");

  // Check output #3.
  config::endpoint output3(*(it++));
  ASSERT_EQ(output3.name, "CentreonSecondaryFailover1");
  ASSERT_EQ(output3.type, "ipv4");
  ASSERT_EQ(output3.params["path"], "retention.dat");
  ASSERT_EQ(output3.params["protocol"], "ndo");

  // Check output #4.
  config::endpoint output4(*it);
  ASSERT_EQ(output4.name, "CentreonSecondaryFailover2");
  ASSERT_EQ(output4.type, "ipv4");
  ASSERT_EQ(output4.params["path"], "retention.dat");
  ASSERT_EQ(output4.params["protocol"], "ndo");
}

/**
 *  Check that 'logger's are properly parsed by the configuration
 *  parser.
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST(parser, global) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": []\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), std::exception);
}

TEST(parser, log) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log\": {\n"
      "       \"directory\": \"/tmp\"\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  config::state s{p.parse(config_file)};

  // Remove temporary file.
  ::remove(config_file.c_str());

  // Check global params
  ASSERT_EQ(s.rpc_port(), 0);
  ASSERT_EQ(s.broker_id(), 1);
  ASSERT_EQ(s.broker_name(), "central-broker-master");
  ASSERT_EQ(s.poller_id(), 1);
  ASSERT_EQ(s.module_directory(), "/etc");
  ASSERT_EQ(s.event_queue_max_size(), 100000);
  ASSERT_EQ(s.command_file(), "/var/lib/centreon-broker/command.sock");
  ASSERT_EQ(s.cache_directory(), "/tmp/");
  ASSERT_EQ(s.log_conf().dirname(), "/tmp");
  ASSERT_EQ(s.log_conf().max_size(), 0u);
}

TEST(parser, logBadFilename) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": {\n"
      "       \"filename\": \"toto/titi\"\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), msg_fmt);

  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, logDefaultDir) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": {\n"
      "       \"directory\": \"/tmp\",\n"
      "       \"filename\": \"toto\",\n"
      "       \"max_size\": \"12345\",\n"
      "       \"loggers\": {\n"
      "         \"tcp\": \"warning\",\n"
      "         \"bam\": \"critical\"\n"
      "       }\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  config::state s{p.parse(config_file)};

  // Remove temporary file.
  ::remove(config_file.c_str());
  ASSERT_EQ(s.log_conf().dirname(), "/tmp");
  ASSERT_EQ(s.log_conf().filename(), "toto");
  ASSERT_EQ(s.log_conf().max_size(), 12345u);
  ASSERT_EQ(s.log_conf().loggers().size(), 2u);
}

TEST(parser, logBadMaxSize) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": {\n"
      "       \"filename\": \"toto\"\n"
      "       \"max_size\": \"12a345\"\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), msg_fmt);

  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, logBadLoggers) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": {\n"
      "       \"filename\": \"toto\"\n"
      "       \"max_size\": \"12345\"\n"
      "       \"loggers\": []\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), msg_fmt);

  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, logBadLogger) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": {\n"
      "       \"filename\": \"toto\"\n"
      "       \"max_size\": \"12345\"\n"
      "       \"loggers\": { \"minou\": \"trace\" }\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), msg_fmt);

  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, logWithNullLoggers) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log_thread_id\": false,\n"
      "     \"log\": {\n"
      "       \"directory\": \"/tmp\",\n"
      "       \"loggers\": null\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_NO_THROW(p.parse(config_file));

  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, unifiedSql) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data{
      "{"
      "    \"centreonBroker\": {\n"
      "        \"broker_id\": 1,\n"
      "        \"broker_name\": \"central-broker-master\",\n"
      "        \"poller_id\": 1,\n"
      "        \"bbdo_version\": \"3.1.2\",\n"
      "        \"poller_name\": \"Central\",\n"
      "        \"module_directory\": "
      "\"/etc\",\n"
      "        \"log_timestamp\": true,\n"
      "        \"log_thread_id\": false,\n"
      "        \"event_queue_max_size\": 100000,\n"
      "        \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "        \"cache_directory\": \"/tmp\",\n"
      "        \"input\": [\n"
      "            {\n"
      "                \"name\": \"connection-to-local\",\n"
      "                \"port\": \"5668\",\n"
      "                \"protocol\": \"bbdo\",\n"
      "                \"tls\": \"no\",\n"
      "                \"private_key\": \"/etc/centreon-broker/server.key\",\n"
      "                \"public_cert\": \"/etc/centreon-broker/server.crt\",\n"
      "                \"ca_certificate\": "
      "\"/etc/centreon-broker/client.crt\",\n"
      "                \"negotiation\": \"yes\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"retry_interval\": \"60\",\n"
      "                \"one_peer_retention_mode\": \"no\",\n"
      "                \"compression\": \"no\",\n"
      "                \"type\": \"ipv4\"\n"
      "            }\n"
      "        ],\n"
      "        \"logger\": [\n"
      "            {\n"
      "                \"name\": "
      "\"/tmp/central-broker-master.log\",\n"
      "                \"config\": \"yes\",\n"
      "                \"debug\": \"no\",\n"
      "                \"error\": \"yes\",\n"
      "                \"info\": \"no\",\n"
      "                \"level\": \"low\",\n"
      "                \"type\": \"file\"\n"
      "            }\n"
      "        ],\n"
      "        \"output\": [\n"
      "            {\n"
      "                \"type\": \"unified_sql\",\n"
      "                \"name\": \"central-broker-master-sql\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"db_host\": \"localhost\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_name\": \"centreon_storage\",\n"
      "                \"interval\": \"60\",\n"
      "                \"length\": \"15552000\",\n"
      "                \"queries_per_transaction\": \"20000\",\n"
      "                \"connections_count\": \"4\",\n"
      "                \"read_timeout\": \"60\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"retry_interval\": \"60\",\n"
      "                \"check_replication\": \"no\",\n"
      "                \"store_in_data_bin\": \"yes\",\n"
      "                \"insert_in_index_data\": \"1\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"centreon-broker-master-rrd\",\n"
      "                \"port\": \"5670\",\n"
      "                \"host\": \"localhost\",\n"
      "                \"protocol\": \"bbdo\",\n"
      "                \"tls\": \"no\",\n"
      "                \"private_key\": \"/etc/centreon-broker/client.key\",\n"
      "                \"public_cert\": \"/etc/centreon-broker/client.crt\",\n"
      "                \"ca_certificate\": "
      "\"/etc/centreon-broker/server.crt\",\n"
      "                \"negotiation\": \"yes\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"retry_interval\": \"60\",\n"
      "                \"one_peer_retention_mode\": \"no\",\n"
      "                \"compression\": \"no\",\n"
      "                \"type\": \"ipv4\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"centreon-bam-monitoring\",\n"
      "                \"cache\": \"yes\",\n"
      "                \"check_replication\": \"no\",\n"
      "                \"command_file\": "
      "\"/var/lib/centreon-engine/rw/centengine.cmd\",\n"
      "                \"db_host\": \"localhost\",\n"
      "                \"db_name\": \"centreon\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"queries_per_transaction\": \"0\",\n"
      "                \"storage_db_name\": \"centreon_storage\",\n"
      "                \"type\": \"bam\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"centreon-bam-reporting\",\n"
      "                \"filters\": {\n"
      "                    \"category\": [\n"
      "                        \"bam\"\n"
      "                    ]\n"
      "                },\n"
      "                \"check_replication\": \"no\",\n"
      "                \"db_host\": \"localhost\",\n"
      "                \"db_name\": \"centreon_storage\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"queries_per_transaction\": \"0\",\n"
      "                \"type\": \"bam_bi\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"test-cache\",\n"
      "                \"path\": \"/usr/share/centreon-broker/lua/test.lua\",\n"
      "                \"type\": \"lua\"\n"
      "            }\n"
      "        ],\n"
      "        \"stats\": [\n"
      "            {\n"
      "                \"type\": \"stats\",\n"
      "                \"name\": \"central-broker-master-stats\",\n"
      "                \"json_fifo\": "
      "\"/var/lib/centreon-broker/central-broker-master-stats.json\"\n"
      "            }\n"
      "        ],\n"
      "        \"grpc\": {\n"
      "            \"port\": 51001\n"
      "        }\n"
      "    }\n"
      "}"};

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  auto retval = p.parse(config_file);
  ASSERT_EQ(retval.get_bbdo_version().major_v, 3u);
  ASSERT_EQ(retval.get_bbdo_version().minor_v, 1u);
  ASSERT_EQ(retval.get_bbdo_version().patch, 2u);
  ASSERT_EQ(retval.get_bbdo_version().total_version, 0x300010002);
  // Remove temporary file.
  ::remove(config_file.c_str());
}

// UnifiedSql is not compatible with storage/sql. The parser returns an error.
TEST(parser, unifiedSqlVsStorageSql) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data{
      "{"
      "    \"centreonBroker\": {\n"
      "        \"broker_id\": 1,\n"
      "        \"broker_name\": \"central-broker-master\",\n"
      "        \"poller_id\": 1,\n"
      "        \"bbdo_version\": \"3.1.2\",\n"
      "        \"poller_name\": \"Central\",\n"
      "        \"module_directory\": "
      "\"/etc\",\n"
      "        \"log_timestamp\": true,\n"
      "        \"log_thread_id\": false,\n"
      "        \"event_queue_max_size\": 100000,\n"
      "        \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "        \"cache_directory\": \"/tmp\",\n"
      "        \"input\": [\n"
      "            {\n"
      "                \"name\": \"connection-to-local\",\n"
      "                \"port\": \"5668\",\n"
      "                \"protocol\": \"bbdo\",\n"
      "                \"tls\": \"no\",\n"
      "                \"private_key\": \"/etc/centreon-broker/server.key\",\n"
      "                \"public_cert\": \"/etc/centreon-broker/server.crt\",\n"
      "                \"ca_certificate\": "
      "\"/etc/centreon-broker/client.crt\",\n"
      "                \"negotiation\": \"yes\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"retry_interval\": \"60\",\n"
      "                \"one_peer_retention_mode\": \"no\",\n"
      "                \"compression\": \"no\",\n"
      "                \"type\": \"ipv4\"\n"
      "            }\n"
      "        ],\n"
      "        \"logger\": [\n"
      "            {\n"
      "                \"name\": "
      "\"/tmp/central-broker-master.log\",\n"
      "                \"config\": \"yes\",\n"
      "                \"debug\": \"no\",\n"
      "                \"error\": \"yes\",\n"
      "                \"info\": \"no\",\n"
      "                \"level\": \"low\",\n"
      "                \"type\": \"file\"\n"
      "            }\n"
      "        ],\n"
      "        \"output\": [\n"
      "            {\n"
      "                \"name\": \"central-broker-master-sql\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"retry_interval\": \"5\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"db_host\": \"1.2.3.4\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_name\": \"centreon_storage\",\n"
      "                \"queries_per_transaction\": \"1000\",\n"
      "                \"connections_count\": \"3\",\n"
      "                \"read_timeout\": \"1\",\n"
      "                \"type\": \"sql\"\n"
      "            },\n"
      "            {\n"
      "                \"type\": \"unified_sql\",\n"
      "                \"name\": \"central-broker-master-sql\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"db_host\": \"localhost\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_name\": \"centreon_storage\",\n"
      "                \"interval\": \"60\",\n"
      "                \"length\": \"15552000\",\n"
      "                \"queries_per_transaction\": \"20000\",\n"
      "                \"connections_count\": \"4\",\n"
      "                \"read_timeout\": \"60\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"retry_interval\": \"60\",\n"
      "                \"check_replication\": \"no\",\n"
      "                \"store_in_data_bin\": \"yes\",\n"
      "                \"insert_in_index_data\": \"1\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"centreon-broker-master-rrd\",\n"
      "                \"port\": \"5670\",\n"
      "                \"host\": \"localhost\",\n"
      "                \"protocol\": \"bbdo\",\n"
      "                \"tls\": \"no\",\n"
      "                \"private_key\": \"/etc/centreon-broker/client.key\",\n"
      "                \"public_cert\": \"/etc/centreon-broker/client.crt\",\n"
      "                \"ca_certificate\": "
      "\"/etc/centreon-broker/server.crt\",\n"
      "                \"negotiation\": \"yes\",\n"
      "                \"buffering_timeout\": \"0\",\n"
      "                \"retry_interval\": \"60\",\n"
      "                \"one_peer_retention_mode\": \"no\",\n"
      "                \"compression\": \"no\",\n"
      "                \"type\": \"ipv4\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"centreon-bam-monitoring\",\n"
      "                \"cache\": \"yes\",\n"
      "                \"check_replication\": \"no\",\n"
      "                \"command_file\": "
      "\"/var/lib/centreon-engine/rw/centengine.cmd\",\n"
      "                \"db_host\": \"localhost\",\n"
      "                \"db_name\": \"centreon\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"queries_per_transaction\": \"0\",\n"
      "                \"storage_db_name\": \"centreon_storage\",\n"
      "                \"type\": \"bam\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"centreon-bam-reporting\",\n"
      "                \"filters\": {\n"
      "                    \"category\": [\n"
      "                        \"bam\"\n"
      "                    ]\n"
      "                },\n"
      "                \"check_replication\": \"no\",\n"
      "                \"db_host\": \"localhost\",\n"
      "                \"db_name\": \"centreon_storage\",\n"
      "                \"db_password\": \"centreon\",\n"
      "                \"db_port\": \"3306\",\n"
      "                \"db_type\": \"mysql\",\n"
      "                \"db_user\": \"centreon\",\n"
      "                \"queries_per_transaction\": \"0\",\n"
      "                \"type\": \"bam_bi\"\n"
      "            },\n"
      "            {\n"
      "                \"name\": \"test-cache\",\n"
      "                \"path\": \"/usr/share/centreon-broker/lua/test.lua\",\n"
      "                \"type\": \"lua\"\n"
      "            }\n"
      "        ],\n"
      "        \"stats\": [\n"
      "            {\n"
      "                \"type\": \"stats\",\n"
      "                \"name\": \"central-broker-master-stats\",\n"
      "                \"json_fifo\": "
      "\"/var/lib/centreon-broker/central-broker-master-stats.json\"\n"
      "            }\n"
      "        ],\n"
      "        \"grpc\": {\n"
      "            \"port\": 51001\n"
      "        }\n"
      "    }\n"
      "}"};

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), std::exception);
  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, grpc_full) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log\": {\n"
      "       \"directory\": \"/tmp\"\n"
      "     },\n"
      "     \"grpc\": {\n"
      "       \"rpc_port\": 51001,\n"
      "       \"listen_address\": \"10.0.2.26\"\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  config::state s{p.parse(config_file)};

  // Remove temporary file.
  ::remove(config_file.c_str());

  // Check global params
  ASSERT_EQ(s.rpc_port(), 51001);
  ASSERT_EQ(s.listen_address(), std::string("10.0.2.26"));
  ASSERT_EQ(s.broker_id(), 1);
  ASSERT_EQ(s.broker_name(), "central-broker-master");
  ASSERT_EQ(s.poller_id(), 1);
  ASSERT_EQ(s.module_directory(), "/etc");
  ASSERT_EQ(s.event_queue_max_size(), 100000);
  ASSERT_EQ(s.command_file(), "/var/lib/centreon-broker/command.sock");
  ASSERT_EQ(s.cache_directory(), "/tmp/");
  ASSERT_EQ(s.log_conf().dirname(), "/tmp");
  ASSERT_EQ(s.log_conf().max_size(), 0u);
}

TEST(parser, grpc_in_error) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data;
  data =
      "{\n"
      "  \"centreonBroker\": {\n"
      "     \"broker_id\": 1,\n"
      "     \"broker_name\": \"central-broker-master\",\n"
      "     \"poller_id\": 1,\n"
      "     \"poller_name\": \"Central\",\n"
      "     \"module_directory\": "
      "\"/etc\",\n"
      "     \"log_timestamp\": true,\n"
      "     \"event_queue_max_size\": 100000,\n"
      "     \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "     \"cache_directory\": \"/tmp\",\n"
      "     \"log\": {\n"
      "       \"directory\": \"/tmp\"\n"
      "     },\n"
      "     \"grpc\": {\n"
      "       \"rpc_port\": \"foo\",\n"
      "       \"listen_address\": \"10.0.2.26\"\n"
      "     }\n"
      "  }\n"
      "}\n";

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), std::exception);

  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, flush_period) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data{
      "{"
      "    \"centreonBroker\": {\n"
      "        \"broker_id\": 1,\n"
      "        \"broker_name\": \"central-broker-master\",\n"
      "        \"poller_id\": 1,\n"
      "        \"bbdo_version\": \"3.1.2\",\n"
      "        \"poller_name\": \"Central\",\n"
      "        \"module_directory\": "
      "\"/etc\",\n"
      "        \"log_timestamp\": true,\n"
      "        \"log_thread_id\": false,\n"
      "        \"event_queue_max_size\": 100000,\n"
      "        \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "        \"cache_directory\": \"/tmp\",\n"
      "        \"input\": [\n"
      "            {\n"
      "                \"name\": \"connection-to-local\",\n"
      "                \"port\": \"5668\",\n"
      "                \"transport_protocol\": \"tcp\",\n"
      "                \"type\": \"bbdo_server\"\n"
      "            }\n"
      "        ],\n"
      "        \"log\": {\n"
      "          \"flush_period\": -12\n"
      "        },\n"
      "        \"logger\": [\n"
      "            {\n"
      "                \"name\": "
      "\"/tmp/central-broker-master.log\",\n"
      "                \"config\": \"yes\",\n"
      "                \"debug\": \"no\",\n"
      "                \"error\": \"yes\",\n"
      "                \"info\": \"no\",\n"
      "                \"level\": \"low\",\n"
      "                \"type\": \"file\"\n"
      "            }\n"
      "        ],\n"
      "        \"output\": [\n"
      "            {\n"
      "                \"name\": \"centreon-broker-master-rrd\",\n"
      "                \"port\": \"5670\",\n"
      "                \"host\": \"localhost\",\n"
      "                \"transport_protocol\": \"tcp\",\n"
      "                \"type\": \"bbdo_client\"\n"
      "            }\n"
      "        ],\n"
      "        \"stats\": [\n"
      "            {\n"
      "                \"type\": \"stats\",\n"
      "                \"name\": \"central-broker-master-stats\",\n"
      "                \"json_fifo\": "
      "\"/var/lib/centreon-broker/central-broker-master-stats.json\"\n"
      "            }\n"
      "        ],\n"
      "        \"grpc\": {\n"
      "            \"port\": 51001\n"
      "        }\n"
      "    }\n"
      "}"};

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  ASSERT_THROW(p.parse(config_file), std::exception);
  // Remove temporary file.
  ::remove(config_file.c_str());
}

TEST(parser, boolean1) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data{
      "{"
      "    \"centreonBroker\": {\n"
      "        \"broker_id\": 1,\n"
      "        \"broker_name\": \"central-broker-master\",\n"
      "        \"poller_id\": 1,\n"
      "        \"bbdo_version\": \"3.1.2\",\n"
      "        \"poller_name\": \"Central\",\n"
      "        \"module_directory\": "
      "\"/etc\",\n"
      "        \"log_timestamp\": true,\n"
      "        \"log_thread_id\": false,\n"
      "        \"event_queue_max_size\": 100000,\n"
      "        \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "        \"cache_directory\": \"/tmp\",\n"
      "        \"input\": [\n"
      "            {\n"
      "                \"name\": \"connection-to-local\",\n"
      "                \"port\": \"5668\",\n"
      "                \"transport_protocol\": \"tCp\",\n"
      "                \"type\": \"bbdo_server\"\n"
      "            }\n"
      "        ],\n"
      "        \"log\": {\n"
      "          \"directory\": \"/tmp\",\n"
      "          \"flush_period\": 12,\n"
      "          \"log_pid\": \"yes\"\n"
      "        },\n"
      "        \"logger\": [\n"
      "            {\n"
      "                \"name\": "
      "\"/tmp/central-broker-master.log\",\n"
      "                \"config\": \"yes\",\n"
      "                \"debug\": \"no\",\n"
      "                \"error\": \"yes\",\n"
      "                \"info\": \"no\",\n"
      "                \"level\": \"low\",\n"
      "                \"type\": \"file\"\n"
      "            }\n"
      "        ],\n"
      "        \"output\": [\n"
      "            {\n"
      "                \"name\": \"centreon-broker-master-rrd\",\n"
      "                \"port\": \"5670\",\n"
      "                \"host\": \"localhost\",\n"
      "                \"transport_protocol\": \"tCp\",\n"
      "                \"type\": \"bbdo_client\"\n"
      "            }\n"
      "        ],\n"
      "        \"stats\": [\n"
      "            {\n"
      "                \"type\": \"stats\",\n"
      "                \"name\": \"central-broker-master-stats\",\n"
      "                \"json_fifo\": "
      "\"/var/lib/centreon-broker/central-broker-master-stats.json\"\n"
      "            }\n"
      "        ],\n"
      "        \"grpc\": {\n"
      "            \"port\": 51001\n"
      "        }\n"
      "    }\n"
      "}"};

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  config::state s{p.parse(config_file)};

  // Remove temporary file.
  ::remove(config_file.c_str());

  // Check global params
  ASSERT_TRUE(s.log_conf().log_pid());
}

TEST(parser, boolean2) {
  // File name.
  std::string config_file(misc::temp_path());

  // Open file.
  FILE* file_stream(fopen(config_file.c_str(), "w"));
  if (!file_stream)
    throw msg_fmt("could not open '{}'", config_file);
  // Data.
  std::string data{
      "{"
      "    \"centreonBroker\": {\n"
      "        \"broker_id\": 1,\n"
      "        \"broker_name\": \"central-broker-master\",\n"
      "        \"poller_id\": 1,\n"
      "        \"bbdo_version\": \"3.1.2\",\n"
      "        \"poller_name\": \"Central\",\n"
      "        \"module_directory\": "
      "\"/etc\",\n"
      "        \"log_timestamp\": true,\n"
      "        \"log_thread_id\": false,\n"
      "        \"event_queue_max_size\": 100000,\n"
      "        \"command_file\": \"/var/lib/centreon-broker/command.sock\",\n"
      "        \"cache_directory\": \"/tmp\",\n"
      "        \"input\": [\n"
      "            {\n"
      "                \"name\": \"connection-to-local\",\n"
      "                \"port\": \"5668\",\n"
      "                \"transport_protocol\": \"tcp\",\n"
      "                \"type\": \"bbdo_server\"\n"
      "            }\n"
      "        ],\n"
      "        \"log\": {\n"
      "          \"directory\": \"/tmp\",\n"
      "          \"flush_period\": 12,\n"
      "          \"log_pid\": \"n\"\n"
      "        },\n"
      "        \"logger\": [\n"
      "            {\n"
      "                \"name\": "
      "\"/tmp/central-broker-master.log\",\n"
      "                \"config\": \"yes\",\n"
      "                \"debug\": \"no\",\n"
      "                \"error\": \"yes\",\n"
      "                \"info\": \"no\",\n"
      "                \"level\": \"low\",\n"
      "                \"type\": \"file\"\n"
      "            }\n"
      "        ],\n"
      "        \"output\": [\n"
      "            {\n"
      "                \"name\": \"centreon-broker-master-rrd\",\n"
      "                \"port\": \"5670\",\n"
      "                \"host\": \"localhost\",\n"
      "                \"transport_protocol\": \"tcp\",\n"
      "                \"type\": \"bbdo_client\"\n"
      "            }\n"
      "        ],\n"
      "        \"stats\": [\n"
      "            {\n"
      "                \"type\": \"stats\",\n"
      "                \"name\": \"central-broker-master-stats\",\n"
      "                \"json_fifo\": "
      "\"/var/lib/centreon-broker/central-broker-master-stats.json\"\n"
      "            }\n"
      "        ],\n"
      "        \"grpc\": {\n"
      "            \"port\": 51001\n"
      "        }\n"
      "    }\n"
      "}"};

  // Write data.
  if (fwrite(data.c_str(), data.size(), 1, file_stream) != 1)
    throw msg_fmt("could not write content of '{}'", config_file);

  // Close file.
  fclose(file_stream);

  // Parse.
  config::parser p;
  config::state s{p.parse(config_file)};

  // Remove temporary file.
  ::remove(config_file.c_str());

  // Check global params
  ASSERT_FALSE(s.log_conf().log_pid());
}
