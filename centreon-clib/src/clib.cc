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

#include <cassert>
#include <cstdlib>
#include "com/centreon/clib.hh"
#include "com/centreon/logging/engine.hh"
#include "com/centreon/process_manager.hh"

using namespace com::centreon;

// Class instance.
static clib* _instance = NULL;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Load the clib singleton.
 */
void clib::load() {
  delete _instance;
  _instance = new clib();
  return;
}

/**
 *  Unload the clib singleton.
 */
void clib::unload() {
  delete _instance;
  _instance = NULL;
  return;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Constructor.
 */
clib::clib() {
  logging::engine::load();
  process_manager::load();
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
clib::clib(clib const& right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
clib::~clib() throw () {
  process_manager::unload();
  logging::engine::unload();
}

/**
 *  Copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
clib& clib::operator=(clib const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Internal copy.
 *
 *  @param[right] right  The object to copy.
 */
void clib::_internal_copy(clib const& right) {
  (void)right;
  assert(!"clib is not copyable");
  abort();
  return;
}
