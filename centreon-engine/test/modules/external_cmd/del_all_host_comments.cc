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
#include "test/unittest.hh"
#include "logging/engine.hh"
#include "error.hh"
#include "commands.hh"
#include "globals.hh"

using namespace com::centreon::engine;

/**
 *  Run del_all_host_comments test.
 */
static void check_del_all_host_comments() {
  init_object_skiplists();

  host* hst = add_host("name", NULL, NULL, "localhost", NULL, 0, 0.0, 0.0, 42,
                       0, 0, 0, 0, 0, 0.0, 0.0, NULL, 0, NULL, 0, 0, NULL, 0,
                       0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0.0, 0.0,
                       0.0, 0, 0, 0, 0, 0);
  if (!hst)
    throw (engine_error() << "create host failed.");

  next_comment_id = 1;
  if (add_new_comment(HOST_COMMENT,
                      USER_COMMENT,
                      "name",
                      NULL,
                      time(NULL),
                      "user",
                      "data",
                      true,
                      COMMENTSOURCE_EXTERNAL,
                      false,
                      0,
                      NULL) == ERROR)
    throw (engine_error() << "create new comment failed.");

  if (add_new_comment(HOST_COMMENT,
                      USER_COMMENT,
                      "name",
                      NULL,
                      time(NULL),
                      "user",
                      "data",
                      true,
                      COMMENTSOURCE_EXTERNAL,
                      false,
                      0,
                      NULL) == ERROR)
    throw (engine_error() << "create new comment failed.");

  char const* cmd("[1317196300] DEL_ALL_HOST_COMMENTS;name");
  process_external_command(cmd);

  if (comment_list)
    throw (engine_error() << "del_host_comment failed.");

  delete[] hst->name;
  delete[] hst->display_name;
  delete[] hst->alias;
  delete[] hst->address;
  delete hst;

  free_object_skiplists();
}

/**
 *  Check processing of del_all_host_comments works.
 */
int main_test() {
  logging::engine& engine = logging::engine::instance();
  check_del_all_host_comments();
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
