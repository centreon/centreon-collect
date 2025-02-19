/**
 * Copyright 2020-2023 Centreon
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

#ifndef CCB_STATS_CENTER_HH
#define CCB_STATS_CENTER_HH

#include <absl/synchronization/mutex.h>
#include "broker.pb.h"

namespace com::centreon::broker::stats {
/**
 * @brief Centralize Broker statistics.
 *
 * It works with the Broker thread pool. To avoid mutexes, we use a strand.
 * Then each modification in stats is serialized through the strand. The big
 * interest is that statistics are always written asynchronously and the
 * impact on the software is almost nothing.
 *
 * The idea is that even there are several threads used to fill Broker
 * statistics, never two updates are done at the same time. The strand forces
 * an update to be done after another one. And then we don't need mutex to
 * protect data.
 *
 * To help the software developer to use this strand not directly available
 * to the developer, member functions are declared.
 *
 * Examples:
 * * update(&EndpointStats::set_queued_events, _stats, value)
 *   calls throw the thread pool the EndpointStats::set_queued_events member
 *   function of the EndpointStats* _stats object with the value value.
 * * update(_stats->mutable_state(), state)
 *   sets the std::string state() of the _stats EndpointStats object to the
 *   value value.
 */
class center {
  BrokerStats _stats ABSL_GUARDED_BY(_stats_m);
  mutable absl::Mutex _stats_m;
  int _json_stats_file_creation;

 public:
  center();

  std::string to_string() ABSL_LOCKS_EXCLUDED(_stats_m);

  EngineStats* register_engine() ABSL_LOCKS_EXCLUDED(_stats_m);
  ConflictManagerStats* register_conflict_manager()
      ABSL_LOCKS_EXCLUDED(_stats_m);
  void unregister_muxer(const std::string& name) ABSL_LOCKS_EXCLUDED(_stats_m);
  void update_muxer(std::string name,
                    std::string queue_file,
                    uint32_t size,
                    uint32_t unack) ABSL_LOCKS_EXCLUDED(_stats_m);
  void init_queue_file(std::string muxer,
                       std::string queue_file,
                       uint32_t max_file_size) ABSL_LOCKS_EXCLUDED(_stats_m);

  bool muxer_stats(const std::string& name, MuxerStats* response)
      ABSL_LOCKS_EXCLUDED(_stats_m);
  MuxerStats* muxer_stats(const std::string& name)
      ABSL_LOCKS_EXCLUDED(_stats_m);
  void clear_muxer_queue_file(const std::string& name)
      ABSL_LOCKS_EXCLUDED(_stats_m);

  bool get_sql_connection_stats(uint32_t index, SqlConnectionStats* response)
      ABSL_LOCKS_EXCLUDED(_stats_m);
  void get_conflict_manager_stats(ConflictManagerStats* response);
  void get_sql_manager_stats(SqlManagerStats* response, int32_t id = -1)
      ABSL_LOCKS_EXCLUDED(_stats_m);
  SqlConnectionStats* connection(size_t idx);
  SqlConnectionStats* add_connection() ABSL_LOCKS_EXCLUDED(_stats_m);
  void remove_connection(SqlConnectionStats* stats)
      ABSL_LOCKS_EXCLUDED(_stats_m);

  int get_json_stats_file_creation(void);
  void get_sql_connection_size(GenericSize* response)
      ABSL_LOCKS_EXCLUDED(_stats_m);
  void get_processing_stats(ProcessingStats* response)
      ABSL_LOCKS_EXCLUDED(_stats_m);
  const BrokerStats& stats() const;
  void lock() ABSL_EXCLUSIVE_LOCK_FUNCTION(_stats_m);
  void unlock() ABSL_UNLOCK_FUNCTION(_stats_m);

  /**
   * @brief Set the value pointed by ptr to the value value.
   *
   * @tparam T The template class.
   * @param ptr A pointer to object of type T
   * @param value The value of type T to set.
   */
  template <typename T>
  void update(T* ptr, T value) ABSL_LOCKS_EXCLUDED(_stats_m) {
    absl::MutexLock lck(&_stats_m);
    *ptr = std::move(value);
  }

  /**
   * @brief Sometimes with protobuf, we can not access to a mutable pointer.
   * For example with int32 values. We have instead a setter member function.
   * To be able to call it, we provide this method that needs the method
   * to call, a pointer to the EndpointStats object and the value to set.
   *
   * @tparam T The type of the value to set.
   * @param U::*f A member function of U.
   * @param ptr A pointer to an existing U.
   * @param value The value to set.
   */
  template <typename U, typename T>
  void update(void (U::*f)(T), U* ptr, T value) ABSL_LOCKS_EXCLUDED(_stats_m) {
    absl::MutexLock lck(&_stats_m);
    (ptr->*f)(value);
  }

  void execute(std::function<void()>&& f) ABSL_LOCKS_EXCLUDED(_stats_m) {
    absl::MutexLock lck(&_stats_m);
    f();
  }

  template <typename U, typename T>
  const T& get(T (U::*f)() const, const U* ptr) ABSL_LOCKS_EXCLUDED(_stats_m) {
    absl::MutexLock lck(&_stats_m);
    return (ptr->*f)();
  }
};

}  // namespace com::centreon::broker::stats

#endif /* !CCB_STATS_CENTER_HH */
