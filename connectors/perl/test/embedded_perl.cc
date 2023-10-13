/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

/* Be careful! gtest.h must be included before embedded_perl.hh */

#include <gtest/gtest.h>

#include "com/centreon/connector/perl/embedded_perl.hh"

#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

static shared_io_context io_context(std::make_shared<asio::io_context>());

TEST(EmbeddedPerl, RunSimple1) {
  // Return value.
  int retval(EXIT_FAILURE);

  // Write simple Perl script.
  std::string script_path(com::centreon::io::file_stream::temp_path());
  ASSERT_NO_THROW({
    com::centreon::io::file_stream fs;
    fs.open(script_path.c_str(), "w");
    char const* data("exit 42;\n");
    unsigned int size(strlen(data));
    unsigned int rb(1);
    do {
      rb = fs.write(data, size);
      size -= rb;
      data += rb;
    } while (rb > 0 && size > 0);

    // Compile and execute script.
    int fds[3];
    pid_t child(embedded_perl::instance().run(script_path, fds, io_context));

    // Wait for child termination.
    int status;
    if (waitpid(child, &status, 0) == child)
      retval = !(WIFEXITED(status) && WEXITSTATUS(status) == 42);

    ASSERT_EQ(retval, 0);

    // Remove temporary file.
    remove(script_path.c_str());
  });
}

TEST(EmbeddedPerl, RunSimple2) {
  // Write simple Perl script.
  std::string script_path(com::centreon::io::file_stream::temp_path());
  ASSERT_NO_THROW({
    com::centreon::io::file_stream fs;
    fs.open(script_path.c_str(), "w");
    std::cout << script_path << std::endl;
    char const* data(
        "my $x;\n"
        "my $y = 40;\n"
        "$x = 2;\n"
        "exit $x + $y;\n");
    unsigned int size(strlen(data));
    unsigned int rb(1);
    do {
      rb = fs.write(data, size);
      size -= rb;
      data += rb;
    } while ((rb > 0) && (size > 0));

    // Compile and execute script.
    int fds[3];
    pid_t child(embedded_perl::instance().run(script_path, fds, io_context));

    // Wait for child termination.
    int status;
    if (waitpid(child, &status, 0) == child) {
      ASSERT_TRUE(WIFEXITED(status));
      ASSERT_EQ(WEXITSTATUS(status), 42);
    } else {
      ASSERT_TRUE(false);
    }

    child = embedded_perl::instance().run(script_path, fds, io_context);

    // Wait for child termination.
    if (waitpid(child, &status, 0) == child) {
      ASSERT_TRUE(WIFEXITED(status));
      ASSERT_EQ(WEXITSTATUS(status), 42);
    } else {
      ASSERT_TRUE(false);
    }

    // Remove temporary file.
    remove(script_path.c_str());
  });
}
