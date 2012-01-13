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

#include "com/centreon/handle_listener.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 */
handle_listener::handle_listener() {

}

/**
 *  Default destructor.
 */
handle_listener::~handle_listener() throw () {

}

/**
 *  Read action on a specific handle.
 *
 *  @param[in] h  The handle notify by a read.
 */
void handle_listener::read(handle& h) {
  // Basic implementation, discard all data.
  char buf[4096];
  h.read(buf, sizeof(buf));
}

/**
 *  Define if the listener was notify for the read action.
 *
 *  @param[in] h  The handle to define to be read.
 *
 *  @return Always return false.
 */
bool handle_listener::want_read(handle& h) {
  (void)h;
  return (false);
}

/**
 *  Define if the listener was notify for the write action.
 *
 *  @param[in] h  The handle to define to be write.
 *
 *  @return Always return false.
 */
bool handle_listener::want_write(handle& h) {
  (void)h;
  return (false);
}

/**
 *  Write action on a specific handle.
 *
 *  @param[in] h  The handle notify by a write.
 */
void handle_listener::write(handle& h) {
  (void)h;
  // Basic implementation, do nothing.
}
