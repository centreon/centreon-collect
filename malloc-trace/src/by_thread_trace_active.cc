/**
 * Copyright 2024 Centreon
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

#include "by_thread_trace_active.hh"

using namespace com::centreon::malloc_trace;

/**
 * @brief Set the flag to true in _by_thread_dump_active
 *
 * @return true the flag was not setted before call
 * @return false the flag was yet setted before call
 */
bool thread_dump_active::set_dump_active(pid_t thread_id) {
  std::lock_guard l(_protect);
  if (!is_initialized()) {
    return false;
  }
  const thread_trace_active* exist = find(thread_id);

  if (!exist) {
    const thread_trace_active* inserted = insert_and_get(thread_id);
    if (!inserted) {
      return false;
    }
    inserted->set_malloc_trace_active();
    return true;
  } else {
    return !exist->set_malloc_trace_active();
  }
}

void thread_dump_active::reset_dump_active(pid_t thread_id) {
  std::lock_guard l(_protect);
  if (!is_initialized()) {
    return;
  }
  const thread_trace_active* exist = find(thread_id);
  if (exist) {
    exist->reset_malloc_trace_active();
  }
}
