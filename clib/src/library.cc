/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/library.hh"
#include <cstdlib>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 */
library::library(std::string const& filename)
    : _filename(filename), _handle(NULL) {}

/**
 *  Destructor.
 */
library::~library() noexcept {
  try {
    unload();
  } catch (...) {
  }
}

/**
 *  Get the library filename.
 *
 *  @return The filename.
 */
std::string const& library::filename() const noexcept {
  return _filename;
}

/**
 *  Check if the library has loaded.
 *
 *  @return True if the library is loaded, otherwise false.
 */
bool library::is_loaded() const noexcept {
  return _handle;
}

/**
 *  Load the library.
 */
void library::load() {
  if (_handle)
    return;
  if (!(_handle = dlopen(_filename.c_str(), RTLD_LAZY | RTLD_GLOBAL)))
    throw basic_error() << "load library failed: " << dlerror();
}

/**
 *  Return the data symbol address.
 *
 *  @param[in] symbol  The exported symbol.
 *
 *  @return Symbol address.
 */
void* library::resolve(char const* symbol) {
  if (!_handle)
    throw basic_error() << "could not find symbol '" << symbol
                        << "': library not loaded";
  dlerror();
  void* sym(dlsym(_handle, symbol));
  if (!sym) {
    char const* msg(dlerror());
    throw basic_error() << "could not find symbol '" << symbol
                        << "': " << (msg ? msg : "unknown error");
  }
  return sym;
}

/**
 *  Overloaded method.
 *
 *  @see resolve
 */
void* library::resolve(std::string const& symbol) {
  return resolve(symbol.c_str());
}

/**
 *  Return the process symbol address.
 *
 *  @param[in] symbol  Symbol to resolve.
 *
 *  @return Symbol address.
 */
void (*library::resolve_proc(char const* symbol))() {
  union {
    void (*func)();
    void* data;
  } type;
  type.data = resolve(symbol);
  return type.func;
}

/**
 *  Overloaded method.
 *
 *  @see resolve_proc
 */
void (*library::resolve_proc(std::string const& symbol))() {
  return resolve_proc(symbol.c_str());
}

/**
 *  Unload the library.
 */
void library::unload() {
  if (!_handle)
    return;
  if (dlclose(_handle))
    throw basic_error() << "unload library failed: " << dlerror();
  _handle = nullptr;
}
