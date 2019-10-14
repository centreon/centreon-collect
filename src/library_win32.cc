/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <cstdlib>
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
    : _filename(filename), _handle(NULL) {}

/**
 *  Destructor.
 */
library::~library() throw() {
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
std::string const& library::filename() const throw() { return (_filename); }

/**
 *  Check if the library has loaded.
 *
 *  @return True if the library is loaded, otherwise false.
 */
bool library::is_loaded() const throw() { return (_handle); }

/**
 *  Load the library.
 */
void library::load() {
  if (_handle)
    return;
  if (!(_handle = LoadLibrary(_filename.c_str()))) {
    DWORD errcode(GetLastError());
    throw(basic_error() << "load library failed: error " << errcode);
  }
}

/**
 *  Return the symbol address.
 *
 *  @param[in] symbol  The exported symbol.
 *
 *  @return Symbol address.
 */
void* library::resolve(char const* symbol) {
  if (!_handle)
    throw(basic_error() << "resolve symbol failed: "
                           "library not loaded");
  void* sym(GetProcAddress(_handle, symbol));
  if (!sym) {
    DWORD errcode(GetLastError());
    throw(basic_error() << "resolve symbol failed: "
                           "error " << errcode);
  }
  return (sym);
}

/**
 *  Overloaded method.
 *
 *  @see resolve
 */
void* library::resolve(std::string const& symbol) {
  return (resolve(symbol.c_str()));
}

/**
 *  Return the process symbol address.
 *
 *  @param[in] symbol Symbol to resolve.
 *
 *  @return Symbol address.
 */
void (*library::resolve_proc(char const* symbol))() {
  return ((void (*)())resolve(symbol));
}

/**
 *  Overloaded method.
 *
 *  @see resolve_proc
 */
void (*library::resolve_proc(std::string const& symbol))() {
  return (resolve_proc(symbol.c_str()));
}

/**
 *  Unload the library.
 */
void library::unload() {
  if (!_handle)
    return;
  if (!FreeLibrary(_handle)) {
    DWORD errcode(GetLastError());
    throw(basic_error() << "unload library failed: error " << errcode);
  }
  _handle = NULL;
}
