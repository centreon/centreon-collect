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

#include <assert.h>
#include <stdlib.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/library.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
library::library(std::string const& filename)
  : _filename(filename),
    _handle(NULL) {

}

/**
 *  Destructor.
 */
library::~library() throw () {
  try {
    unload();
  }
  catch (...) {

  }
}

/**
 *  Get the library filename.
 *
 *  @return The filename.
 */
std::string const& library::filename() const throw () {
  return (_filename);
}

/**
 *  Check if the library has loaded.
 *
 *  @return True if the library is loaded, otherwise false.
 */
bool library::is_loaded() const throw () {
  return (_handle);
}

/**
 *  Load the library.
 */
void library::load() {
  if (_handle)
    return;
  if (!(_handle = dlopen(_filename.c_str(), RTLD_NOW | RTLD_GLOBAL)))
    throw (basic_error() << "load library failed: " << dlerror());
}

/**
 *  Return the symbole address.
 *
 *  @param[in] symbole  The exported symbole.
 *
 *  @return Symbole adrress.
 */
void* library::resolve(char const* symbole) {
  if (!_handle)
    throw (basic_error() << "resolve symbole failed: library not loaded");
  dlerror();
  void* sym(dlsym(_handle, symbole));
  char const* msg(dlerror());
  if (msg)
    throw (basic_error() << "resolve symbole failed: " << msg);
  return (sym);
}

/**
 *  Return the symbole address.
 *
 *  @param[in] symbole  The exported symbole.
 *
 *  @return Symbole adrress.
 */
void* library::resolve(std::string const& symbole) {
  return (resolve(symbole.c_str()));
}

/**
 *  Unload the library.
 */
void library::unload() {
  if (!_handle)
    return;
  if (dlclose(_handle))
    throw (basic_error() << "unload library failed: " << dlerror());
  _handle = NULL;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
library::library(library const& right) {
  _internal_copy(right);
}

/**
 *  Assignment operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
library& library::operator=(library const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Calls abort().
 *
 *  @param[in] t Unused.
 */
void library::_internal_copy(library const& right) {
  (void)right;
  assert(!"library is not copyable");
  abort();
  return ;
}
