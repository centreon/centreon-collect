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

#include "com/centreon/handle.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 *
 *  @param[in] internal_handle  Set the internal handle.
 */
handle::handle(native_handle internal_handle)
  : _internal_handle(internal_handle) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
handle::handle(handle const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
handle::~handle() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle& handle::operator=(handle const& right) {
  return (_internal_copy(right));
}

/**
 *  Get the internal handle.
 *
 *  @return The internal handle.
 */
native_handle handle::get_internal_handle() const throw () {
  return (_internal_handle);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle& handle::_internal_copy(handle const& right) {
  if (this != &right) {
    _internal_handle = right._internal_handle;
  }
  return (*this);
}
