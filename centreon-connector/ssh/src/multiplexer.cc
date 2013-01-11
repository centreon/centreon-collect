/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon SSH Connector.
**
** Centreon SSH Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon SSH Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon SSH Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include "com/centreon/connector/ssh/multiplexer.hh"

using namespace com::centreon::connector::ssh;

// Class instance pointer.
static multiplexer* _instance = NULL;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Destructor.
 */
multiplexer::~multiplexer() throw () {}

/**
 *  Get class instance.
 *
 *  @return multiplexer instance.
 */
multiplexer& multiplexer::instance() throw () {
  return (*_instance);
}

/**
 *  Load singleton.
 */
void multiplexer::load() {
  if (!_instance)
    _instance = new multiplexer;
  return ;
}

/**
 *  Unload singleton.
 */
void multiplexer::unload() {
  delete _instance;
  _instance = NULL;
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
multiplexer::multiplexer()
  : com::centreon::handle_manager(this) {}
