/**
 * Copyright 2011-2025 Centreon
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

#ifndef CCE_COMMANDS_COMMAND_LISTENER_HH
#define CCE_COMMANDS_COMMAND_LISTENER_HH

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include "com/centreon/engine/commands/result.hh"

namespace com::centreon::engine::commands {

class command;
/**
 *  @class command_listener command_listener.hh
 *  @brief Notify command events.
 *
 *  This class provide interface to notify command events.
 */
class command_listener {
  std::unordered_map<command*, std::function<void()>> _clean_callbacks
      ABSL_GUARDED_BY(_clean_callbacks_m);
  absl::Mutex _clean_callbacks_m;

 public:
  virtual ~command_listener() noexcept {
    for (auto it = _clean_callbacks.begin(), end = _clean_callbacks.end();
         it != end; ++it) {
      (it->second)();
    }
  }

  virtual void finished(result const& res) noexcept = 0;
  void reg(command* const ptr, std::function<void()>& regf) {
    absl::MutexLock l(&_clean_callbacks_m);
    _clean_callbacks.insert({ptr, regf});
  }
  void unreg(command* const ptr) {
    absl::MutexLock l(&_clean_callbacks_m);
    _clean_callbacks.erase(ptr);
  }
};

}  // namespace com::centreon::engine::commands

#endif  // !CCE_COMMANDS_COMMAND_LISTENER_HH
