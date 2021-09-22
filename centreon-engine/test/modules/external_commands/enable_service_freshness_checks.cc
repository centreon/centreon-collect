/*
** Copyright 2011-2013 Merethis
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

#include <exception>
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/modules/external_commands/commands.hh"
#include "com/centreon/logging/engine.hh"
#include "test/unittest.hh"

using namespace com::centreon::engine;

/**
 *  Run enable_service_freshness_checks test.
 */
static int check_enable_service_freshness_checks(int argc, char** argv) {
  (void)argc;
  (void)argv;

  config->check_service_freshness(false);
  char const* cmd("[1317196300] ENABLE_SERVICE_FRESHNESS_CHECKS");
  process_external_command(cmd);

  if (!config->check_service_freshness())
    throw(engine_error() << "enable_service_freshness_checks failed.");

  return (0);
}

/**
 *  Init unit test.
 */
int main(int argc, char** argv) {
  unittest utest(argc, argv, &check_enable_service_freshness_checks);
  return (utest.run());
}
