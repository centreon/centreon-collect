/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
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
#include <EXTERN.h>
#include <gtest/gtest.h>
#include <perl.h>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/embedded_perl.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;

/**
 *  Tester entry point.
 *
 *  @param[in] argc  Argument count.
 *  @param[in] argv  Argument values.
 *
 *  @return 0 on success, any other value on failure.
 */
int main(int argc, char* argv[], char** env) {
  log::instance().set_level(spdlog::level::trace);
  log::instance().switch_to_stdout();
  log::instance().add_pid_to_log();
  // GTest initialization.
  testing::InitGoogleTest(&argc, argv);
  PERL_SYS_INIT3(&argc, &argv, &env);
  embedded_perl::load(argc, argv, env);
  // Run all tests.
  int ret = RUN_ALL_TESTS();

  PL_perl_destruct_level = 1;
  perl_destruct(my_perl);
  perl_free(my_perl);
  PERL_SYS_TERM();
  my_perl = nullptr;

  // Unload.
  embedded_perl::unload();

  return ret;
}
