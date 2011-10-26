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

#include <QTemporaryFile>
#include <QString>
#include <QCoreApplication>
#include <QDebug>
#include <exception>
#include "test/unittest.hh"
#include "logging/engine.hh"
#include "error.hh"
#include "commands.hh"
#include "globals.hh"

using namespace com::centreon::engine;

/**
 *  Run process_file test.
 */
static void check_process_file() {
  QTemporaryFile tmp("external_commands.cmd");
  if (!tmp.open())
    throw (engine_error() << "impossible to create temporary file.");
  tmp.write("[1317196300] ENABLE_NOTIFICATIONS\n");
  tmp.close();

  config.set_enable_notifications(false);
  QString cmd("[1317196300] PROCESS_FILE;" + tmp.fileName() + ";0\n");
  process_external_command(qPrintable(cmd));

  if (!config.get_enable_notifications())
    throw (engine_error() << "process_file failed.");
}

/**
 *  Check processing of process_file works.
 */
int main_test() {
  logging::engine& engine = logging::engine::instance();
  check_process_file();
  engine.cleanup();
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
