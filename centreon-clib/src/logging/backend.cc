/*
** Copyright 2011-2012 Merethis
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

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif // _WIN32

#include <cstring>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/concurrency/thread.hh"
#include "com/centreon/logging/backend.hh"
#include "com/centreon/misc/stringifier.hh"
#include "com/centreon/timestamp.hh"

using namespace com::centreon::logging;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] is_sync         Enable synchronization.
 *  @param[in] show_pid        Enable show pid.
 *  @param[in] show_timestamp  Enable show timestamp.
 *  @param[in] show_thread_id  Enable show thread id.
 */
backend::backend(
           bool is_sync,
           bool show_pid,
           time_precision show_timestamp,
           bool show_thread_id)
  : _is_sync(is_sync),
    _show_pid(show_pid),
    _show_timestamp(show_timestamp),
    _show_thread_id(show_thread_id) {

}

/**
 *  Copy constructor.
 */
backend::backend(backend const& right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
backend::~backend() throw () {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
backend& backend::operator=(backend const& right) {
  if (this != &right)
    _internal_copy(right);
  return (*this);
}

/**
 *  Get if all backends was synchronize.
 *
 *  @return True if synchronize, otherwise false.
 */
bool backend::enable_sync() const {
  concurrency::locker lock(&_lock);
  return (_is_sync);
}

/**
 *  Set if all backends was synchronize.
 *
 *  @param[in] enable  True to synchronize backends data.
 */
void backend::enable_sync(bool enable) {
  concurrency::locker lock(&_lock);
  _is_sync = enable;
}

/**
 *  Log messages.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 *  @param[in] msg      The message to log.
 */
void backend::log(
                unsigned long long types,
                unsigned int verbose,
                char const* msg) throw () {
  log(
    types,
    verbose,
    msg,
    static_cast<unsigned int>(strlen(msg)));
  return ;
}

/**
 *  Get if the pid is display.
 *
 *  @return True if pid is display, otherwise false.
 */
bool backend::show_pid() const {
  concurrency::locker lock(&_lock);
  return (_show_pid);
}

/**
 *  Set pid display.
 *
 *  @param[in] enable  Enable or disable display pid.
 */
void backend::show_pid(bool enable) {
  concurrency::locker lock(&_lock);
  _show_pid = enable;
}

/**
 *  Get if the timestamp is display.
 *
 *  @return Time precision is display, otherwise none.
 */
time_precision backend::show_timestamp() const {
  concurrency::locker lock(&_lock);
  return (_show_timestamp);
}

/**
 *  Set timestamp display.
 *
 *  @param[in] enable  Enable or disable display timestamp.
 */
void backend::show_timestamp(time_precision val) {
  concurrency::locker lock(&_lock);
  _show_timestamp = val;
}

/**
 *  Get if the thread id is display.
 *
 *  @return True if thread id is display, otherwise false.
 */
bool backend::show_thread_id() const {
  concurrency::locker lock(&_lock);
  return (_show_thread_id);
}

/**
 *  Set thread id display.
 *
 *  @param[in] enable  Enable or disable display thread id.
 */
void backend::show_thread_id(bool enable) {
  concurrency::locker lock(&_lock);
  _show_thread_id = enable;
}

/**
 *  Build header line with backend information.
 *
 *  @param[out] buffer  The buffer to fill.
 */
void backend::_build_header(misc::stringifier& buffer) {
  // Build line header.
  if (_show_timestamp == second)
    buffer << "[" << timestamp::now().to_seconds() << "] ";
  else if (_show_timestamp == millisecond)
    buffer << "[" << timestamp::now().to_mseconds() << "] ";
  else if (_show_timestamp == microsecond)
    buffer << "[" << timestamp::now().to_useconds() << "] ";
  if (_show_pid) {
#ifdef _WIN32
    buffer << "[" << GetCurrentProcessId() << "] ";
#else
    buffer << "[" << getpid() << "] ";
#endif
  }
  if (_show_thread_id)
    buffer << "[" << concurrency::thread::get_current_id() << "] ";
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void backend::_internal_copy(backend const& right) {
  concurrency::locker lock1(&_lock);
  concurrency::locker lock2(&right._lock);
  _is_sync = right._is_sync;
  _show_pid = right._show_pid;
  _show_timestamp = right._show_timestamp;
  _show_thread_id = right._show_thread_id;
}
