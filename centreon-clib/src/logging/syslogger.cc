/*
** Copyright 2011-2013 Merethis
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
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::logging;

/**
 *  Constructor.
 *
 *  @param[in] id              The id prepended to every message.
 *  @param[in] facility        This is the syslog facility.
 *  @param[in] is_sync         Enable synchronization.
 *  @param[in] show_pid        Enable show pid.
 *  @param[in] show_timestamp  Enable show timestamp.
 *  @param[in] show_thread_id  Enable show thread id.
 */
syslogger::syslogger(
             std::string const& id,
             int facility,
             bool is_sync,
             bool show_pid,
             time_precision show_timestamp,
             bool show_thread_id)
  : backend(is_sync, show_pid, show_timestamp, show_thread_id),
    _facility(facility),
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
  concurrency::locker lock(&_lock);
  closelog();
}

/**
 *  Write message into the syslog.
 *  @remark This method is thread safe.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 *  @param[in] msg      The message to write.
 *  @param[in] size     The message's size.
 */
void syslogger::log(
                  unsigned long long types,
                  unsigned int verbose,
                  char const* msg,
                  unsigned int size) throw () {
  (void)types;
  (void)verbose;
  (void)size;

  misc::stringifier header;
  _build_header(header);

  concurrency::locker lock(&_lock);
  syslog(LOG_ERR, "%s%s", header.data(), msg);
}

/**
 *  Open syslog.
 */
void syslogger::open() {
  concurrency::locker lock(&_lock);
  openlog(_id.c_str(), 0, _facility);
}

/**
 *  Close and open syslog.
 */
void syslogger::reopen() {
  concurrency::locker lock(&_lock);
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
