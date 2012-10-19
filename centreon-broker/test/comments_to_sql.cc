/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QVariant>
#include <sstream>
#include "com/centreon/broker/exceptions/msg.hh"
#include "test/config.hh"
#include "test/engine.hh"
#include "test/external_command.hh"
#include "test/generate.hh"
#include "test/vars.hh"

using namespace com::centreon::broker;

#define DB_NAME "broker_comments_to_sql"

/**
 *  Check that comments are properly inserted in SQL database.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  // Variables that need cleaning.
  std::list<host> hosts;
  std::list<service> services;
  std::string engine_config_path(tmpnam(NULL));
  external_command commander;
  engine daemon;

  try {
    // Prepare database.
    QSqlDatabase db(config_db_open(DB_NAME));

    // Prepare monitoring engine configuration parameters.
    generate_hosts(hosts, 10);
    generate_services(services, hosts, 5);
    commander.set_file(tmpnam(NULL));
    std::string additional_config;
    {
      std::ostringstream oss;
      oss << commander.get_engine_config()
          << "broker_module=" << CBMOD_PATH << " "
          << PROJECT_SOURCE_DIR << "/test/cfg/comments_to_sql.xml\n";
      additional_config = oss.str();
    }

    // Generate monitoring engine configuration files.
    config_write(
      engine_config_path.c_str(),
      additional_config.c_str(),
      &hosts,
      &services);

    // Start monitoring engine.
    std::string engine_config_file(engine_config_path);
    engine_config_file.append("/nagios.cfg");
    daemon.set_config_file(engine_config_file);
    daemon.start();

    // Base time.
    time_t now(time(NULL));

    // Let the daemon initialize.
    sleep(10);

    // Add comments on two hosts.
    commander.execute(
                "ADD_HOST_COMMENT;21;1;Merethis;Centreon");
    commander.execute(
                "ADD_HOST_COMMENT;42;0;RandomAuthor;Some random comment.");

    // Add comments on two services.
    commander.execute(
                "ADD_SVC_COMMENT;3;13;1;Broker;Another comment !");
    commander.execute(
                "ADD_SVC_COMMENT;1;1;0;FooBar;Baz    Qux");

    // Let the monitoring engine run a while.
    sleep(4);

    // Check comments.
    {
      std::ostringstream query;
      query << "SELECT internal_id, host_id, service_id, author, data, persistent, entry_time, deletion_time"
            << "  FROM comments"
            << "  ORDER BY internal_id";
      QSqlQuery q(db);
      if (!q.exec(query.str().c_str()))
        throw (exceptions::msg() << "cannot get comments from DB: "
               << q.lastError().text().toStdString().c_str());
      
      if (// Host comment #1.
          !q.next()
          || (q.value(0).toUInt() != 1)
          || (q.value(1).toUInt() != 21)
          || !q.value(2).isNull()
          || (q.value(3).toString() != "Merethis")
          || (q.value(4).toString() != "Centreon")
          || !q.value(5).toUInt()
          || (q.value(6).toUInt() - 30 < now)
          || !q.value(7).isNull()
          // Host comment #2.
          || !q.next())
        throw (exceptions::msg() << "invalid comment entry in DB");
    }

    // Success.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "unknown exception" << std::endl;
  }

  // Cleanup.
  daemon.stop();
  //config_remove(engine_config_path.c_str());
  //config_db_close(DB_NAME);
  free_hosts(hosts);
  free_services(services);

  return (retval);
}
