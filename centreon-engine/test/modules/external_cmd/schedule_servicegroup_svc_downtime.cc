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
 *  Run schedule_servicegroup_svc_downtime test.
 */
static void check_schedule_servicegroup_svc_downtime() {
  init_object_skiplists();

  service* svc = add_service("name", "description", NULL,
                             NULL, 0, 42, 0, 0, 0, 42.0, 0.0, 0.0, NULL,
                             0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, "command", 0, 0,
                             0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                             0, 0, NULL, NULL, NULL, NULL, NULL,
                             0, 0, 0);
  if (!svc)
    throw (engine_error() << "create service failed.");

  servicegroup* group = add_servicegroup("group", NULL, NULL, NULL, NULL);
  if (!group)
    throw (engine_error() << "create servicegroup failed.");

  servicesmember* member = add_service_to_servicegroup(group, "name", "description");
  if (!member)
    throw (engine_error() << "create servicemember failed.");
  member->service_ptr = svc;

  scheduled_downtime_list = NULL;
  char const* cmd("[1317196300] SCHEDULE_SERVICEGROUP_SVC_DOWNTIME;group;1317196300;2000000000;0;0;7200;user;comment");
  process_external_command(cmd);

  if (!scheduled_downtime_list)
    throw (engine_error() << "schedule_servicegroup_svc_downtime failed.");

  delete[] member->host_name;
  delete[] member->service_description;
  delete member;

  delete[] group->group_name;
  delete[] group->alias;
  delete group;

  delete[] svc->host_name;
  delete[] svc->description;
  delete[] svc->service_check_command;
  delete[] svc->display_name;
  delete svc;

  free_object_skiplists();
}

/**
 *  Check processing of schedule_servicegroup_svc_downtime works.
 */
int main_test() {
  logging::engine& engine = logging::engine::instance();
  check_schedule_servicegroup_svc_downtime();
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
