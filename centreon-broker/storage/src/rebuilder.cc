/*
** Copyright 2012-2014 Merethis
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
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <QVariant>
#include <sstream>
#include "com/centreon/broker/database.hh"
#include "com/centreon/broker/database_query.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/misc/shared_ptr.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/storage/metric.hh"
#include "com/centreon/broker/storage/rebuild.hh"
#include "com/centreon/broker/storage/status.hh"
#include "com/centreon/broker/storage/rebuilder.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::storage;

// Local types.
struct index_info {
  unsigned int index_id;
  unsigned int host_id;
  unsigned int service_id;
  unsigned int rrd_retention;
};

struct metric_info {
  unsigned int metric_id;
  QString metric_name;
  short metric_type;
};

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] db_type                 DB type.
 *  @param[in] db_host                 DB host.
 *  @param[in] db_port                 DB port.
 *  @param[in] db_user                 DB user.
 *  @param[in] db_password             DB password.
 *  @param[in] db_name                 DB name.
 *  @param[in] rebuild_check_interval  How often the rebuild thread will
 *                                     check for rebuild.
 *  @param[in] interval_length         Base time unit.
 *  @param[in] rrd_length              Length of RRD files.
 */
rebuilder::rebuilder(
             std::string const& db_type,
             std::string const& db_host,
             unsigned short db_port,
             std::string const& db_user,
             std::string const& db_password,
             std::string const& db_name,
             unsigned int rebuild_check_interval,
             time_t interval_length,
             unsigned int rrd_length)
  : _db_type(db_type),
    _db_host(db_host),
    _db_port(db_port),
    _db_user(db_user),
    _db_password(db_password),
    _db_name(db_name),
    _interval(rebuild_check_interval),
    _interval_length(interval_length),
    _rrd_len(rrd_length),
    _should_exit(false) {}

/**
 *  Destructor.
 */
rebuilder::~rebuilder() throw () {}

/**
 *  Set the exit flag.
 */
void rebuilder::exit() throw () {
  _should_exit = true;
  return ;
}

/**
 *  Get the rebuild check interval.
 *
 *  @return Rebuild check interval in seconds.
 */
unsigned int rebuilder::get_interval() const throw () {
  return (_interval);
}

/**
 *  Get the interval length in seconds.
 *
 *  @return Interval length in seconds.
 */
time_t rebuilder::get_interval_length() const throw () {
  return (_interval_length);
}

/**
 *  Get the RRD length in seconds.
 *
 *  @return RRD length in seconds.
 */
unsigned int rebuilder::get_rrd_length() const throw () {
  return (_rrd_len);
}

/**
 *  Thread entry point.
 */
void rebuilder::run() {
  while (!_should_exit && _interval) {
    try {
      // Open DB.
      std::auto_ptr<database> db;
      try {
        db.reset(new database(
                       _db_type,
                       _db_host,
                       _db_port,
                       _db_user,
                       _db_password,
                       _db_name));
      }
      catch (std::exception const& e) {
        throw (broker::exceptions::msg() << "storage: rebuilder: could "
               "not connect to Centreon Storage database: "
               << e.what());
      }

      // Fetch index to rebuild.
      std::list<index_info> index_to_rebuild;
      {
        database_query index_to_rebuild_query(*db);
        index_to_rebuild_query.run_query(
          "SELECT id, host_id, service_id, rrd_retention"
          " FROM index_data"
          " WHERE must_be_rebuild='1'",
          "storage: rebuilder: could not fetch index to rebuild");
        while (!_should_exit && index_to_rebuild_query.next()) {
          index_info info;
          info.index_id = index_to_rebuild_query.value(0).toUInt();
          info.host_id = index_to_rebuild_query.value(1).toUInt();
          info.service_id = index_to_rebuild_query.value(2).toUInt();
          info.rrd_retention
            = (index_to_rebuild_query.value(3).isNull()
               ? 0
               : index_to_rebuild_query.value(3).toUInt());
          if (!info.rrd_retention)
            info.rrd_retention = _rrd_len;
          index_to_rebuild.push_back(info);
        }
      }

      // Browse list of index to rebuild.
      while (!_should_exit && !index_to_rebuild.empty()) {
        // Get check interval of host/service.
        unsigned int index_id;
        unsigned int check_interval(0);
        unsigned int rrd_len;
        {
          index_info info(index_to_rebuild.front());
          index_id = info.index_id;
          rrd_len = info.rrd_retention;
          index_to_rebuild.pop_front();

          std::ostringstream oss;
          if (!info.service_id)
            oss << "SELECT check_interval"
                << " FROM hosts"
                << " WHERE host_id=" << info.host_id;
          else
            oss << "SELECT check_interval"
                << " FROM services"
                << " WHERE host_id=" << info.host_id
                << "  AND service_id=" << info.service_id;
          database_query query(*db);
          query.run_query(oss.str());
          if (query.next())
            check_interval = query.value(0).toUInt();
          if (!check_interval)
            check_interval = 5;
        }
        logging::info(logging::medium) << "storage: rebuilder: index "
          << index_id << " (interval " << check_interval
          << ") will be rebuild";

        // Set index as being rebuilt.
        _set_index_rebuild(*db, index_id, 2);

        try {
          // Fetch metrics to rebuild.
          std::list<metric_info> metrics_to_rebuild;
          {
            std::ostringstream oss;
            oss << "SELECT metric_id, metric_name, data_source_type"
                << " FROM metrics"
                << " WHERE index_id=" << index_id;
            database_query metrics_to_rebuild_query(*db);
            try { metrics_to_rebuild_query.run_query(oss.str()); }
            catch (std::exception const& e) {
              throw (exceptions::msg()
                     << "storage: rebuilder: could not fetch "
                     << "metrics of index " << index_id);
            }
            while (!_should_exit && metrics_to_rebuild_query.next()) {
              metric_info info;
              info.metric_id
                = metrics_to_rebuild_query.value(0).toUInt();
              info.metric_name
                = metrics_to_rebuild_query.value(1).toString();
              info.metric_type
                = metrics_to_rebuild_query.value(2).toInt();
              metrics_to_rebuild.push_back(info);
            }
          }

          // Browse metrics to rebuild.
          while (!_should_exit && !metrics_to_rebuild.empty()) {
            metric_info info(metrics_to_rebuild.front());
            metrics_to_rebuild.pop_front();
            _rebuild_metric(
              *db,
              info.metric_id,
              info.metric_name,
              info.metric_type,
              check_interval * _interval_length,
              rrd_len);
          }

          // Rebuild status.
          _rebuild_status(
            *db,
            index_id,
            check_interval * _interval_length);
        }
        catch (...) {
          // Set index as to-be-rebuilt.
          _set_index_rebuild(*db, index_id, 1);

          // Rethrow exception.
          throw ;
        }

        // Set index as rebuilt or to-be-rebuild
        // if we were interrupted.
        _set_index_rebuild(*db, index_id, (_should_exit ? 1 : 0));
      }
    }
    catch (std::exception const& e) {
      logging::error(logging::high) << e.what();
    }
    catch (...) {
      logging::error(logging::high)
        << "storage: rebuilder: unknown error";
    }

    // Sleep a while.
    time_t target(time(NULL) + _interval);
    while (!_should_exit && (target > time(NULL)))
      sleep(1);
  }
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
rebuilder::rebuilder(rebuilder const& other) : QThread() {
  (void)other;
  assert(!"rebuild threads are not copyable");
  abort();
}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
rebuilder& rebuilder::operator=(rebuilder const& other) {
  (void)other;
  assert(!"rebuild threads are not copyable");
  abort();
  return (*this);
}

/**
 *  Rebuild a metric.
 *
 *  @param[in] db           Database object.
 *  @param[in] metric_id    Metric ID.
 *  @param[in] metric_name  Metric name.
 *  @param[in] type         Metric type.
 *  @param[in] interval     Host/service check interval.
 *  @param[in] length       Metric RRD length in seconds.
 */
void rebuilder::_rebuild_metric(
                  database& db,
                  unsigned int metric_id,
                  QString const& metric_name,
                  short metric_type,
                  unsigned int interval,
                  unsigned int length) {
  // Log.
  logging::info(logging::low)
    << "storage: rebuilder: rebuilding metric " << metric_id
    << " (name " << metric_name << ", type " << metric_type
    << ", interval " << interval << ")";

  // Send rebuild start event.
  _send_rebuild_event(false, metric_id, false);

  try {
    // Get data.
    std::ostringstream oss;
    oss << "SELECT ctime, value"
        << " FROM data_bin"
        << " WHERE id_metric=" << metric_id
        << " ORDER BY ctime ASC";
    database_query data_bin_query(db);
    bool caught(false);
    try { data_bin_query.run_query(oss.str()); }
    catch (std::exception const& e) {
      caught = true;
      logging::error(logging::medium) << "storage: rebuilder: "
        << "cannot fetch data of metric " << metric_id << ": "
        << e.what();
    }
    if (!caught)
      while (!_should_exit && data_bin_query.next()) {
        misc::shared_ptr<storage::metric> entry(new storage::metric);
        entry->ctime = data_bin_query.value(0).toUInt();
        entry->interval = interval;
        entry->is_for_rebuild = true;
        entry->metric_id = metric_id;
        entry->name = metric_name;
        entry->rrd_len = length;
        entry->value_type = metric_type;
        entry->value = data_bin_query.value(1).toDouble();
        if (entry->value > FLT_MAX * 0.999)
          entry->value = INFINITY;
        else if (entry->value < FLT_MIN * 0.999)
          entry->value = -INFINITY;
        multiplexing::publisher().write(entry);
      }
  }
  catch (...) {
    // Send rebuild end event.
    _send_rebuild_event(true, metric_id, false);

    // Rethrow exception.
    throw ;
  }

  // Send rebuild end event.
  _send_rebuild_event(true, metric_id, false);

  return ;
}

/**
 *  Rebuild a status.
 *
 *  @param[in] db        Database object.
 *  @param[in] index_id  Index ID.
 *  @param[in] interval  Host/service check interval.
 */
void rebuilder::_rebuild_status(
                  database& db,
                  unsigned int index_id,
                  unsigned int interval) {
  // Log.
  logging::info(logging::low)
    << "storage: rebuilder: rebuilding status " << index_id
    << "(interval " << interval << ")";

  // Send rebuild start event.
  _send_rebuild_event(false, index_id, true);

  try {
    // Get data.
    std::ostringstream oss;
    oss << "SELECT d.ctime, d.status"
        << " FROM metrics AS m"
        << " JOIN data_bin AS d"
        << " ON m.metric_id=d.id_metric"
        << " WHERE m.index_id=" << index_id
        << " ORDER BY d.ctime ASC";
    database_query data_bin_query(db);
    bool caught(false);
    try { data_bin_query.run_query(oss.str()); }
    catch (std::exception const& e) {
      caught = true;
      logging::error(logging::medium) << "storage: rebuilder: "
        << "cannot fetch data of index " << index_id << ": "
        << e.what();
    }
    if (!caught)
      while (!_should_exit && data_bin_query.next()) {
        misc::shared_ptr<storage::status> entry(new storage::status);
        entry->ctime = data_bin_query.value(0).toUInt();
        entry->index_id = index_id;
        entry->interval = interval;
        entry->is_for_rebuild = true;
        entry->rrd_len = _rrd_len;
        entry->state = data_bin_query.value(1).toInt();
        multiplexing::publisher().write(entry);
      }
  }
  catch (...) {
    // Send rebuild end event.
    _send_rebuild_event(true, index_id, true);

    // Rethrow exception.
    throw ;
  }

  // Send rebuild end event.
  _send_rebuild_event(true, index_id, true);

  return ;
}

/**
 *  Send a rebuild event.
 *
 *  @param[in] end      false if rebuild is starting, true if it is ending.
 *  @param[in] id       Index or metric ID.
 *  @param[in] is_index true for an index ID, false for a metric ID.
 */
void rebuilder::_send_rebuild_event(
                  bool end,
                  unsigned int id,
                  bool is_index) {
  misc::shared_ptr<storage::rebuild> rb(new storage::rebuild);
  rb->end = end;
  rb->id = id;
  rb->is_index = is_index;
  multiplexing::publisher().write(rb);
  return ;
}

/**
 *  Set index rebuild flag.
 *
 *  @param[in] db        Database object.
 *  @param[in] index_id  Index to update.
 *  @param[in] state     Rebuild state (0, 1 or 2).
 */
void rebuilder::_set_index_rebuild(
                  database& db,
                  unsigned int index_id,
                  short state) {
  std::ostringstream oss;
  oss << "UPDATE index_data"
      << " SET must_be_rebuild=" << state + 1
      << " WHERE id=" << index_id;
  database_query update_index_query(db);
  try { update_index_query.run_query(oss.str()); }
  catch (std::exception const& e) {
    logging::error(logging::low)
      << "storage: rebuilder: cannot update state of index "
      << index_id << ": " << e.what();
  }
  return ;
}
