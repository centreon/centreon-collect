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

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/globals.hh"
#include "helper.hh"

using namespace com::centreon::engine;

class GlobalExternalCommand : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }

 protected:
  /* Build a command line the way mmap_fgets() hands it over when reading the
   * external command file, that is with its trailing end of line. */
  static std::string line(const std::string& cmd) {
    return fmt::format("[{}] {}\n", std::time(nullptr), cmd);
  }
};

/**
 * @brief An argument less command is recognized even though the line it comes
 * from ends with an end of line. The name used to be built up to the '\0'
 * instead of up to the right trim, so the '\n' ended up inside it and every
 * program wide command was rejected as unrecognized.
 */
TEST_F(GlobalExternalCommand, ArgumentLessCommandIsRecognized) {
  pb_indexed_config.mut_state().set_enable_flap_detection(true);

  ASSERT_TRUE(commands::processing::execute(line("DISABLE_FLAP_DETECTION")));
  ASSERT_FALSE(pb_indexed_config.state().enable_flap_detection());

  ASSERT_TRUE(commands::processing::execute(line("ENABLE_FLAP_DETECTION")));
  ASSERT_TRUE(pb_indexed_config.state().enable_flap_detection());
}

/**
 * @brief Same thing with trailing blanks before the end of line: the right trim
 * takes care of them too.
 */
TEST_F(GlobalExternalCommand, ArgumentLessCommandIsTrimmed) {
  pb_indexed_config.mut_state().set_enable_flap_detection(true);

  ASSERT_TRUE(
      commands::processing::execute(line("DISABLE_FLAP_DETECTION  \t ")));
  ASSERT_FALSE(pb_indexed_config.state().enable_flap_detection());
}

/**
 * @brief A command carrying arguments is still parsed as before: its name stops
 * at the first ';', well before the right trim.
 */
TEST_F(GlobalExternalCommand, CommandWithArgumentsIsStillParsed) {
  /* No host named test_host here, so the command is recognized (this is what is
   * asserted) but has nothing to act upon. */
  ASSERT_TRUE(
      commands::processing::execute(line("ENABLE_HOST_FLAP_DETECTION;test")));
}

/**
 * @brief An unknown command is still rejected, end of line or not.
 */
TEST_F(GlobalExternalCommand, UnknownCommandIsRejected) {
  ASSERT_FALSE(commands::processing::execute(line("NO_SUCH_COMMAND")));
  ASSERT_FALSE(commands::processing::execute(line("NO_SUCH_COMMAND;arg")));
}

/**
 * @brief Malformed lines are rejected instead of being parsed into something.
 */
TEST_F(GlobalExternalCommand, MalformedLinesAreRejected) {
  ASSERT_FALSE(commands::processing::execute("DISABLE_FLAP_DETECTION\n"));
  ASSERT_FALSE(commands::processing::execute("[123 DISABLE_FLAP_DETECTION\n"));
  /* A single space is expected right after the ']', as in Nagios. */
  ASSERT_FALSE(commands::processing::execute("[123]DISABLE_FLAP_DETECTION\n"));
  /* No entry time at all. */
  ASSERT_FALSE(commands::processing::execute("[] DISABLE_FLAP_DETECTION\n"));
  /* An entry time that is not a number used to be silently taken as 0. */
  ASSERT_FALSE(commands::processing::execute("[abc] DISABLE_FLAP_DETECTION\n"));
}

/**
 * @brief The command line may be surrounded by blanks, and the entry time may
 * be padded inside the brackets.
 */
TEST_F(GlobalExternalCommand, SurroundingBlanksAreAccepted) {
  pb_indexed_config.mut_state().set_enable_flap_detection(true);

  ASSERT_TRUE(commands::processing::execute(
      fmt::format("  [ {} ] DISABLE_FLAP_DETECTION\n", std::time(nullptr))));
  ASSERT_FALSE(pb_indexed_config.state().enable_flap_detection());
}
