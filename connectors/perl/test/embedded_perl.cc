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

#include <unistd.h>
#include <fstream>

#include "com/centreon/connector/perl/embedded_perl.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

static shared_io_context io_context(std::make_shared<asio::io_context>());

static std::string make_temp_path() {
  char tmpl[] = "/tmp/centreon_XXXXXX";
  int fd = mkstemp(tmpl);
  if (fd < 0)
    throw std::runtime_error(strerror(errno));
  close(fd);
  return tmpl;
}

TEST(EmbeddedPerl, RunSimple1) {
  // Return value.
  int retval(EXIT_FAILURE);

  // Write simple Perl script.
  std::string script_path = make_temp_path();
  ASSERT_NO_THROW({
    {
      std::ofstream fs(script_path);
      fs << "exit 42;\n";
    }

    // Compile and execute script.
    int fds[3];
    pid_t child(embedded_perl::instance().run(script_path, fds, io_context));

    // Wait for child termination.
    int status;
    if (waitpid(child, &status, 0) == child)
      retval = !(WIFEXITED(status) && (WEXITSTATUS(status) == 42));

    ASSERT_EQ(retval, 0);

    // Remove temporary file.
    remove(script_path.c_str());
  });
}

TEST(EmbeddedPerl, RunSimple2) {
  // Write simple Perl script.
  std::string script_path = make_temp_path();
  ASSERT_NO_THROW({
    {
      std::ofstream fs(script_path);
      fs << "my $x;\n"
            "my $y = 40;\n"
            "$x = 2;\n"
            "exit $x + $y;\n";
    }
    std::cout << script_path << std::endl;

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
