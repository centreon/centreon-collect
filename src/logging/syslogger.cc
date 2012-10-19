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

#include <cassert>
#include <cstdlib>
#include <syslog.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/syslogger.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::logging;

/**
 *  Default constructor.
 *
 *  @param[in] id        The id prepended to every message.
 *  @param[in] facility  This is the syslog facility.
 */
syslogger::syslogger(std::string const& id, int facility)
  : _facility(facility),
    _id(id) {
  open();
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
  close();
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
 *  Close syslog.
 */
void syslogger::close() throw () {
  locker lock(&_mtx);
  closelog();
}

/**
 *  Do nothing.
 */
void syslogger::flush() throw () {

}

/**
 *  Write message into the syslog.
 *  @remark This method is thread safe.
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
 *  Open syslog.
 */
void syslogger::open() {
  locker lock(&_mtx);
  openlog(_id.c_str(), 0, _facility);
}

/**
 *  Close and open syslog.
 */
void syslogger::reopen() {
  locker lock(&_mtx);
  closelog();
  openlog(_id.c_str(), 0, _facility);
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
