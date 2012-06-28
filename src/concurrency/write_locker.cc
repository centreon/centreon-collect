/*
** Copyright 2012 Merethis
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
#include "com/centreon/concurrency/read_write_lock.hh"
#include "com/centreon/concurrency/write_locker.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 */
write_locker::write_locker(read_write_lock* rwl)
  : _locked(false), _rwl(rwl) {
  relock();
}

/**
 *  Destructor.
 */
write_locker::~write_locker() throw () {
  try {
    if (_locked)
      unlock();
  }
  catch (...) {}
}

/**
 *  Relock.
 */
void write_locker::relock() {
  _rwl->write_lock();
  _locked = true;
  return ;
}

/**
 *  Unlock.
 */
void write_locker::unlock() {
  _rwl->write_unlock();
  _locked = false;
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
 *  @param[in] right Object to copy.
 */
write_locker::write_locker(write_locker const& right) {
  _internal_copy(right);
}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
write_locker& write_locker::operator=(write_locker const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void write_locker::_internal_copy(write_locker const& right) {
  (void)right;
  assert(!"write locker is not copyable");
  abort();
  return ;
}
