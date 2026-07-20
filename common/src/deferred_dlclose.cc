/**
 * Copyright 2026 Centreon
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

#include "deferred_dlclose.hh"

#include <dlfcn.h>

#include <mutex>
#include <vector>

namespace {
std::mutex _deferred_m;
std::vector<void*> _deferred_handles;
}  // namespace

namespace com::centreon::common {

/**
 * @brief Register a dlopen() handle whose dlclose() must be postponed to the
 * very end of the program, once no object living in the library can be
 * referenced anymore (asio services registered in a global io_context,
 * vtables, ...). The caller gives up ownership of the handle.
 *
 * @param handle The handle returned by dlopen(). nullptr is ignored.
 */
void defer_dlclose(void* handle) noexcept {
  if (!handle)
    return;
  std::lock_guard<std::mutex> lock(_deferred_m);
  _deferred_handles.push_back(handle);
}

/**
 * @brief dlclose() all the handles registered with defer_dlclose(). To be
 * called at the end of main(), after every global object able to reference
 * code from the loaded libraries (in particular the io_context shared with
 * the modules) has been destroyed. If it is never called, the libraries just
 * stay mapped until the process exits, which is harmless.
 */
void run_deferred_dlclose() noexcept {
  std::lock_guard<std::mutex> lock(_deferred_m);
  for (void* h : _deferred_handles)
    dlclose(h);
  _deferred_handles.clear();
}

}  // namespace com::centreon::common
