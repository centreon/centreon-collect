/*
** Copyright 2011 Merethis
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

#include <QCoreApplication>
#include <QDebug>
#include <exception>
#include "error.hh"
#include "test/unittest.hh"
#include "commands/raw.hh"
#include "globals.hh"
#include "objects.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::commands;

#define CMD_HOSTADDR  "localhost"
#define CMD_USER1     "/usr/bin"
#define CMD_ARG1      "default_arg"
#define CMD_LINE      "$USER1$/test -w $ARG1$ -c $$ARG1$$ $HOSTADDRESS$ $EMPTY$"
#define CMD_PROCESSED CMD_USER1 "/test -w " CMD_ARG1 " -c $ARG1$ " CMD_HOSTADDR " "

/**
 *  Check the process command line replacement macros.
 */
int main_test() {
  nagios_macros macros = nagios_macros();

  // add macros arg1.
  macros.argv[0] = new char[strlen(CMD_ARG1) + 1];
  strcpy(macros.argv[0], CMD_ARG1);

  // add macros user1.
  macro_user[0] = new char[strlen(CMD_USER1) + 1];
  strcpy(macro_user[0], CMD_USER1);

  // add macros hostaddress.
  macro_x_names[MACRO_HOSTADDRESS] = new char[strlen("HOSTADDRESS") + 1];
  strcpy(macro_x_names[MACRO_HOSTADDRESS], "HOSTADDRESS");

  host hst = host();
  hst.address = new char[strlen(CMD_HOSTADDR) + 1];
  strcpy(hst.address, CMD_HOSTADDR);
  macros.host_ptr = &hst;

  // process command.
  raw cmd(__func__, CMD_LINE);
  QString cmd_processed = cmd.process_cmd(&macros);

  delete[] hst.address;
  delete[] macro_x_names[MACRO_HOSTADDRESS];
  delete[] macro_user[0];
  delete[] macros.argv[0];

  if (cmd_processed != CMD_PROCESSED)
    throw (engine_error() << "command::process failed.");

  return (0);
}

/**
 *  Init unit test.
 */
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  unittest utest(&main_test);
  QObject::connect(&utest, SIGNAL(finished()), &app, SLOT(quit()));
  utest.start();
  app.exec();
  return (utest.ret());
}
