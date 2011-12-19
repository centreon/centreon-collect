/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include "com/centreon/connector/ssh/multiplexer.hh"

using namespace com::centreon::connector::ssh;

// Class instance pointer.
std::auto_ptr<multiplexer> multiplexer::_instance;

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
  if (!_instance.get())
    _instance.reset(new multiplexer);
  return ;
}

/**
 *  Unload singleton.
 */
void multiplexer::unload() {
  _instance.reset();
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
  : com::centreon::task_manager(0),
    com::centreon::handle_manager(this) {}

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] m Unused.
 */
multiplexer::multiplexer(multiplexer const& m)
  : com::centreon::task_manager(),
    com::centreon::handle_manager() {
  (void)m;
  assert(false);
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] m Unused.
 *
 *  @return This object.
 */
multiplexer& multiplexer::operator=(multiplexer const& m) {
  (void)m;
  assert(false);
  abort();
  return (*this);
}
