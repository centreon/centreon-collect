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

#ifndef CMT_BY_THREAD_TRACE_ACTIVE_HH
#define CMT_BY_THREAD_TRACE_ACTIVE_HH

#include "intrusive_map.hh"

namespace com::centreon::malloc_trace {

/**
 * @brief This class is used to store the tracing of a thread
 * The problem is: malloc is called, we explore call stack and this research may
 * do another malloc and we risk an infinite loop
 * So the first malloc set the _malloc_trace_active and explore call stack
 * The next malloc called by stacktrace process try to set _malloc_trace_active
 * and as it's yet setted we don't try to explore call stack
 *
 */
class thread_trace_active : public boost::intrusive::set_base_hook<> {
  pid_t _thread_id;
  mutable bool _malloc_trace_active = false;

 public:
  thread_trace_active() {}
  thread_trace_active(pid_t thread_id) : _thread_id(thread_id) {}

  pid_t get_thread_id() const { return _thread_id; }

  /**
   * @brief Set _malloc_trace_active
   *
   * @return old value of _malloc_trace_active
   */
  bool set_malloc_trace_active() const {
    if (_malloc_trace_active) {
      return true;
    }
    _malloc_trace_active = true;
    return false;
  }

  /**
   * @brief reset _malloc_trace_active
   *
   * @return old value of _malloc_trace_active
   */
  bool reset_malloc_trace_active() const {
    if (!_malloc_trace_active) {
      return false;
    }
    _malloc_trace_active = false;
    return true;
  }

  bool is_malloc_trace_active() const { return _malloc_trace_active; }

  struct key_extractor {
    using type = pid_t;
    type operator()(const thread_trace_active& node) const {
      return node._thread_id;
    }
  };
};

/**
 * @brief container of thread_trace_active with zero allocation
 * the drawback is that we are limited to store 4096 thread trace states
 *
 */
class thread_dump_active
    : protected intrusive_map<thread_trace_active,
                              thread_trace_active::key_extractor,
                              4096> {
  std::mutex _protect;

 public:
  bool set_dump_active(pid_t thread_id);
  void reset_dump_active(pid_t thread_id);
};

}  // namespace com::centreon::malloc_trace

#endif
