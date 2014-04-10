/*
** Copyright 2014 Merethis
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

#include "com/centreon/exceptions/interruption.hh"

using namespace com::centreon::exceptions;

/**
 *  Default constructor.
 */
interruption::interruption() {}

/**
 *  Constructor with debugging informations.
 *
 *  @param[in] file      The file in which this object is created.
 *  @param[in] function  The function creating this object.
 *  @param[in] line      The line in the file creating this object.
 */
interruption::interruption(
                char const* file,
                char const* function,
                int line)
  : basic(file, function, line) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
interruption::interruption(interruption const& other) : basic(other) {}

/**
 *  Destructor.
 */
interruption::~interruption() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
interruption& interruption::operator=(interruption const& other) {
  basic::operator=(other);
  return (*this);
}
