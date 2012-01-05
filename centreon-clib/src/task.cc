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

#include "com/centreon/task.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
task::task() {}

/**
 *  Copy constructor.
 *
 *  @param[in] t Object to copy.
 */
task::task(task const& t) {
  (void)t;
}

/**
 *  Destructor.
 */
task::~task() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] t Object to copy.
 *
 *  @return This object.
 */
task& task::operator=(task const& t) {
  (void)t;
  return (*this);
}

/**
 *  Empty task.
 */
void task::run() {
  return ;
}
