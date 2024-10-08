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

#include "com/centreon/logging/backend.hh"
#include <fmt/std.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/timestamp.hh"

using namespace com::centreon::logging;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Constructor.
 *
 *  @param[in] is_sync         Enable synchronization.
 *  @param[in] show_pid        Enable show pid.
 *  @param[in] show_timestamp  Enable show timestamp.
 *  @param[in] show_thread_id  Enable show thread id.
 */
backend::backend(bool is_sync,
                 bool show_pid,
                 time_precision show_timestamp,
                 bool show_thread_id)
    : _is_sync(is_sync),
      _show_pid(show_pid),
      _show_timestamp(show_timestamp),
      _show_thread_id(show_thread_id) {}

/**
 *  Copy constructor.
 */
backend::backend(backend const& right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
backend::~backend() throw() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
backend& backend::operator=(backend const& right) {
  if (this != &right)
    _internal_copy(right);
  return (*this);
}

/**
 *  Get if all backends was synchronize.
 *
 *  @return True if synchronize, otherwise false.
 */
bool backend::enable_sync() const {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  return (_is_sync);
}

/**
 *  Set if all backends was synchronize.
 *
 *  @param[in] enable  True to synchronize backends data.
 */
void backend::enable_sync(bool enable) {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _is_sync = enable;
}

/**
 *  Log messages.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 *  @param[in] msg      The message to log.
 */
void backend::log(uint64_t types, uint32_t verbose, char const* msg) noexcept {
  log(types, verbose, msg, static_cast<uint32_t>(strlen(msg)));
}

/**
 *  Get if the pid is display.
 *
 *  @return True if pid is display, otherwise false.
 */
bool backend::show_pid() const {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  return (_show_pid);
}

/**
 *  Set pid display.
 *
 *  @param[in] enable  Enable or disable display pid.
 */
void backend::show_pid(bool enable) {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _show_pid = enable;
}

/**
 *  Get if the timestamp is display.
 *
 *  @return Time precision is display, otherwise none.
 */
time_precision backend::show_timestamp() const {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  return (_show_timestamp);
}

/**
 *  Set timestamp display.
 *
 *  @param[in] enable  Enable or disable display timestamp.
 */
void backend::show_timestamp(time_precision val) {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _show_timestamp = val;
}

/**
 *  Get if the thread id is display.
 *
 *  @return True if thread id is display, otherwise false.
 */
bool backend::show_thread_id() const {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  return (_show_thread_id);
}

/**
 *  Set thread id display.
 *
 *  @param[in] enable  Enable or disable display thread id.
 */
void backend::show_thread_id(bool enable) {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _show_thread_id = enable;
}

/**
 *  Build header line with backend information.
 *
 *  @param[out] buffer  The buffer to fill.
 */
std::string backend::_build_header() {
  // Build line header.
  std::string buffer;
  if (_show_timestamp == second)
    buffer = fmt::format("[{}] ", timestamp::now().to_seconds());
  else if (_show_timestamp == millisecond)
    buffer = fmt::format("[{}] ", timestamp::now().to_mseconds());
  else if (_show_timestamp == microsecond)
    buffer = fmt::format("[{}] ", timestamp::now().to_useconds());
  if (_show_pid) {
    buffer += fmt::format("[{}] ", getpid());
  }
  if (_show_thread_id)
    buffer += fmt::format("[{}] ", std::this_thread::get_id());
  return buffer;
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void backend::_internal_copy(backend const& right) {
  std::lock_guard<std::recursive_mutex> lock1(_lock);
  std::lock_guard<std::recursive_mutex> lock2(right._lock);
  _is_sync = right._is_sync;
  _show_pid = right._show_pid;
  _show_timestamp = right._show_timestamp;
  _show_thread_id = right._show_thread_id;
}
