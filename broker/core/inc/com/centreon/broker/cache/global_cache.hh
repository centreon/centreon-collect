/*
** Copyright 2022 Centreon
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

#ifndef CCB_GLOBAL_CACHE_HH
#define CCB_GLOBAL_CACHE_HH

#include "bbdo/tag.pb.h"

namespace com::centreon::broker {

namespace cache {

namespace interprocess = boost::interprocess;

/**
 * @brief as allocation is yet protected by _protect mutex, we don't need to
 * protect allocation alogorithm (interprocess::null_mutex_family)
 *
 */
using managed_mapped_file = interprocess::basic_managed_mapped_file<
    char,
    interprocess::rbtree_best_fit<interprocess::null_mutex_family>,
    interprocess::iset_index>;
using segment_manager = managed_mapped_file::segment_manager;
using char_allocator = managed_mapped_file::allocator<char>::type;
using uint64_t_allocator = interprocess::allocator<uint64_t, segment_manager>;

// std::string can't be used in mapped memory => use boost::container::string
// with managed_mapped_file allocator
using string =
    interprocess::basic_string<char, std::char_traits<char>, char_allocator>;

using host_serv_pair = std::pair<uint64_t /*host_id*/, uint64_t /*serv_id*/>;

inline bool operator==(const string& left, const std::string_view& right) {
  return left.compare(0, right.length(), right.data()) == 0;
}

inline bool operator!=(const string& left, const std::string_view& right) {
  return left.compare(0, right.length(), right.data()) != 0;
}

struct metric_info {
  template <class string_view_class>
  metric_info(uint64_t i_index_id,
              const string_view_class& i_name,
              const string_view_class& i_unit,
              double i_min,
              double i_max,
              const char_allocator& char_alloc)
      : index_id(i_index_id),
        name(i_name.data(), i_name.length(), char_alloc),
        unit(i_unit.data(), i_unit.length(), char_alloc),
        min(i_min),
        max(i_max) {}

  uint64_t index_id;
  string name;
  string unit;
  double min;
  double max;
};

struct resource_info {
  resource_info(const char_allocator& char_alloc) : name(char_alloc) {}

  resource_info(const std::string_view& nam,
                uint64_t res_id,
                uint64_t sev_id,

                const char_allocator& char_alloc)
      : name(nam.data(), nam.length(), char_alloc),
        resource_id(res_id),
        severity_id(sev_id) {}

  string name;
  uint64_t resource_id;
  uint64_t severity_id;
};

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
 * When you insert data, you must catch interprocess::bad_allo and call
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
  size_t _file_size;

  void _open(size_t initial_size_on_create, const void* address = 0);
  void _grow(size_t new_size, void* address = 0);

 protected:
  const std::string _file_path;

  std::unique_ptr<managed_mapped_file> _file;

  mutable absl::Mutex _protect;

  global_cache(const std::string& file_path);

  void allocation_exception_handler();

  virtual void managed_map(bool create [[maybe_unused]]) {}

 public:
  using pointer = std::shared_ptr<global_cache>;

  static pointer load(const std::string& file_path,
                      size_t initial_size = 0x20000000,
                      const void* address = 0);

  static void unload();

  static pointer instance_ptr() { return _instance; }

  virtual ~global_cache();

  /**
   * @brief lock the object in read only
   * mandatory before using a getter
   *
   */
  class lock {
    absl::ReaderMutexLock _lock;

   public:
    lock();
  };

  // use only for tests
  const void* get_address() const;

  virtual void set_metric_info(uint64_t metric_id,
                               uint64_t index_id,
                               const std::string_view& name,
                               const std::string_view& unit,
                               double min,
                               double max) = 0;

  virtual void store_instance(uint64_t instance_id,
                              const std::string_view& instance_name) = 0;

  /**
   * @brief As we don't have access to the repeated TagInfo type, we fill
   * service and host tags ids with this function
   *
   */

  virtual void store_host(uint64_t host_id,
                          const std::string_view& host_name,
                          uint64_t resource_id,
                          uint64_t severity_id) = 0;

  virtual void store_service(uint64_t host_id,
                             uint64_t service_id,
                             const std::string_view& service_description,
                             uint64_t resource_id,
                             uint64_t severity_id) = 0;

  virtual void set_index_mapping(uint64_t index_id,
                                 uint64_t host_id,
                                 uint64_t service_id) = 0;

  virtual const metric_info* get_metric_info(uint32_t metric_id) const = 0;

  virtual const resource_info* get_host(uint64_t host_id) const = 0;
  virtual const resource_info* get_host_from_index_id(
      uint64_t index_id) const = 0;
  virtual const resource_info* get_service(uint64_t host_id,
                                           uint64_t service_id) const = 0;
  virtual const resource_info* get_service_from_index_id(
      uint64_t index_id) const = 0;

  virtual const host_serv_pair* get_host_serv_id(uint64_t index_id) const = 0;

  virtual const string* get_instance_name(uint64_t instance_id) const = 0;

  virtual void add_host_to_group(uint64_t group,
                                 uint64_t host,
                                 uint64_t poller_id) = 0;
  virtual void remove_host_from_group(uint64_t group, uint64_t host) = 0;
  virtual void remove_host_group_members(uint64_t group,
                                         uint64_t poller_id) = 0;

  virtual void add_service_to_group(uint64_t group,
                                    uint64_t host,
                                    uint64_t service,
                                    uint64_t poller_id) = 0;
  virtual void remove_service_from_group(uint64_t group,
                                         uint64_t host,
                                         uint64_t service) = 0;
  virtual void remove_service_group_members(uint64_t group,
                                            uint64_t poller_id) = 0;

  virtual void append_service_group(uint64_t host,
                                    uint64_t service,
                                    std::ostream& request_body) = 0;
  virtual void append_host_group(uint64_t host, std::ostream& request_body) = 0;

  virtual void add_tag(uint64_t tag_id,
                       const std::string_view& tag_name,
                       TagType tag_type,
                       uint64_t poller_id) = 0;

  virtual void remove_tag(uint64_t tag_id) = 0;

  using tag_id_enumerator = std::function<uint64_t()>;
  virtual void set_host_tag(uint64_t host, tag_id_enumerator&& tag_filler) = 0;
  virtual void set_serv_tag(uint64_t host,
                            uint64_t serv,
                            tag_id_enumerator&& tag_filler) = 0;

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
};

};  // namespace cache

}  // namespace com::centreon::broker

#endif
