/*
** Copyright 2012-2013 Centreon
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

#ifndef CC_LIBRARY_POSIX_HH
#define CC_LIBRARY_POSIX_HH

#include <dlfcn.h>
#include <string>

namespace com::centreon {

/**
 *  @class library library.hh "com/centreon/library.hh"
 *  @brief Wrapper of libc's library loader.
 *
 *  Wrap standard library loader objects.
 */
class library {
 public:
  library(std::string const& filename);
  ~library() throw();
  std::string const& filename() const throw();
  bool is_loaded() const throw();
  void load();
  void* resolve(char const* symbol);
  void* resolve(std::string const& symbol);
  void (*resolve_proc(char const* symbol))();
  void (*resolve_proc(std::string const& symbol))();
  void* release() noexcept;

 private:
  library(library const& right);
  library& operator=(library const& right);
  /* dlclose() in the middle of the process life is dangerous: code of the
   * library (vtables, asio services, ...) may still be referenced elsewhere.
   * Modules must be detached with release() and their dlclose() deferred
   * (see com::centreon::common::defer_dlclose()). The destructor keeps
   * unloading as a last resort for never-detached libraries. */
  void unload();

  std::string _filename;
  void* _handle;
};

}

#endif  // !CC_LIBRARY_POSIX_HH
