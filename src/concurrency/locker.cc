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
#include "com/centreon/concurrency/locker.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 *
 *  @param[in] m  The mutex to lock.
 */
locker::locker(mutex* m)
  : _m(m) {
  if (_m)
    relock();
}

/**
 *  Default destructor.
 */
locker::~locker() throw () {
  unlock();
}

/**
 *  Get the mutex.
 *
 *  @return The internal mutex.
 */
mutex* locker::get_mutex() const throw() {
  return (_m);
}

/**
 *  Lock the internal mutex.
 */
void locker::relock() {
  if (_m)
    _m->lock();
}

/**
 *  Unlock the internal mutex.
 */
void locker::unlock() {
  if (_m)
    _m->unlock();
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
locker::locker(locker const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
locker& locker::operator=(locker const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
locker& locker::_internal_copy(locker const& right) {
  (void)right;
  assert(!"impossible to copy locker");
  abort();
  return (*this);
}
