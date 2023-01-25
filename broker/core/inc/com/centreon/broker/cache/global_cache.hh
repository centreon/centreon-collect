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

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace cache {

namespace interprocess = boost::interprocess;

using segment_manager = interprocess::managed_mapped_file::segment_manager;
using char_allocator = interprocess::allocator<char, segment_manager>;
// std::string can't be used in mapped memory => use boost::container::string
// with managed_mapped_file allocator
using string = boost::interprocess::
    basic_string<char, std::char_traits<char>, char_allocator>;

inline bool operator==(const string& left, const absl::string_view& right) {
  return left.compare(0, right.length(), right.data()) == 0;
}

inline bool operator!=(const string& left, const absl::string_view& right) {
  return left.compare(0, right.length(), right.data()) != 0;
}

struct metric_info {
  uint64_t index_id;
  const string* name;
  const string* unit;
  uint64_t host_id;
  const string* host_name;
  uint64_t service_id;
  const string* service_description;
  double min;
  double max;
};

/**
 * @brief this singleton is used to store many things in a mapped file
 * All memory stored in it are allocated in a memory segment mapped to a file
 * Sometimes file needs to be growned, so memory segment may be moved
 * So before getting data, you must aquire a shared lock by creating a
 * global_cache::lock object
 *
 * this class is abstract, it only deals with mapping, data are stored in
 * global_cache_data
 *
 * When you insert data, you must catch interprocess::bad_allo and call
 * allocation_exception_handler to grow file outside any lock
 */
class global_cache {
 protected:
  std::string _file_path;
  size_t _file_size;

  std::unique_ptr<interprocess::managed_mapped_file> _file;

  mutable absl::Mutex _protect;

  static std::shared_ptr<global_cache> _instance;

  global_cache(const std::string& file_path);

  void open(size_t initial_size_on_create, const void* address = 0);

  void grow(size_t new_size, void* address = 0);

  void allocation_exception_handler();

  uint64_t calc_checksum() const;

  void write_checksum(uint64_t checksum) const;
  uint64_t read_checksum() const;

  virtual void managed_map(bool create){};

 public:
  using pointer = std::shared_ptr<global_cache>;

  static pointer load(const std::string& file_path,
                      size_t initial_size = 0x10000000,
                      const void* address = 0);

  static pointer instance() { return _instance; }

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
                               const absl::string_view& name,
                               const absl::string_view& unit,
                               double min,
                               double max) = 0;

  virtual void store_host(uint64_t host_id,
                          const absl::string_view& host_name) = 0;

  virtual void store_service(uint64_t host_id,
                             uint64_t service_id,
                             const absl::string_view& service_description) = 0;

  virtual void set_index_mapping(uint64_t index_id,
                                 uint64_t host_id,
                                 uint64_t service_id);

  virtual const metric_info* get_metric_info(uint32_t metric_id) const = 0;
};

};  // namespace cache

CCB_END()

#endif
