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

#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/retention/downtimes_file.hh"
#include "common/proto_file/proto_file.hh"
#include "helper.hh"

using namespace com::centreon::engine;

/**
 * @brief The files Engine keeps next to its configuration in centralized mode,
 * state.prot and downtimes.prot, read and written through proto_file.
 */
class ApplierStateProt : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;
  std::filesystem::path _saved_proto_conf;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();
    _saved_proto_conf = proto_conf;
    proto_conf = std::filesystem::temp_directory_path() /
                 ("ut_engine_state_prot_" + std::to_string(::getpid()));
    std::filesystem::remove_all(proto_conf);
    std::filesystem::create_directories(proto_conf);
  }

  void TearDown() override {
    std::filesystem::remove_all(proto_conf);
    proto_conf = _saved_proto_conf;
    deinit_config_state();
  }

  /** @brief Write bytes that are no protobuf message to @p name. */
  static void write_garbage(const std::string& name) {
    /* Field number 0 does not exist: no message starts with a null byte. */
    std::ofstream f(proto_conf / name, std::ios::binary);
    f.write("\0\1\2\3\4\5\6\7", 8);
  }
};

/**
 * Scenario: Engine gives Broker the configuration it stores.
 * Given a state.prot written compressed, as the step 6 of the configuration
 * delivery will do
 * When Broker asks for the current configuration
 * Then Engine reads it whole.
 */
TEST_F(ApplierStateProt, CurrentStateIsRead) {
  configuration::State written;
  written.set_poller_id(3);
  written.set_config_version("v1");
  std::error_code ec;
  com::centreon::common::write_proto_file(proto_conf / "state.prot", written,
                                          true, ec);
  ASSERT_FALSE(ec) << ec.message();

  configuration::State st;
  configuration::applier::state::instance().get_current_state(st);
  EXPECT_EQ(st.poller_id(), 3u);
  EXPECT_EQ(st.config_version(), "v1");
}

/**
 * Scenario: a state.prot that cannot be read is not sent to Broker.
 * Given a state.prot that is not a protobuf message
 * When Broker asks for the current configuration
 * Then Engine gives an empty State, whose poller_id 0 tells the event loop
 * there is nothing to send: what was partly read would become the
 * configuration Broker stores for this poller.
 */
TEST_F(ApplierStateProt, UnreadableCurrentStateIsEmpty) {
  write_garbage("state.prot");

  configuration::State st;
  st.set_poller_id(3);
  configuration::applier::state::instance().get_current_state(st);
  EXPECT_EQ(st.poller_id(), 0u);
  EXPECT_EQ(st.ByteSizeLong(), 0u);
}

/**
 * Scenario: the downtimes file survives its round trip, or its absence.
 * Given no downtimes.prot
 * When it is restored
 * Then nothing is restored
 * When the downtimes are saved, with none scheduled
 * Then the file exists, with no temporary file left beside it
 * And restoring it restores nothing.
 */
TEST_F(ApplierStateProt, DowntimesFileRoundTrip) {
  const auto path = proto_conf / "downtimes.prot";
  EXPECT_EQ(retention::downtimes_file::restore(path), 0u);

  ASSERT_TRUE(retention::downtimes_file::save(path));
  EXPECT_TRUE(std::filesystem::exists(path));
  size_t files = 0;
  for ([[maybe_unused]] const auto& entry :
       std::filesystem::directory_iterator(proto_conf))
    ++files;
  EXPECT_EQ(files, 1u);
  EXPECT_EQ(retention::downtimes_file::restore(path), 0u);
}

/**
 * Scenario: a downtimes file that cannot be read restores nothing.
 * Given a downtimes.prot that is not a protobuf message
 * When it is restored
 * Then nothing is restored, and Engine goes on.
 */
TEST_F(ApplierStateProt, UnreadableDowntimesFileRestoresNothing) {
  write_garbage("downtimes.prot");
  EXPECT_EQ(retention::downtimes_file::restore(proto_conf / "downtimes.prot"),
            0u);
}
