/**
 * Copyright 2011-2013,2015, 2021 Centreon
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

#include "com/centreon/broker/modules/handle.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::modules;

using log_v2 = com::centreon::common::log_v2::log_v2;

constexpr const char* handle::deinitialization;
constexpr const char* handle::initialization;
constexpr const char* handle::updatization;
constexpr const char* handle::versionning;
constexpr const char* handle::parents_list;

handle::handle(const std::string& filename, void* h, const void* arg)
    : _filename{filename},
      _handle{h},
      _logger{log_v2::instance().get(log_v2::CORE)} {
  _init(arg);
}

/**
 *  Destructor.
 */
handle::~handle() noexcept {
  try {
    _close();
  } catch (std::exception const& e) {
    _logger->error("{}", e.what());
  } catch (...) {
    _logger->error("modules: unknown error while unloading '{}'", _filename);
  }
}

/**
 *  @brief Close the library object.
 *
 *  If the underlying library object is open, this method will call the
 *  module's deinitialization routine (if it exists) and then unload the
 *  library.
 */
void handle::_close() {
  if (is_open()) {
    // Log message.
    _logger->info("modules: closing '{}'", _filename);

    // Find deinitialization routine.
    union {
      bool (*code)();
      void* data;
    } sym;
    sym.data = dlsym(_handle, deinitialization);

    bool can_unload = true;
    // Could not find deinitialization routine.
    char const* error_str{dlerror()};
    if (error_str) {
      _logger->info(
          "modules: could not find deinitialization routine in '{}': {}",
          _filename, error_str);
    }
    // Call deinitialization routine.
    else {
      _logger->debug("modules: running deinitialization routine of '{}'",
                     _filename);
      can_unload = (*(sym.code))();
    }

    if (!can_unload) {
      _logger->debug("modules: don't unload library '{}'", _filename);
      return;
    }
    // Reset library handle.
    _logger->debug("modules: unloading library '{}'", _filename);
    // Library was not unloaded.
    if (dlclose(_handle)) {
      char const* error_str{dlerror()};
      _logger->info("modules: could not unload library '{}': {}", _filename,
                    error_str);
    } else
      _handle = nullptr;
  }
}

/**
 *  Check if the library is loaded.
 *
 *  @return true if the library is loaded, false otherwise.
 */
bool handle::is_open() const {
  return _handle;
}

/**
 *  Update a library file.
 *
 *  @param[in] arg Library argument.
 */
void handle::update(void const* arg) {
  // Check that library is loaded.
  if (!is_open())
    throw msg_fmt("modules: could not update module that is not loaded");

  // Find update routine.
  union {
    void (*code)();
    void* data;
  } sym;
  sym.data = dlsym(_handle, updatization);

  // Found routine.
  if (sym.data) {
    _logger->debug("modules: running update routine of '{}'", _filename);
    (*(void (*)(void const*))(sym.code))(arg);
  }
}

/**
 *  Call the module's initialization routine.
 *
 *  @param[in] arg Module argument.
 */
void handle::_init(void const* arg) {
  // Find initialization routine.
  union {
    void (*code)();
    void* data;
  } sym;
  sym.data = dlsym(_handle, initialization);

  // Call initialization routine.
  _logger->debug("modules: running initialization routine of '{}'", _filename);
  (*(void (*)(void const*))(sym.code))(arg);
}
