/*
** Copyright 2011-2012 Merethis
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
#include <iomanip>
#include <sstream>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/io/exceptions/shutdown.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/rrd/cached.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"
#include "com/centreon/broker/rrd/lib.hh"
#include "com/centreon/broker/rrd/output.hh"
#include "com/centreon/broker/storage/metric.hh"
#include "com/centreon/broker/storage/perfdata.hh"
#include "com/centreon/broker/storage/rebuild.hh"
#include "com/centreon/broker/storage/status.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Standard constructor.
 *
 *  @param[in] metrics_path Path in which metrics RRD files should be
 *                          written.
 *  @param[in] status_path  Path in which status RRD files should be
 *                          written.
 */
output::output(QString const& metrics_path, QString const& status_path)
  : _backend(new lib),
    _metrics_path(metrics_path),
    _process_out(true),
    _status_path(status_path) {}

/**
 *  Local socket constructor.
 *
 *  @param[in] metrics_path See standard constructor.
 *  @param[in] status_path  See standard constructor.
 *  @param[in] local        Local socket connection parameters.
 */
output::output(
          QString const& metrics_path,
          QString const& status_path,
          QString const& local)
  : _metrics_path(metrics_path),
    _process_out(true),
    _status_path(status_path) {
#if QT_VERSION >= 0x040400
  std::auto_ptr<cached> rrdcached(new cached);
  rrdcached->connect_local(local);
  _backend.reset(rrdcached.release());
#else
  throw (broker::exceptions::msg()
         << "RRD: local connection is not supported on Qt "
         << QT_VERSION_STR);
#endif // Qt version
}

/**
 *  Network socket constructor.
 *
 *  @param[in] metrics_path See standard constructor.
 *  @param[in] status_path  See standard constructor.
 *  @param[in] port         rrdcached listening port.
 */
output::output(QString const& metrics_path,
               QString const& status_path,
               unsigned short port)
  : _metrics_path(metrics_path),
    _process_out(true),
    _status_path(status_path) {
  std::auto_ptr<cached> rrdcached(new cached);
  rrdcached->connect_remote("localhost", port);
  _backend.reset(rrdcached.release());
}

/**
 *  Destructor.
 */
output::~output() {}

/**
 *  Set if output should be processed or not.
 *
 *  @param[in] in  Unused.
 *  @param[in] out Set to true to process output events.
 */
void output::process(bool in, bool out) {
  (void)in;
  _process_out = out;
  return ;
}

/**
 *  Read data.
 *
 *  @param[out] d Cleared.
 */
void output::read(misc::shared_ptr<io::data>& d) {
  d.clear();
  throw (broker::exceptions::msg()
           << "RRD: attempt to read data from an output endpoint");
  return ;
}

/**
 *  Write an event.
 *
 *  @param[in] d Data to write.
 */
void output::write(misc::shared_ptr<io::data> const& d) {
  // Check that data exists and should be processed.
  if (!_process_out)
    throw (io::exceptions::shutdown(true, true)
             << "RRD output is shutdown");
  if (d.isNull())
    return ;

  if (d->type() == "com::centreon::broker::storage::metric") {
    // Debug message.
    misc::shared_ptr<storage::metric>
      e(d.staticCast<storage::metric>());
    logging::debug(logging::medium) << "RRD: new data for metric "
      << e->metric_id << " (time " << e->ctime << ")";

    // Metric path.
    QString metric_path;
    {
      std::ostringstream oss;
      oss << _metrics_path.toStdString()
          << "/" << e->metric_id << ".rrd";
      metric_path = oss.str().c_str();
    }

    // Check that metric is not being rebuild.
    rebuild_cache::iterator it(_metrics_rebuild.find(metric_path));
    if (it == _metrics_rebuild.end()) {
      // Write metrics RRD.
      try {
        _backend->open(metric_path, e->name);
      }
      catch (exceptions::open const& b) {
        _backend->open(
          metric_path,
          e->name,
          e->rrd_len / (e->interval ? e->interval : 60) + 1,
          0,
          e->interval,
          e->value_type);
      }
      std::ostringstream oss;
      if (e->value_type != storage::perfdata::gauge)
        oss << static_cast<long long>(e->value);
      else
        oss << std::fixed << e->value;
      try {
        _backend->update(e->ctime, oss.str().c_str());
      }
      catch (exceptions::update const& b) {
        logging::error(logging::low) << b.what() << " (ignored)";
      }
    }
    else
      // Cache value.
      it->push_back(d);
  }
  else if (d->type() == "com::centreon::broker::storage::status") {
    // Debug message.
    misc::shared_ptr<storage::status>
      e(d.staticCast<storage::status>());
    logging::debug(logging::medium) << "RRD: new status data for index"
      << e->index_id << " (" << e->state << ")";

    // Status path.
    QString status_path;
    {
      std::ostringstream oss;
      oss << _status_path.toStdString()
          << "/" << e->index_id << ".rrd";
      status_path = oss.str().c_str();
    }

    // Check that status is not begin rebuild.
    rebuild_cache::iterator it(_status_rebuild.find(status_path));
    if (it != _status_rebuild.end()) {
      // Write status RRD.
      try {
        _backend->open(status_path, "status");
      }
      catch (exceptions::open const& b) {
        _backend->open(
          status_path,
          "status",
          e->rrd_len / (e->interval ? e->interval : 60),
          0,
          e->interval);
      }
      std::ostringstream oss;
      if (e->state == 0)
        oss << 100;
      else if (e->state == 1)
        oss << 75;
      else if (e->state == 2)
        oss << 0;
      try {
        _backend->update(e->ctime, oss.str().c_str());
      }
      catch (exceptions::update const& b) {
        logging::error(logging::medium) << b.what() << " (ignored)";
      }
    }
    else
      // Cache value.
      it->push_back(d);
  }
  else if (d->type() == "com::centreon::broker::storage::rebuild") {
    // Debug message.
    misc::shared_ptr<storage::rebuild>
      e(d.staticCast<storage::rebuild>());
    logging::debug(logging::medium) << "RRD: rebuild request for "
      << (e->is_index ? "index " : "metric ") << e->id
      << (e->end ? "(end)" : "(start)");

    // Generate path.
    QString path;
    {
      std::ostringstream oss;
      oss << (e->is_index ? _status_path : _metrics_path).toStdString()
          << "/" << e->id << ".rrd";
      path = oss.str().c_str();
    }

    // Rebuild is starting.
    if (!e->end) {
      if (e->is_index)
        _status_rebuild[path];
      else
        _metrics_rebuild[path];
    }
    // Rebuild is ending.
    else {
      // Find cache.
      std::list<misc::shared_ptr<io::data> > l;
      {
        rebuild_cache::iterator it;
        if (e->is_index) {
          it = _status_rebuild.find(path);
          if (it != _status_rebuild.end()) {
            l = *it;
            _status_rebuild.erase(it);
          }
        }
        else {
          it = _metrics_rebuild.find(path);
          if (it != _metrics_rebuild.end()) {
            l = *it;
            _metrics_rebuild.erase(it);
          }
        }
      }

      // Resend cache data.
      while (!l.empty()) {
        write(l.front());
        l.pop_front();
      }
    }
  }

  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] o Object to copy.
 */
output::output(output const& o) : io::stream(o) {
  assert(!"RRD output is not copyable");
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] o Object to copy.
 *
 *  @return This object.
 */
output& output::operator=(output const& o) {
  (void)o;
  assert(!"RRD output is not copyable");
  abort();
  return (*this);
}
