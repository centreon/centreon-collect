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
#include "com/centreon/broker/cache/protobuf.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "protobuf_utils.hh"

namespace com::centreon::broker {

namespace cache {

class host;
class service;
class instance;
class host_group;
class service_group;
class dimension_ba_event;
class dimension_bv_event;

using host_serv_pair = std::pair<uint64_t /*host_id*/, uint64_t /*serv_id*/>;

inline bool operator==(const string& left, const std::string_view& right) {
  return left.compare(0, right.length(), right.data()) == 0;
}

inline bool operator!=(const string& left, const std::string_view& right) {
  return left.compare(0, right.length(), right.data()) != 0;
}

/**
 * @brief Singleton giving access to a persistent, memory-mapped cache of
 * broker objects (hosts, services, groups, tags, BAM dimensions, …).
 *
 * ## Dual-cache design
 *
 * Two independent files are maintained side by side:
 *  - **conf cache** (`*.cnf`): updated only by configuration events (Host,
 *    Service, HostGroup, ServiceGroup, Instance, Tag, …). It is stable and
 *    survives crashes.
 *  - **real-time cache** (`*.rt`): updated by all events, including status
 *    updates. If a crash is detected (dirty flag set), this file is discarded
 *    and rebuilt from the conf cache on the next access.
 *
 * `instance_ptr()` always returns the **real-time** cache. The conf cache is
 * an internal implementation detail; external callers never access it directly.
 * All public getters transparently fall back to the conf cache when the RT
 * cache does not yet have the requested object (e.g. after a crash recovery).
 *
 * ## Thread safety
 *
 * All memory lives inside a mapped file segment. The segment may be remapped
 * (moved in virtual address space) when it grows, so any pointer into the
 * segment becomes invalid after a grow. To protect against this:
 *
 *  - Acquire a `global_cache::lock` (shared or upgrade state) before calling *
 * any **getter** and hold it for as long as you use the returned pointer. *  *
 * - Use `lock` in upgrade state before calling `get_host`, `get_service` or
 *    `get_instance` — these may promote to a write lock internally to copy
 *    data from the conf cache into the RT cache.
 *  - Never hold a lock while calling `write()`.
 *
 * Example — read-only getter:
 * @code {.cpp}
 * {
 *   global_cache::lock l;
 *   const cache::host* h =
 *       global_cache::instance_ptr()->get_host(host_id, l);
 *   if (h) {
 *     // use h here, while l is still in scope
 *   }
 * }  // lock released, h must not be used after this point
 * @endcode
 *
 * ## Growing the mapped file
 *
 * When an allocation inside the segment fails, catch
 * `interprocess::bad_alloc` **outside** any lock and call
 * `allocation_exception_handler()`, then retry the operation:
 * @code {.cpp}
 * try {
 *   boost::unique_lock l(_protect);
 *   _some_map->emplace(...);
 * } catch (const interprocess::bad_alloc&) {
 *   SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
 *   allocation_exception_handler();   // acquires its own lock internally
 *   retry_the_operation();
 * }
 * @endcode
 *
 * ## Dirty flag
 *
 * A boolean `dirty` object is persisted inside the mapped file. It is set to
 * `true` on every modification and cleared only by an explicit `flush()`. On
 * startup, if the flag is found `true` the file was not closed gracefully
 * (crash) and is discarded and recreated from scratch.
 */
class global_cache : public std::enable_shared_from_this<global_cache> {
 private:
  static std::shared_ptr<global_cache> _instance ABSL_GUARDED_BY(_instance_m);
  static absl::Mutex _instance_m;
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
  const std::shared_ptr<global_cache> _conf_cache;
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
                   std::chrono::seconds(5));

  void _open(size_t initial_size_on_create, const void* address = 0);

  void allocation_exception_handler();

  virtual void managed_map(bool create);

  void _set_dirty_and_increment_modif() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

  void _start_save_timer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);
  void _save_timer_handler(const boost::system::error_code& err);

  void _flush() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

  virtual void _write_impl(const std::shared_ptr<io::data>& d,
                           bool first_attempt) = 0;

 public:
  using pointer = std::shared_ptr<global_cache>;

  static pointer load(const std::shared_ptr<asio::io_context> io_context,
                      const std::string& file_path,
                      size_t initial_size = 0x10000000,
                      const void* address = 0,
                      unsigned grow_step = 0x10000000,
                      unsigned nb_update_before_save = 1000,
                      std::chrono::system_clock::duration save_interval =
                          std::chrono::seconds(10));

  static void unload();

  void stop();

  static pointer instance_ptr();

  virtual ~global_cache();

  /**
   * @brief RAII lock token for the global_cache mutex.
   *
   * Unlike a traditional RAII lock, no lock is acquired on construction.
   * The lock is acquired by the cache itself during getter calls and can
   * transition between states (shared → upgrade → unique) without being
   * released.  The destructor releases whichever lock state is currently held.
   *
   * States:
   *  - no      : not locked.
   *  - shared  : multiple readers allowed, no writer.
   *  - upgrade : one upgrade holder + multiple readers; can be promoted to
   *              unique without releasing.
   *  - unique  : exclusive write lock.
   *
   * WARNING: a single lock instance must not be used concurrently from
   * multiple threads.
   *
   */
  class lock {
    enum class e_state : unsigned { no = 0, shared, upgrade, unique };

    e_state _state = e_state::no;

    boost::upgrade_mutex* _mut = nullptr;

    void upgrade_lock(boost::upgrade_mutex* mut);
    void shared_lock(boost::upgrade_mutex* mut);
    void unique(boost::upgrade_mutex* mut);
    void upgrade_to_unique();
    void unique_to_shared();

   public:
    friend class global_cache;
    friend class global_cache_data;

    ~lock();
  };

  // use only for tests
  const void* get_address() const;

  void write(const std::shared_ptr<io::data>& d) { _write_impl(d, true); }

  virtual const host* get_host(uint64_t host_id, lock& l) = 0;
  virtual const service* get_service(uint64_t host_id,
                                     uint64_t service_id,
                                     lock& l) = 0;

  virtual std::optional<host_serv_pair> get_host_serv_id(uint64_t index_id) = 0;

  virtual const instance* get_instance(uint64_t instance_id, lock& l) = 0;

  virtual const host_group* get_host_group(uint64_t group_id,
                                           lock& l) const = 0;
  virtual const service_group* get_service_group(uint64_t group_id,
                                                 lock& l) const = 0;
  virtual void append_service_group(uint64_t host,
                                    uint64_t service,
                                    std::ostream& request_body) const = 0;
  virtual void append_host_group(uint64_t host,
                                 std::ostream& request_body) const = 0;
  virtual void append_host_tag_id(uint64_t host,
                                  TagType tag_type,
                                  std::ostream& request_body) const = 0;
  virtual void append_serv_tag_id(uint64_t host,
                                  uint64_t serv,
                                  TagType tag_type,
                                  std::ostream& request_body) const = 0;
  virtual void append_host_tag_name(uint64_t host,
                                    TagType tag_type,
                                    std::ostream& request_body) const = 0;
  virtual void append_serv_tag_name(uint64_t host,
                                    uint64_t serv,
                                    TagType tag_type,
                                    std::ostream& request_body) const = 0;

  virtual uint64_t get_index_id_from_metric_id(uint64_t metric_id) const = 0;

  virtual std::optional<int32_t> get_severity(uint64_t host_id,
                                              uint64_t service_id) const = 0;

  virtual const dimension_ba_event* get_dimension_ba_event(uint64_t ba_id,
                                                           lock& l) const = 0;
  virtual const dimension_bv_event* get_dimension_bv_event(uint64_t bv_id,
                                                           lock& l) const = 0;
  using bv_enumerator = std::function<void(uint64_t)>;
  virtual void enumerate_bvs(uint64_t ba_id,
                             bv_enumerator&& enumerator) const = 0;

  using group_enumerator = std::function<void(uint64_t /* group_id */,
                                              const string& /* group_name */)>;
  virtual void enumerate_host_group(uint64_t host_id,
                                    group_enumerator&& enumerator) const = 0;
  virtual void enumerate_service_group(uint64_t host_id,
                                       uint64_t service_id,
                                       group_enumerator&& enumerator) const = 0;
};

};  // namespace cache

}  // namespace com::centreon::broker

#endif
