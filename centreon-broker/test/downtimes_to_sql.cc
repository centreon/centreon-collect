/*
** Copyright 2012-2015 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <sstream>
#include "com/centreon/broker/exceptions/msg.hh"
#include "test/broker_extcmd.hh"
#include "test/config.hh"
#include "test/engine.hh"
#include "test/engine_extcmd.hh"
#include "test/generate.hh"
#include "test/misc.hh"
#include "test/vars.hh"

using namespace com::centreon::broker;

#define DB_NAME "broker_downtimes_to_sql"

/**
 *  Check that downtimes are properly inserted in SQL database.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  // Variables that need cleaning.
  std::list<host> hosts;
  std::list<service> services;
  std::list<command> commands;
  std::string engine_config_path(tmpnam(NULL));
  engine_extcmd engine_commander;
  broker_extcmd broker_commander;
  engine daemon;
  test_file broker_cfg;
  test_db db;

  try {
    // Prepare database.
    db.open(DB_NAME);

    // Prepare monitoring engine configuration parameters.
    generate_commands(commands, 1);
    {
      char const* cmdline(MY_PLUGIN_PATH " 1");
      commands.front().command_line = new char[strlen(cmdline) + 1];
      strcpy(commands.front().command_line, cmdline);
    }
    generate_hosts(hosts, 10);
    for (std::list<host>::iterator it(hosts.begin()), end(hosts.end());
         it != end;
         ++it)
      if (!strcmp(it->name, "2")) {
        it->host_check_command = new char[2];
        strcpy(it->host_check_command, "1");
        break ;
      }
    generate_services(services, hosts, 5);
    for (std::list<service>::iterator
           it(services.begin()),
           end(services.end());
         it != end;
         ++it)
      if (!strcmp(it->host_name, "7")
          && !strcmp(it->description, "31")) {
        it->service_check_command = new char[2];
        strcpy(it->service_check_command, "1");
        break ;
      }
    engine_commander.set_file(tmpnam(NULL));
    broker_commander.set_file(tmpnam(NULL));
    broker_cfg.set_template(
      PROJECT_SOURCE_DIR "/test/cfg/downtimes_to_sql.xml.in");
    broker_cfg.set("BROKER_COMMAND_FILE", broker_commander.get_file());
    std::string additional_config;
    {
      std::ostringstream oss;
      oss << engine_commander.get_engine_config()
          << "broker_module=" << CBMOD_PATH << " "
          << broker_cfg.generate() << "\n";
      additional_config = oss.str();
    }

    // Generate monitoring engine configuration files.
    config_write(
      engine_config_path.c_str(),
      additional_config.c_str(),
      &hosts,
      &services,
      &commands);

    // Start monitoring engine.
    std::string engine_config_file(engine_config_path);
    engine_config_file.append("/nagios.cfg");
    daemon.set_config_file(engine_config_file);
    daemon.start();

    // Let the daemon initialize.
    sleep_for(10);

    // Set soon-to-be-in-downtime service as passive.
    {
      engine_commander.execute("DISABLE_SVC_CHECK;7;31");
    }

    // Run a little while.
    sleep_for(4);

    // Base time.
    time_t now(time(NULL));

    // Add downtimes on two hosts.
    {
      std::ostringstream oss;
      oss << "EXECUTE;84;downtimestosql-nodeevents;SCHEDULE_HOST_DOWNTIME;2;"
          << now << ";" << now + 3600
          << ";1;0;3600;Merethis;Centreon is beautiful";
      broker_commander.execute(oss.str().c_str());
    }
    {
      std::ostringstream oss;
      oss << "EXECUTE;84;downtimestosql-nodeevents;SCHEDULE_HOST_DOWNTIME;1;"
          << now + 1000 << ";" << now + 2000
          << ";0;0;123;Broker;Some random and useless comment.";
      broker_commander.execute(oss.str().c_str());
    }

    // Add downtimes on two services.
    {
      std::ostringstream oss;
      oss << "EXECUTE;84;downtimestosql-nodeevents;SCHEDULE_SVC_DOWNTIME;7;31;"
          << now << ";" << now + 8638
          << ";0;0;7129;Default Author; This is a comment !";
      broker_commander.execute(oss.str().c_str());
    }
    {
      std::ostringstream oss;
      oss << "EXECUTE;84;downtimestosql-nodeevents;SCHEDULE_SVC_DOWNTIME;10;48;"
          << now + 2834 << ";" << now + 987564
          << ";1;0;2;Author;Scheduling downtime";
      broker_commander.execute(oss.str().c_str());
    }

    // Let the monitoring engine run a while.
    sleep_for(20);

    // New time.
    time_t t1(now);
    now = time(NULL);

    // Check downtimes.
    {
      std::ostringstream query;
      query << "SELECT internal_id, host_id, service_id, entry_time,"
            << "       actual_end_time, actual_start_time, author,"
            << "       cancelled, comment_data, deletion_time,"
            << "       duration, end_time, fixed, start_time, started,"
            << "       triggered_by, type"
            << "  FROM rt_downtimes"
            << "  ORDER BY internal_id ASC";
      QSqlQuery q(*db.centreon_db());
      if (!q.exec(query.str().c_str()))
        throw (exceptions::msg() << "cannot get downtimes from DB: "
               << q.lastError().text().toStdString().c_str());

      if (// Host downtime #1.
          !q.next()
          || (q.value(0).toUInt() != 1)
          || (q.value(1).toUInt() != 2)
          || !q.value(2).isNull()
          || (static_cast<time_t>(q.value(3).toLongLong()) < t1)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          || !q.value(4).isNull()
          || (static_cast<time_t>(q.value(5).toLongLong()) < t1)
          || (static_cast<time_t>(q.value(5).toLongLong()) > now)
          || (q.value(6).toString() != "Merethis")
          || q.value(7).toUInt()
          || (q.value(8).toString() != "Centreon is beautiful")
          || !q.value(9).isNull()
          || (q.value(10).toUInt() != 3600)
          || (static_cast<time_t>(q.value(11).toLongLong()) != t1 + 3600)
          || !q.value(12).toUInt()
          || (static_cast<time_t>(q.value(13).toLongLong()) != t1)
          || !q.value(14).toUInt()
          || !q.value(15).isNull()
          || (q.value(16).toUInt() != 2)
          // Host downtime #2.
          || !q.next()
          || (q.value(0).toUInt() != 2)
          || (q.value(1).toUInt() != 1)
          || !q.value(2).isNull()
          || (static_cast<time_t>(q.value(3).toLongLong()) < t1)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          || !q.value(4).isNull()
          || !q.value(5).isNull()
          || (q.value(6).toString() != "Broker")
          || q.value(7).toUInt()
          || (q.value(8).toString() != "Some random and useless comment.")
          || !q.value(9).isNull()
          || (q.value(10).toUInt() != 123)
          || (static_cast<time_t>(q.value(11).toLongLong()) != t1 + 2000)
          || q.value(12).toUInt()
          || (static_cast<time_t>(q.value(13).toLongLong()) != t1 + 1000)
          || q.value(14).toUInt()
          || !q.value(15).isNull()
          || (q.value(16).toUInt() != 2)
          // Service downtime #1.
          || !q.next()
          || (q.value(0).toUInt() != 3)
          || (q.value(1).toUInt() != 7)
          || (q.value(2).toUInt() != 31)
          || (static_cast<time_t>(q.value(3).toLongLong()) < t1)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          || !q.value(4).isNull()
          || (static_cast<time_t>(q.value(5).toLongLong()) < t1)
          || (static_cast<time_t>(q.value(5).toLongLong()) > now)
          || (q.value(6).toString() != "Default Author")
          || q.value(7).toUInt()
          || (q.value(8).toString() != " This is a comment !")
          || !q.value(9).isNull()
          || (q.value(10).toUInt() != 7129)
          || (static_cast<time_t>(q.value(11).toLongLong()) != t1 + 8638)
          || q.value(12).toUInt()
          || (static_cast<time_t>(q.value(13).toLongLong()) != t1)
          || !q.value(14).toUInt()
          || !q.value(15).isNull()
          || (q.value(16).toUInt() != 1)
          // Service downtime #2.
          || !q.next()
          || (q.value(0).toUInt() != 4)
          || (q.value(1).toUInt() != 10)
          || (q.value(2).toUInt() != 48)
          || (static_cast<time_t>(q.value(3).toLongLong()) < t1)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          || !q.value(4).isNull()
          || !q.value(5).isNull()
          || (q.value(6).toString() != "Author")
          || q.value(7).toUInt()
          || (q.value(8).toString() != "Scheduling downtime")
          || !q.value(9).isNull()
          || (q.value(10).toUInt() != 984730)
          || (static_cast<time_t>(q.value(11).toLongLong())
              != t1 + 987564)
          || !q.value(12).toUInt()
          || (static_cast<time_t>(q.value(13).toLongLong())
              != t1 + 2834)
          || q.value(14).toUInt()
          || !q.value(15).isNull()
          || (q.value(16).toUInt() != 1)
          // EOF
          || q.next())
        throw (exceptions::msg() << "invalid downtime entry in DB");
    }

    // Check hosts.
    {
      std::ostringstream query;
      query << "SELECT COUNT(*)"
            << "  FROM rt_hosts"
            << "  WHERE scheduled_downtime_depth=0";
      QSqlQuery q(*db.centreon_db());
      if (!q.exec(query.str().c_str()))
        throw (exceptions::msg()
               << "cannot get host status from DB: "
               << qPrintable(q.lastError().text()));

      if (!q.next()
          || (q.value(0).toUInt() != (10 - 1))
          || q.next())
        throw (exceptions::msg()
               << "invalid host status during downtime");
    }

    // Check services.
    {
      std::ostringstream query;
      query << "SELECT COUNT(*)"
            << "  FROM rt_services"
            << "  WHERE scheduled_downtime_depth=0";
      QSqlQuery q(*db.centreon_db());
      if (!q.exec(query.str().c_str()))
        throw (exceptions::msg()
               << "cannot get service status from DB: "
               << qPrintable(q.lastError().text()));

      if (!q.next()
          || (q.value(0).toUInt() != (10 * 5 - 1))
          || q.next())
        throw (exceptions::msg()
               << "invalid service status during downtime");
    }

    // Delete downtimes.
    broker_commander.execute(
      "EXECUTE;84;downtimestosql-nodeevents;DEL_HOST_DOWNTIME;2");
    broker_commander.execute(
      "EXECUTE;84;downtimestosql-nodeevents;DEL_SVC_DOWNTIME;4");
    broker_commander.execute(
      "EXECUTE;84;downtimestosql-nodeevents;DEL_SVC_DOWNTIME;3");
    broker_commander.execute(
      "EXECUTE;84;downtimestosql-nodeevents;DEL_HOST_DOWNTIME;1");

    // Run a while.
    sleep_for(10);

    // Update time.
    time_t t2(now);
    now = time(NULL);

    // Check for deletion.
    {
      std::ostringstream query;
      query << "SELECT internal_id, actual_end_time, cancelled, deletion_time"
            << "  FROM rt_downtimes"
            << "  ORDER BY internal_id";
      QSqlQuery q(*db.centreon_db());
      if (!q.exec(query.str().c_str()))
        throw (exceptions::msg()
               << "cannot get deletion_time of downtimes: "
               << q.lastError().text().toStdString().c_str());
      if (// Host downtime #1.
          !q.next()
          || (q.value(0).toUInt() != 1)
          || (static_cast<time_t>(q.value(1).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(1).toLongLong()) > now)
          || !q.value(2).toUInt()
          || (static_cast<time_t>(q.value(3).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          // Host downtime #2.
          || !q.next()
          || (q.value(0).toUInt() != 2)
          || (static_cast<time_t>(q.value(1).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(1).toLongLong()) > now)
          || !q.value(2).toUInt()
          || (static_cast<time_t>(q.value(3).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          // Service downtime #1.
          || !q.next()
          || (q.value(0).toUInt() != 3)
          || (static_cast<time_t>(q.value(1).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(1).toLongLong()) > now)
          || !q.value(2).toUInt()
          || (static_cast<time_t>(q.value(3).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          // Service downtime #2.
          || !q.next()
          || (q.value(0).toUInt() != 4)
          || (static_cast<time_t>(q.value(1).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(1).toLongLong()) > now)
          || !q.value(2).toUInt()
          || (static_cast<time_t>(q.value(3).toLongLong()) < t2)
          || (static_cast<time_t>(q.value(3).toLongLong()) > now)
          // EOF
          || q.next())
        throw (exceptions::msg()
               << "invalid actual_end_time or deletion_time in DB");
    }

    // Success.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
    db.set_remove_db_on_close(false);
  }
  catch (...) {
    std::cerr << "unknown exception" << std::endl;
    db.set_remove_db_on_close(false);
  }

  // Cleanup.
  daemon.stop();
  config_remove(engine_config_path.c_str());
  free_hosts(hosts);
  free_services(services);

  return (retval);
}
