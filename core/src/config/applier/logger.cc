/*
** Copyright 2011-2012 Merethis
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

#include <assert.h>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include "com/centreon/broker/config/applier/logger.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/file.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/logging/manager.hh"
#include "com/centreon/broker/logging/syslogger.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::config::applier;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
logger::logger() {}

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] l Object to copy.
 */
logger::logger(logger const& l) {
  (void)l;
  assert(false);
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] l Object to copy.
 *
 *  @return This object.
 */
logger& logger::operator=(logger const& l) {
  (void)l;
  assert(false);
  abort();
  return (*this);
}

/**
 *  Create a backend object from its configuration.
 *
 *  @param[in] cfg Logging backend configuration.
 *
 *  @return New logging backend.
 */
QSharedPointer<logging::backend> logger::_new_backend(config::logger const& cfg) {
  QSharedPointer<logging::backend> back;
  switch (cfg.type()) {
   case config::logger::file:
    {
      if (cfg.name().isEmpty())
        throw (exceptions::msg()
                 << "log applier: attempt to log on an empty file");
      std::auto_ptr<logging::file> file(new logging::file(cfg.name()));
      back = QSharedPointer<logging::backend>(file.get());
      file.release();
    }
    break ;
   case config::logger::standard:
    {
      FILE* out;
      if ((cfg.name() == "stderr") || (cfg.name() == "cerr"))
        out = stderr;
      else if ((cfg.name() == "stdout") || (cfg.name() == "cout"))
        out = stdout;
      else
        throw (exceptions::msg() << "log applier: attempt to log on " \
                 "an undefined output object");
      back = QSharedPointer<logging::backend>(new logging::file(out));
    }
    break ;
   case config::logger::syslog:
    back = QSharedPointer<logging::backend>(
      new logging::syslogger(cfg.facility()));
    break ;
   default:
    throw (exceptions::msg() << "log applier: attempt to create a " \
             "logging object of unknown type");
  }

  return (back);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Destructor.
 */
logger::~logger() {
  logging::debug << logging::HIGH << "log applier: destruction";
}

/**
 *  Apply the configuration of a set of loggers.
 *
 *  @param[in] loggers List of loggers that should exist.
 */
void logger::apply(QList<config::logger> const& loggers) {
  // Log message.
  logging::config << logging::HIGH << "log applier: applying "
    << loggers.size() << " logging objects";

  // Find which loggers are already created,
  // which should be created
  // and which should be deleted.
  QList<config::logger> to_create;
  QMap<config::logger, QSharedPointer<logging::backend> > to_delete(_backends);
  QMap<config::logger, QSharedPointer<logging::backend> > to_keep;
  for (QList<config::logger>::const_iterator it = loggers.begin(),
         end = loggers.end();
       it != end;
       ++it) {
    QMap<config::logger, QSharedPointer<logging::backend> >::iterator backend(to_delete.find(*it));
    if (backend != to_delete.end()) {
      to_keep.insert(backend.key(), backend.value());
      to_delete.erase(backend);
    }
    else
      to_create.push_back(*it);
  }

  // Set the backends that will be kept.
  _backends = to_keep;

  // Remove loggers that do not exist anymore.
  for (QMap<config::logger, QSharedPointer<logging::backend> >::const_iterator it = to_delete.begin(),
         end = to_delete.end();
       it != end;
       ++it)
    logging::manager::instance().log_on(*it.value(), 0, logging::none);

  // Free some memory.
  to_delete.clear();
  to_keep.clear();

  // Create new backends.
  for (QList<config::logger>::const_iterator it = to_create.begin(),
         end = to_create.end();
       it != end;
       ++it) {
    logging::config << logging::medium
      << "log applier: creating new logger";
    QSharedPointer<logging::backend> backend(_new_backend(*it));
    _backends[*it] = backend;
    logging::manager::instance().log_on(
      *backend,
      it->types(),
      it->level());
  }

  return ;
}

/**
 *  Get the class instance.
 *
 *  @return logger instance.
 */
logger& logger::instance() {
  static logger gl_logger;
  return (gl_logger);
}
