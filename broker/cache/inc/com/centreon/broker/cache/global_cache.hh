/**
 * Copyright 2022-2024 Centreon
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

#ifndef CCB_GLOBAL_CACHE_HH
#define CCB_GLOBAL_CACHE_HH

#include <boost/thread/shared_mutex.hpp>
#include "com/centreon/broker/io/protobuf.hh"
#include "protobuf.hh"

namespace com::centreon::broker {

namespace cache {

using host_serv_pair = std::pair<uint64_t /*host_id*/, uint64_t /*serv_id*/>;

inline bool operator==(const string& left, const std::string_view& right) {
  return left.compare(0, right.length(), right.data()) == 0;
}

inline bool operator!=(const string& left, const std::string_view& right) {
  return left.compare(0, right.length(), right.data()) != 0;
}

/**
 * @brief this singleton is used to store many things in a mapped file
 * All memory stored in it are allocated in a memory segment mapped to a file
 * Sometimes file needs to be growned, so memory segment may be moved
 * So before getting data and during the result usage, you must aquire a shared
 * lock by creating a global_cache::lock object
 * example:
 * @code
 * global_cache::lock l;
 * const string * host_name =
 * global_cache::instance_ptr()->get_host_name("toto");
 * @endcode
 *
 *
 * this class is abstract, it only deals with mapping, data are stored in
 * global_cache_data
 *
 * When you insert data, you must catch interprocess::bad_alloc and call
 * allocation_exception_handler to grow file outside any lock
 * @code {.c++}
 * void global_cache_data::add_service_to_group(uint64_t group,
 *                                         uint64_t host,
 *                                         uint64_t service) {
 * try {
 *   absl::WriterMutexLock l(&_protect);
 *   _service_group->emplace(service_group_element{{host, service}, group});
 * } catch (const interprocess::bad_alloc& e) {
 *   SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
 *   allocation_exception_handler();
 *   add_service_to_group(group, host, service);
 * }
 *}
 * @endcode
 *
 *
 * a flag _dirty indicates if the file has been closed gracefully
 */
class global_cache : public std::enable_shared_from_this<global_cache> {
 private:
  static std::shared_ptr<global_cache> _instance;
  std::shared_ptr<asio::io_context> _io_context;
  asio::system_timer _save_timer ABSL_GUARDED_BY(_protect);
  const unsigned _grow_step;
  const unsigned _nb_update_before_save;
  const std::chrono::system_clock::duration _save_interval;
  size_t _file_size;

  bool* _dirty;
  unsigned _modif_counter;
  time_t _last_save_time;

  void _grow(size_t new_size, void* address = 0);

 protected:
  const std::string _file_path;
  /**
   * @brief in fact, we have two cache, one stable updated only by conf event
   * such as service, host, host_group..
   * And another that deals with all events.
   * So if a crash happens, real time one is corrupted and erased and will be
   * built from conf one each time we need data (like a service)
   */
  enum class e_cache_type { conf, real_time };
  std::shared_ptr<global_cache> _conf_cache;
  const e_cache_type _cache_type;

  std::shared_ptr<spdlog::logger> _logger;

  std::unique_ptr<managed_mapped_file> _file;

  mutable boost::upgrade_mutex _protect;

  global_cache(const std::shared_ptr<asio::io_context> io_context,
               const std::string& file_path,
               const std::shared_ptr<spdlog::logger>& logger,
               e_cache_type cache_type,
               const std::shared_ptr<global_cache>& conf_cache,
               unsigned grow_step = 0x20000000,
               unsigned nb_update_before_save = 1000,
               std::chrono::system_clock::duration save_interval =
                   std::chrono::seconds(10));

  void _open(size_t initial_size_on_create, const void* address = 0);

  void allocation_exception_handler();

  virtual void managed_map(bool create [[maybe_unused]]) {}

  void _set_dirty_and_increment_modif() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

  void _start_save_timer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);
  void _save_timer_handler(const boost::system::error_code& err);

  void _flush() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

 public:
  using pointer = std::shared_ptr<global_cache>;

  static pointer load(const std::shared_ptr<asio::io_context> io_context,
                      const std::string& file_path,
                      size_t initial_size = 0x20000000,
                      const void* address = 0,
                      unsigned grow_step = 0x20000000,
                      unsigned nb_update_before_save = 1000,
                      std::chrono::system_clock::duration save_interval =
                          std::chrono::seconds(10));

  static void unload();

  void stop();

  static pointer instance_ptr() { return _instance; }

  virtual ~global_cache();

  /**
   * @brief lock the object in read only getter
   * mandatory before using a getter to use except for get_host, get_service and
   * get_instance
   *
   */
  class lock {
    boost::shared_lock<boost::upgrade_mutex> _lock;

   public:
    lock();
    lock(global_cache* cache);
    lock(global_cache::pointer cache) : lock(cache.get()) {}
  };

  /**
   * @brief lock the object in read only getter
   * mandatory before using get_host, get_service and get_instance
   * When upgrade_lock is set, all other shared lock (lock object) can get
   * ownership but not other upgrade_locks
   * So use lock class for all other getters
   */
  class upgrade_lock {
    boost::upgrade_lock<boost::upgrade_mutex> _lock;
    friend class global_cache_data;

   public:
    upgrade_lock();
    upgrade_lock(global_cache* cache);
    upgrade_lock(global_cache::pointer cache) : upgrade_lock(cache.get()) {}
  };

  // use only for tests
  const void* get_address() const;

  virtual void write(const std::shared_ptr<io::data>& d) = 0;

  virtual const host* get_host(uint64_t host_id, upgrade_lock& l) = 0;
  virtual const service* get_service(uint64_t host_id,
                                     uint64_t service_id,
                                     upgrade_lock& l) = 0;

  virtual const host_serv_pair* get_host_serv_id(uint64_t index_id) = 0;

  virtual const instance* get_instance(uint64_t instance_id,
                                       upgrade_lock& l) = 0;

  virtual void append_service_group(uint64_t host,
                                    uint64_t service,
                                    std::ostream& request_body) = 0;
  virtual void append_host_group(uint64_t host, std::ostream& request_body) = 0;
  virtual void append_host_tag_id(uint64_t host,
                                  TagType tag_type,
                                  std::ostream& request_body) = 0;
  virtual void append_serv_tag_id(uint64_t host,
                                  uint64_t serv,
                                  TagType tag_type,
                                  std::ostream& request_body) = 0;
  virtual void append_host_tag_name(uint64_t host,
                                    TagType tag_type,
                                    std::ostream& request_body) = 0;
  virtual void append_serv_tag_name(uint64_t host,
                                    uint64_t serv,
                                    TagType tag_type,
                                    std::ostream& request_body) = 0;

  virtual uint64_t get_index_id_from_metric_id(uint64_t metric_id) = 0;

  virtual int32_t get_severity(const uint64_t host_id,
                               const uint64_t service_id) = 0;
};

};  // namespace cache

}  // namespace com::centreon::broker

#endif
