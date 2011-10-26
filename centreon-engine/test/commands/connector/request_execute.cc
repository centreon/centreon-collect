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
#include "commands/connector/execute_query.hh"
#include "commands/connector/execute_response.hh"
#include "test/commands/connector/check_request.hh"
#include "engine.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::commands::connector;

#define ID        1
#define TIMEOUT   42
#define TIMESTAMP 1308131877
#define BINARY    "./bin"

#define IS_EXECUTED 1
#define STDERR      ""
#define STDOUT      "is valid request"

#define QUERY    "2\0" TOSTR(ID) "\0" TOSTR(TIMEOUT) "\0" TOSTR(TIMESTAMP) "\0" BINARY CMD_END
#define RESPONSE "3\0" TOSTR(ID) "\0" TOSTR(IS_EXECUTED) "\0" TOSTR(STATE_OK) "\0" TOSTR(TIMESTAMP) "\0" STDERR "\0" STDOUT CMD_END

/**
 *  Check the execute request.
 */
int main_test() {
  QDateTime time;
  time.setTime_t(TIMESTAMP);
  execute_query query(ID, BINARY, time, TIMEOUT);
  if (check_request_valid(&query, REQUEST(QUERY)) == false)
    throw (engine_error() << "error: query is valid failed.");
  if (check_request_invalid(&query) == false)
    throw (engine_error() << "error: query is invalid failed.");
  if (check_request_clone(&query) == false)
    throw (engine_error() << "error: query clone failed");

  execute_response response(ID, IS_EXECUTED, STATE_OK, time, STDERR, STDOUT);
  if (check_request_valid(&response, REQUEST(RESPONSE)) == false)
    throw (engine_error() << "error: response is valid failed.");
  if (check_request_invalid(&response) == false)
    throw (engine_error() << "error: response is invalid failed.");
  if (check_request_clone(&response) == false)
    throw (engine_error() << "error: response clone failed");

  return (0);
}

/**
 *  Init the unit test.
 */
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  unittest utest(&main_test);
  QObject::connect(&utest, SIGNAL(finished()), &app, SLOT(quit()));
  utest.start();
  app.exec();
  return (utest.ret());
}
