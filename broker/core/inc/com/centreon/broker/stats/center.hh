/*
** Copyright 2020-2021 Centreon
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

#ifndef CCB_STATS_CENTER_HH
#define CCB_STATS_CENTER_HH

#include "broker.pb.h"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace stats {
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
  static center* _instance;
  BrokerStats _stats;
  std::mutex _stats_m;
  int _json_stats_file_creation;

  center();
  ~center();

 public:
  static center& instance();
  static void load();
  static void unload();
  std::string to_string();

  EngineStats* register_engine();
  ConflictManagerStats* register_conflict_manager();
  void unregister_muxer(const std::string& name);
  void update_muxer(std::string name,
                    std::string queue_file,
                    uint32_t size,
                    uint32_t unack);
  void init_queue_file(std::string muxer,
                       std::string queue_file,
                       uint32_t max_file_size);

  bool muxer_stats(const std::string& name, MuxerStats* response);
  MuxerStats* muxer_stats(const std::string& name);
  void clear_muxer_queue_file(const std::string& name);

  bool get_sql_connection_stats(uint32_t index, SqlConnectionStats* response);
  void get_conflict_manager_stats(ConflictManagerStats* response);
  void get_sql_manager_stats(SqlManagerStats* response, int32_t id = -1);
  SqlConnectionStats* connection(size_t idx);
  size_t add_connection();
  void remove_connection(size_t idx);

  int get_json_stats_file_creation(void);
  void get_sql_connection_size(GenericSize* response);
  void get_processing_stats(ProcessingStats* response);
  void lock();
  void unlock();

  /**
   * @brief Set the value pointed by ptr to the value value.
   *
   * @tparam T The template class.
   * @param ptr A pointer to object of type T
   * @param value The value of type T to set.
   */
  template <typename T>
  void update(T* ptr, T value) {
    std::lock_guard<std::mutex> lck(_stats_m);
    *ptr = std::move(value);
  }

  /**
   * @brief Almost the same function as in the previous case, but with a
   * Timestamp object. And we can directly set a time_t value.
   *
   * @param ptr A pointer to object of type Timestamp.
   * @param value The value of type time_t to set.
   */
  // void update(google::protobuf::Timestamp* ptr, time_t value) {
  //  _strand.post([ptr, value] {
  //    ptr->Clear();
  //    ptr->set_seconds(value);
  //  });
  //}

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
  void update(void (U::*f)(T), U* ptr, T value) {
    std::lock_guard<std::mutex> lck(_stats_m);
    (ptr->*f)(value);
  }

  void execute(std::function<void()>&& f) {
    std::lock_guard<std::mutex> lck(_stats_m);
    f();
  }

  template <typename U, typename T>
  const T& get(T (U::*f)() const, const U* ptr) {
    std::lock_guard<std::mutex> lck(_stats_m);
    return (ptr->*f)();
  }
};

}  // namespace stats

CCB_END()

#endif /* !CCB_STATS_CENTER_HH */
