/*
** Copyright 2014 Merethis
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

#include <cassert>
#include <cstdlib>
#include <sstream>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include "com/centreon/broker/bam/ba_status.hh"
#include "com/centreon/broker/bam/ba_event.hh"
#include "com/centreon/broker/bam/ba_duration_event.hh"
#include "com/centreon/broker/bam/bool_status.hh"
#include "com/centreon/broker/bam/configuration/reader.hh"
#include "com/centreon/broker/bam/configuration/state.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/bam/kpi_status.hh"
#include "com/centreon/broker/bam/kpi_event.hh"
#include "com/centreon/broker/bam/meta_service_status.hh"
#include "com/centreon/broker/bam/reporting_stream.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/exceptions/shutdown.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/misc/global_lock.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "com/centreon/broker/storage/internal.hh"
#include "com/centreon/broker/storage/metric.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] db_type                 BAM DB type.
 *  @param[in] db_host                 BAM DB host.
 *  @param[in] db_port                 BAM DB port.
 *  @param[in] db_user                 BAM DB user.
 *  @param[in] db_password             BAM DB password.
 *  @param[in] db_name                 BAM DB name.
 *  @param[in] queries_per_transaction Queries per transaction.
 *  @param[in] check_replication       true to check replication status.
 */
reporting_stream::reporting_stream(
          QString const& db_type,
          QString const& db_host,
          unsigned short db_port,
          QString const& db_user,
          QString const& db_password,
          QString const& db_name,
          unsigned int queries_per_transaction,
          bool check_replication) {
  // Process events.
  _process_out = true;

  // Queries per transaction.
  _queries_per_transaction = ((queries_per_transaction >= 2)
                              ? queries_per_transaction
                              : 1);
  _transaction_queries = 0;

  // BAM connection ID.
  QString bam_id;
  bam_id.setNum((qulonglong)this, 16);

  // Add database connection.
  _db.reset(
        new QSqlDatabase(QSqlDatabase::addDatabase(
                                         db_type,
                                         bam_id)));

  // Set DB parameters.
  _db->setHostName(db_host);
  _db->setPort(db_port);
  _db->setUserName(db_user);
  _db->setPassword(db_password);
  _db->setDatabaseName(db_name);

  try {
    {
      QMutexLocker lock(&misc::global_lock);
      // Open database.
      if (!_db->open()) {
        QString error(_db->lastError().text());
        _clear_qsql();
        throw (broker::exceptions::msg()
               << "BAM: could not connect to reporting database '"
               << db_name << "' on host '" << db_host
               << ":" << db_port << "': " << error);
      }
    }

    // Check that replication is OK.
    if (check_replication)
      _check_replication();
    else
      logging::debug(logging::medium)
        << "BAM: NOT checking replication status of reporting database '"
        << _db->databaseName() << "' on host '" << _db->hostName()
        << ":" << _db->port() << "'";

    // Prepare queries.
    _prepare();

    // Initial transaction.
    if (_queries_per_transaction > 1)
      _db->transaction();
  }
  catch (...) {
    {
      QMutexLocker lock(&misc::global_lock);
      // Delete statements.
      _clear_qsql();
    }

    // Remove this connection.
    QSqlDatabase::removeDatabase(bam_id);

    throw ;
  }
}

/**
 *  Destructor.
 */
reporting_stream::~reporting_stream() {
  // Connection ID.
  QString bam_id;
  bam_id.setNum((qulonglong)this, 16);

  {
    QMutexLocker lock(&misc::global_lock);
    // Reset statements.
    _clear_qsql();
  }

  // Remove this connection.
  QSqlDatabase::removeDatabase(bam_id);
}

/**
 *  Enable or disable output event processing.
 *
 *  @param[in] in  Unused.
 *  @param[in] out Set to true to enable output event processing.
 */
void reporting_stream::process(bool in, bool out) {
  _process_out = in || !out; // Only for immediate shutdown.
  return ;
}

/**
 *  Read from the datbase.
 *  Get the next available bam event.
 *
 *  @param[out] d Cleared.
 *  @param[out] d The next available bam event.
 */
void reporting_stream::read(misc::shared_ptr<io::data>& d) {
  d.clear();
  throw (exceptions::msg()
         << "BAM: attempt to read from a BAM reporting stream (not supported)");
  return ;
}
/**
 *  Get endpoint statistics.
 *
 *  @param[out] tree Output tree.
 */
void reporting_stream::statistics(io::properties& tree) const {
  QMutexLocker lock(&_statusm);
  if (!_status.empty()) {
    io::property& p(tree["status"]);
    p.set_perfdata(_status);
    p.set_graphable(false);
  }
  return ;
}

/**
 *  Write an event.
 *
 *  @param[in] data Event pointer.
 *
 *  @return Number of events acknowledged.
 */
unsigned int reporting_stream::write(misc::shared_ptr<io::data> const& data) {
  // Check that processing is enabled.
  if (!_process_out)
    throw (io::exceptions::shutdown(true, true)
           << "BAM reporting stream is shutdown");

  if (!data.isNull()) {
    if (data->type()
             == io::events::data_type<io::events::bam, bam::de_kpi_event>::value) {
      logging::debug(logging::low)
        << "BAM: processing KPI event";
      _process_kpi_event(data);
    }
    else if (data->type()
             == io::events::data_type<io::events::bam, bam::de_ba_event>::value) {
      logging::debug(logging::low)
        << "BAM: processing BA event";
      _process_ba_event(data);
    }
  }
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] other Unused.
 */
reporting_stream::reporting_stream(reporting_stream const& other)
  : io::stream(other) {
  assert(!"BAM reporting stream is not copyable");
  abort();
}

/**
 *  Assignment operator.
 *
 *  @param[in] other Unused.
 *
 *  @return This object.
 */
reporting_stream& reporting_stream::operator=(reporting_stream const& other) {
  (void)other;
  assert(!"BAM reporting stream is not copyable");
  abort();
  return (*this);
}

/**
 *  Check that replication is OK.
 */
void reporting_stream::_check_replication() {
  // Check that replication is OK.
  logging::debug(logging::medium)
    << "BAM: checking replication status of reporting database '"
    << _db->databaseName() << "' on host '" << _db->hostName()
    << ":" << _db->port() << "'";
  QSqlQuery q(*_db);
  if (!q.exec("SHOW SLAVE STATUS"))
    logging::info(logging::medium)
      << "BAM: could not check replication status of reporting database '"
      << _db->databaseName() << "' on host '" << _db->hostName()
      << ":" << _db->port() << "': " << q.lastError().text();
  else {
    if (!q.next())
      logging::info(logging::medium)
        << "BAM: reorting database '" << _db->databaseName()
        << "' on host '" << _db->hostName() << ":" << _db->port()
        << "' is not under replication";
    else {
      QSqlRecord record(q.record());
      unsigned int i(0);
      for (QString field(record.fieldName(i));
           !field.isEmpty();
           field = record.fieldName(++i))
        if (((field == "Slave_IO_Running")
             && (q.value(i).toString() != "Yes"))
            || ((field == "Slave_SQL_Running")
                && (q.value(i).toString() != "Yes"))
            || ((field == "Seconds_Behind_Master")
                && (q.value(i).toInt() != 0)))
          throw (broker::exceptions::msg()
                 << "BAM: replication of reporting database '"
                 << _db->databaseName() << "' on host '"
                 << _db->hostName() << ":" << _db->port()
                 << "' is not complete: " << field
                 << "=" << q.value(i).toString());
      logging::info(logging::medium)
        << "storage: replication of reporting database '"
        << _db->databaseName() << "' on host '" << _db->hostName()
        << ":" << _db->port() << "' is complete, connection granted";
    }
  }
  return ;
}

/**
 *  Clear QtSql objects.
 */
void reporting_stream::_clear_qsql() {
  _ba_event_insert.reset();
  _ba_event_update.reset();
  _ba_duration_event_insert.reset();
  _kpi_event_insert.reset();
  _kpi_event_update.reset();
  _kpi_event_link.reset();
  _db.reset();
  return ;
}

/**
 *  Prepare queries.
 */
void reporting_stream::_prepare() {
  // BA event insertion.
  {
    QString query;
    query = "INSERT INTO ba_events (ba_id, start_time, status, in_downtime)"
            "  VALUES (:ba_id, :start_time, :status, :in_downtime)";
    _ba_event_insert.reset(new QSqlQuery(*_db));
    if (!_ba_event_insert->prepare(query))
      throw (exceptions::msg()
             << "BAM: could not prepare BA event insertion query: "
             << _ba_event_insert->lastError().text());
  }

  // BA event update.
  {
    QString query;
    query = "UPDATE ba_events"
            "  SET end_time=:end_time"
            "  WHERE ba_id=:ba_id AND start_time=:start_time";
    _ba_event_update.reset(new QSqlQuery(*_db));
    if (!_ba_event_update->prepare(query))
      throw (exceptions::msg()
             << "BAM: could not prepare BA event update query: "
             << _ba_event_update->lastError().text());
  }

  // BA duration event insert.
  {
    QString query;
    query = "INSERT INTO ba_events_durations ("
             "                ba_event_id, start_time, "
             "                end_time, duration, sla_duration, timeperiod_id, "
             "                timeperiod_is_default)"
             "  SELECT b.ba_event_id, :start_time, :end_time, :duration, "
             "         :sla_duration, :timeperiod_id, :timeperiod_is_default"
             "  FROM ba_events as b"
             "  WHERE b.ba_id=:ba_id AND b.start_time=:real_start_time";
    _ba_duration_event_insert.reset(new QSqlQuery(*_db));
    if (_ba_duration_event_insert->prepare(query))
      throw (exceptions::msg()
             << "BAM: could not prepare BA duration event insert query: "
             << _ba_duration_event_insert->lastError().text());
  }

  // KPI event insertion.
  {
    QString query;
    query = "INSERT INTO kpi_events (kpi_id, start_time, status, "
            "            in_downtime, impact_level, first_output, "
            "            first_perfdata)"
            "  VALUES (:kpi_id, :start_time, :status, :in_downtime, "
            "         :impact_level, :output, :perfdata)";
    _kpi_event_insert.reset(new QSqlQuery(*_db));
    if (!_kpi_event_insert->prepare(query))
      throw (exceptions::msg()
             << "BAM: could not prepare KPI event insertion query: "
             << _kpi_event_insert->lastError().text());
  }

  // KPI event update.
  {
    QString query;
    query = "UPDATE kpi_events"
            "  SET end_time=:end_time"
            "  WHERE kpi_id=:kpi_id AND start_time=:start_time";
    _kpi_event_update.reset(new QSqlQuery(*_db));
    if (!_kpi_event_update->prepare(query))
      throw (exceptions::msg()
             << "BAM: could not prepare KPI event update query: "
             << _kpi_event_update->lastError().text());
  }

  // KPI event link to BA event.
  {
    QString query;
    query = "INSERT INTO relations_ba_kpi_events (ba_event_id, kpi_event_id)"
            "  SELECT be.ba_event_id, ke.kpi_event_id"
            "    FROM kpi_events AS ke"
            "    INNER JOIN ba_events AS be"
            "    ON ((ke.start_time >= be.start_time)"
            "       AND (be.end_time IS NULL OR ke.start_time < be.end_time))"
            "    WHERE ke.kpi_id=:kpi_id AND ke.start_time=:start_time";
    _kpi_event_link.reset(new QSqlQuery(*_db));
    if (!_kpi_event_link->prepare(query))
      throw (exceptions::msg()
             << "BAM: could not prepare link query of BA and KPI events: "
             << _kpi_event_link->lastError().text());
  }

  return ;
}

/**
 *  Process a ba event and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_ba_event(misc::shared_ptr<io::data> const& e) {
  bam::ba_event const& be(*static_cast<bam::ba_event const*>(e.data()));
  if ((be.end_time != 0) && (be.end_time != (time_t)-1)) {
    _ba_event_update->bindValue(":ba_id", be.ba_id);
    _ba_event_update->bindValue(
      ":start_time",
      static_cast<qlonglong>(be.start_time.get_time_t()));
    _ba_event_update->bindValue(
      ":end_time",
      static_cast<qlonglong>(be.end_time.get_time_t()));
    if (!_ba_event_update->exec())
      throw (exceptions::msg() << "BAM: could not close event of BA "
             << be.ba_id << " starting at " << be.start_time
             << " and ending at " << be.end_time);
  }
  else {
    _ba_event_insert->bindValue(":ba_id", be.ba_id);
    _ba_event_insert->bindValue(
      ":start_time",
      static_cast<qlonglong>(be.start_time.get_time_t()));
    _ba_event_insert->bindValue(":status", be.status);
    _ba_event_insert->bindValue(":in_downtime", be.in_downtime);
    if (!_ba_event_insert->exec())
      throw (exceptions::msg() << "BAM: could not insert event of BA "
             << be.ba_id << " starting at " << be.start_time);
  }
  return ;
}

/**
 *  Process a ba duration event and write it to the db.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_ba_duration_event(misc::shared_ptr<io::data> const& e) {
  bam::ba_duration_event const&
    bde(*static_cast<bam::ba_duration_event const*>(e.data()));
  _ba_duration_event_insert->bindValue(":ba_id", bde.ba_id);
  _ba_duration_event_insert->bindValue(
    ":real_start_time",
    static_cast<qlonglong>(bde.real_start_time.get_time_t()));
  _ba_duration_event_insert->bindValue(
    ":end_time",
    static_cast<qlonglong>(bde.end_time.get_time_t()));
  _ba_duration_event_insert->bindValue(
    ":start_time",
    static_cast<qlonglong>(bde.start_time.get_time_t()));
  _ba_duration_event_insert->bindValue(":duration", bde.duration);
  _ba_duration_event_insert->bindValue(":sla_duration", bde.sla_duration);
  _ba_duration_event_insert->bindValue(":timeperiod_id", bde.timeperiod_id);
  _ba_duration_event_insert->bindValue(
    ":timeperiod_is_default",
    bde.timeperiod_is_default);
  if (_ba_duration_event_insert->exec())
    throw (exceptions::msg() << "BAM: could not insert duration event of BA "
           << bde.ba_id << " starting at " << bde.start_time);
  return ;
}

/**
 *  Process a kpi event and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_kpi_event(misc::shared_ptr<io::data> const& e) {
  bam::kpi_event const& ke(*static_cast<bam::kpi_event const*>(e.data()));
  if ((ke.end_time != 0) && (ke.end_time != (time_t)-1)) {
    _kpi_event_update->bindValue(":kpi_id", ke.kpi_id);
    _kpi_event_update->bindValue(
      ":start_time",
      static_cast<qlonglong>(ke.start_time.get_time_t()));
    _kpi_event_update->bindValue(
      ":end_time",
      static_cast<qlonglong>(ke.end_time.get_time_t()));
    if (!_kpi_event_update->exec())
      throw (exceptions::msg() << "BAM: could not close event of KPI "
             << ke.kpi_id << " starting at " << ke.start_time
             << " and ending at " << ke.end_time << ": "
             << _kpi_event_update->lastError().text());

    _kpi_event_link->bindValue(
      ":start_time",
      static_cast<qlonglong>(ke.start_time.get_time_t()));
    _kpi_event_link->bindValue(":kpi_id", ke.kpi_id);
    if (!_kpi_event_link->exec())
      throw (exceptions::msg()
             << "BAM: could not create link from event of KPI "
             << ke.kpi_id << " starting at " << ke.start_time
             << " to its associated BA event: "
             << _kpi_event_link->lastError().text());
  }
  else {
    _kpi_event_insert->bindValue(":kpi_id", ke.kpi_id);
    _kpi_event_insert->bindValue(
      ":start_time",
      static_cast<qlonglong>(ke.start_time.get_time_t()));
    _kpi_event_insert->bindValue(":status", ke.status);
    _kpi_event_insert->bindValue(":in_downtime", ke.in_downtime);
    _kpi_event_insert->bindValue(":impact_level", ke.impact_level);
    _kpi_event_insert->bindValue(":output", ke.output.c_str());
    _kpi_event_insert->bindValue(":perfdata", ke.perfdata.c_str());
    if (!_kpi_event_insert->exec())
      throw (exceptions::msg() << "BAM: could not insert event of KPI "
             << ke.kpi_id << " starting at " << ke.start_time << ": "
             << _kpi_event_insert->lastError().text());
  }
  return ;
}

/**
 *  Update status of endpoint.
 *
 *  @param[in] status New status.
 */
void reporting_stream::_update_status(std::string const& status) {

}
