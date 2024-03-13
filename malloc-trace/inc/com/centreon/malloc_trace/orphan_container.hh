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

#ifndef CMT_ORPHAN_CONTAINER_HH
#define CMT_ORPHAN_CONTAINER_HH

#include <com/centreon/common/node_allocator.hh>

#include "funct_info_cache.hh"

namespace com::centreon::malloc_trace {

constexpr size_t max_backtrace_size = 15;

/**
 * @brief information of free or malloc with stacktrace
 * In this bean, we store:
 *  - allocated address
 *  - size of memory allocated (0 if free)
 *  - thread id
 *  - function name: malloc, free, realloc or freerealloc
 *  - length of the backtrace
 *  - backtrace
 *  - timestamp
 */
class backtrace_info {
  const void* _allocated;
  const size_t _allocated_size;
  const pid_t _thread_id;
  const std::string_view _funct_name;
  size_t _backtrace_size;
  boost::stacktrace::frame::native_frame_ptr_t _backtrace[max_backtrace_size];
  const std::chrono::system_clock::time_point _last_allocated;

 public:
  backtrace_info(const void* allocated,
                 size_t allocated_size,
                 pid_t thread_id,
                 const std::string_view& funct_name,
                 const boost::stacktrace::stacktrace& backtrace,
                 size_t backtrace_offset);

  const void* get_allocated() const { return _allocated; }
  size_t get_allocated_size() const { return _allocated_size; }
  size_t get_backtrace_size() const { return _backtrace_size; }
  const boost::stacktrace::frame::native_frame_ptr_t* get_backtrace() const {
    return _backtrace;
  }
  const std::chrono::system_clock::time_point& get_last_allocated() const {
    return _last_allocated;
  }

  void to_file(int fd, funct_cache_map& funct_info_cache) const;
};

/**
 * @brief infos of malloc
 * as this object is stored in 2 containers, it has to set hooks:
 * set_base_hook and allocated_time_hook
 *
 */
class orphan_malloc : public backtrace_info,
                      public boost::intrusive::set_base_hook<> {
 public:
  boost::intrusive::set_member_hook<> allocated_time_hook;

  orphan_malloc(const void* allocated,
                size_t allocated_size,
                pid_t thread_id,
                const std::string_view& funct_name,
                const boost::stacktrace::stacktrace& backtrace,
                size_t backtrace_offset)
      : backtrace_info(allocated,
                       allocated_size,
                       thread_id,
                       funct_name,
                       backtrace,
                       backtrace_offset) {}

  // key extractor used to create a map addr to orphan_malloc
  struct address_extractor {
    using type = const void*;
    type operator()(const orphan_malloc& node) const {
      return node.get_allocated();
    }
  };

  // key extractor used to create a map time_alloc to orphan_malloc
  struct time_allocated_extractor {
    using type = std::chrono::system_clock::time_point;
    const type& operator()(const orphan_malloc& node) const {
      return node.get_last_allocated();
    }
  };
};

/**
 * @brief infos of a free
 * this object is stored in single link list: slist_base_hook
 *
 */
class orphan_free : public backtrace_info,
                    public boost::intrusive::slist_base_hook<> {
 public:
  orphan_free(const void* allocated,
              pid_t thread_id,
              const std::string_view& funct_name,
              const boost::stacktrace::stacktrace& backtrace,
              size_t backtrace_offset)
      : backtrace_info(allocated,
                       0,
                       thread_id,
                       funct_name,
                       backtrace,
                       backtrace_offset) {}
};

/**
 * @brief this object contains all orphan mallocs (malloc without free) and all
 * orphan frees In order to limit allocation and improve performance, all
 * objects are stored in intrusive container and allocated in simple node
 * allocator (allocator that doesn't allow to allocate multiple object at once)
 *
 */
class orphan_container {
  // malloc part
  // map alloc adress => orphan_malloc
  using orphan_malloc_address_set = boost::intrusive::set<
      orphan_malloc,
      boost::intrusive::key_of_value<orphan_malloc::address_extractor>>;

  // map time alloc => orphan_malloc
  using orphan_malloc_time_hook =
      boost::intrusive::member_hook<orphan_malloc,
                                    boost::intrusive::set_member_hook<>,
                                    &orphan_malloc::allocated_time_hook>;
  using orphan_malloc_time_set = boost::intrusive::multiset<
      orphan_malloc,
      orphan_malloc_time_hook,
      boost::intrusive::key_of_value<orphan_malloc::time_allocated_extractor>>;

  // node allocator used to create orphan_malloc
  using orphan_malloc_allocator = com::centreon::common::
      node_allocator<orphan_malloc, std::allocator<uint8_t>, 0x100000>;

  orphan_malloc_address_set _address_to_malloc;
  orphan_malloc_time_set _time_to_malloc;
  orphan_malloc_allocator _malloc_allocator;

  // free part
  // orphan_free are stored in single linked list
  using orphan_free_list =
      boost::intrusive::slist<orphan_free, boost::intrusive::cache_last<true>>;

  // node allocator used to create orphan_free
  using orphan_free_allocator = com::centreon::common::
      node_allocator<orphan_free, std::allocator<uint8_t>, 0x100000>;

  orphan_free_list _free;
  orphan_free_allocator _free_allocator;

  funct_cache_map _funct_info_cache;

  std::chrono::system_clock::duration _malloc_peremption;
  std::chrono::system_clock::time_point _last_flush;
  size_t _max_file_size;
  std::string_view _out_file_path;

  mutable std::mutex _protect;

  int open_file();

 public:
  orphan_container();

  void add_malloc(const void* addr,
                  size_t allocated_size,
                  pid_t thread_id,
                  const std::string_view& funct_name,
                  const boost::stacktrace::stacktrace& backtrace,
                  size_t backtrace_offset);

  bool free(const void* addr);

  void add_free(const void* addr,
                pid_t thread_id,
                const std::string_view& funct_name,
                const boost::stacktrace::stacktrace& backtrace,
                size_t backtrace_offset);

  void flush_to_file();
};

}  // namespace com::centreon::malloc_trace

#endif
