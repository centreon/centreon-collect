/**
 * Copyright 2011-2013, 2021 Centreon
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

#ifndef CC_TEST_LOGGING_BACKEND_TEST_HH
#define CC_TEST_LOGGING_BACKEND_TEST_HH

#include "com/centreon/logging/backend.hh"
#include "com/centreon/misc/stringifier.hh"

namespace com::centreon::logging {
/**
 *  @class backend_test
 *  @brief litle implementation of backend to test logging engine.
 */
class backend_test : public backend {
 public:
  backend_test(bool is_sync = false,
               bool show_pid = false,
               time_precision show_timestamp = none,
               bool show_thread_id = false)
      : backend(is_sync, show_pid, show_timestamp, show_thread_id),
        _nb_call(0) {}
  ~backend_test() noexcept {}
  void close() noexcept {}
  std::string const& data() const noexcept { return _buffer; }
  void log(uint64_t types,
           uint32_t verbose,
           char const* msg,
           uint32_t size) noexcept {
    std::lock_guard<std::recursive_mutex> lock(_lock);

    (void)types;
    (void)verbose;

    misc::stringifier header;
    _build_header(header);
    _buffer.append(header.data(), header.size());
    _buffer.append(msg, size);
    ++_nb_call;
  }
  uint32_t get_nb_call() const noexcept { return _nb_call; }
  void open() {}
  void reopen() {}
  void reset() noexcept { _buffer.clear(); }

 private:
  std::string _buffer;
  uint32_t _nb_call;
};
}  // namespace com::centreon::logging

#endif  // !CC_TEST_LOGGING_BACKEND_TEST_HH
