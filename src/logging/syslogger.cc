/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <syslog.h>
#include <stdlib.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exception/basic.hh"
#include "com/centreon/logging/syslogger.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::logging;

/**
 *  Default constructor.
 *
 *  @param[in] id        The id prepended to every message.
 *  @param[in] facility  This is the syslog facility.
 */
syslogger::syslogger(std::string const& id, int facility) {
  openlog(id.c_str(), 0, facility);
    throw (basic_error() << "failed to open syslogger \""
           << id << "\"");
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
syslogger::syslogger(syslogger const& right)
  : backend(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
syslogger::~syslogger() throw () {
  locker lock(&_mtx);
  closelog();
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
syslogger& syslogger::operator=(syslogger const& right) {
  return (_internal_copy(right));
}

/**
 *  Do nothing.
 */
void syslogger::flush() throw () {

}

/**
 *  Write message into the syslog.
 *
 *  @param[in] msg   The message to write.
 *  @param[in] size  The message's size.
 */
void syslogger::log(char const* msg, unsigned int size) throw () {
  (void)size;

  locker lock(&_mtx);
  syslog(LOG_ERR, "%s", msg);
}

/**
 *  Internal copy.
 *
 *  @param[in] right The object to copy.
 *
 *  @return This object.
 */
syslogger& syslogger::_internal_copy(syslogger const& right) {
  (void)right;
  assert(!"impossible to copy logging::syslogger");
  abort();
  return (*this);
}
