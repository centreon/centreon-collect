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

using namespace com::centreon::engine;
using namespace com::centreon::engine::commands;

#define CMD_NAME "command_name"
#define CMD_LINE "command_name arg1 arg2"

/**
 *  Check constructor and copy object.
 */
int main_test() {
  raw cmd1(CMD_NAME, CMD_LINE);
  if (cmd1.get_name() != CMD_NAME
      || cmd1.get_command_line() != CMD_LINE)
    throw (engine_error() << "error: Constructor failed.");

  raw cmd2(cmd1);
  if (cmd2 != cmd1)
    throw (engine_error() << "error: Default copy constructor failed.");

  raw cmd3 = cmd2;
  if (cmd3 != cmd2)
    throw (engine_error() << "error: Default copy operator failed.");

  QSharedPointer<commands::command> cmd4(cmd3.clone());
  if (cmd4.isNull() == true)
    throw (engine_error() << "error: clone failed.");

  if (*cmd4 != cmd3)
    throw (engine_error() << "error: clone failed.");

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
