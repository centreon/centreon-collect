/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <ctime>
#include <exception>
#include "com/centreon/engine/commands/result.hh"
#include "com/centreon/engine/error.hh"
#include "test/unittest.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::commands;

#define DEFAULT_ID 42
#define DEFAULT_STDOUT "stdout string test"
#define DEFAULT_STDERR "stderr string test"
#define DEFAULT_RETURN 0
#define DEFAULT_TIMEOUT true
#define DEFAULT_EXIT_OK false

/**
 *  Check the constructor and copy object.
 */
int main_test(int argc, char** argv) {
  (void)argc;
  (void)argv;

  // Default constructor.
  result res1;
  if (res1.get_command_id() != 0
      || res1.get_stdout() != ""
      || res1.get_stderr() != ""
      || res1.get_exit_code() != 0
      || res1.get_is_timeout() != false
      || res1.get_is_executed () != true
      || res1.get_start_time() != 0
      || res1.get_end_time() != 0)
    throw (engine_error() << "error: default constructor failed");

  // Constructor.
  com::centreon::timestamp now(com::centreon::timestamp::now());
  result res2(
           DEFAULT_ID,
           DEFAULT_STDOUT,
           DEFAULT_STDERR,
           now,
           now,
           DEFAULT_RETURN,
           DEFAULT_TIMEOUT,
           DEFAULT_EXIT_OK);
  if (res2.get_command_id() != DEFAULT_ID
      || res2.get_stdout() != DEFAULT_STDOUT
      || res2.get_stderr() != DEFAULT_STDERR
      || res2.get_exit_code() != DEFAULT_RETURN
      || res2.get_is_timeout() != DEFAULT_TIMEOUT
      || res2.get_is_executed() != DEFAULT_EXIT_OK
      || res2.get_start_time() != now
      || res2.get_end_time() != now)
    throw (engine_error() << "error: constructor failed");

  // Copy constructor.
  result res3(res2);
  if (res2 != res3)
    throw (engine_error() << "error: copy constructor failed");

  // Assignment operator.
  result res4;
  res4 = res3;
  if (res2 != res4)
    throw (engine_error() << "error: assignment operator failed");

  return (0);
}

/**
 *  Init unit test.
 */
int main(int argc, char** argv) {
  unittest utest(argc, argv, &main_test);
  return (utest.run());
}
