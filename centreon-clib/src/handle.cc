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

#include "com/centreon/handle.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
handle::handle() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
handle::handle(handle const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
handle::~handle() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle& handle::operator=(handle const& right) {
  (void)right;
  return (*this);
}
