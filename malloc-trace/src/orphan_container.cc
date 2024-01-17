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

#include <fmt/format.h>
#include <boost/stacktrace.hpp>

#include "orphan_container.hh"

using namespace com::centreon::malloc_trace;

/**
 * @brief Construct a new backtrace info::backtrace info object
 *
 * @param allocated address allocated or freed
 * @param allocated_size size allocated
 * @param thread_id
 * @param funct_name malloc, free or freerealloc
 * @param backtrace
 * @param backtrace_offset we will ignore backtrace_offset first frames
 */
backtrace_info::backtrace_info(const void* allocated,
                               size_t allocated_size,
                               pid_t thread_id,
                               const std::string_view& funct_name,
                               const boost::stacktrace::stacktrace& backtrace,
                               size_t backtrace_offset)
    : _allocated(allocated),
      _allocated_size(allocated_size),
      _thread_id(thread_id),
      _funct_name(funct_name),
      _backtrace_size(0),
      _last_allocated(std::chrono::system_clock::now()) {
  if (backtrace_offset >= backtrace.size()) {
    return;
  }

  boost::stacktrace::stacktrace::const_iterator frame_iter = backtrace.begin();
  std::advance(frame_iter, backtrace_offset);
  for (; frame_iter != backtrace.end() && _backtrace_size < max_backtrace_size;
       ++frame_iter) {
    _backtrace[_backtrace_size++] = frame_iter->address();
  }
}

/**
 * @brief add a line in output file
 *
 * @param fd  file descriptor
 * @param funct_info_cache  map address=>function info   where we store infos of
 * functions (name, source line)
 */
void backtrace_info::to_file(int fd, funct_cache_map& funct_info_cache) const {
  if (fd <= 0) {
    return;
  }
  constexpr unsigned size_buff = 0x40000;
  char buff[size_buff];
  char* end_buff = buff + size_buff - 10;
  *end_buff = 0;

  char* work_pos =
      fmt::format_to_n(buff, size_buff, "\"{}\";{};{};{};{};\"[", _funct_name,
                       _thread_id, reinterpret_cast<std::uintptr_t>(_allocated),
                       _allocated_size,
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           _last_allocated.time_since_epoch())
                           .count())
          .out;

  for (unsigned stack_cpt = 0; stack_cpt < _backtrace_size; ++stack_cpt) {
    const boost::stacktrace::frame::native_frame_ptr_t addr =
        _backtrace[stack_cpt];
    boost::stacktrace::frame frame(addr);
    if (end_buff - work_pos < 1000) {
      break;
    }

    if (stack_cpt) {
      *work_pos++ = ',';
    }
    funct_cache_map::const_iterator cache_entry = funct_info_cache.find(addr);
    if (cache_entry == funct_info_cache.end()) {  // not found => search and
                                                  // save
      funct_info to_insert(frame.name(), frame.source_file(),
                           frame.source_line());
      cache_entry = funct_info_cache.emplace(addr, to_insert).first;
    }

    if (cache_entry->second.get_source_file().empty()) {
      work_pos = fmt::format_to_n(work_pos, end_buff - work_pos,
                                  "{{\\\"f\\\":\\\"{}\\\" }}",
                                  cache_entry->second.get_funct_name())
                     .out;
    } else {
      work_pos =
          fmt::format_to_n(
              work_pos, end_buff - work_pos,
              "{{\\\"f\\\":\\\"{}\\\" , \\\"s\\\":\\\"{}\\\" , \\\"l\\\":{}}}",
              cache_entry->second.get_funct_name(),
              cache_entry->second.get_source_file(),
              cache_entry->second.get_source_line())
              .out;
    }
  }

  *work_pos++ = ']';
  *work_pos++ = '"';
  *work_pos++ = '\n';

  ::write(fd, buff, work_pos - buff);
}

/*********************************************************************************
   orphan_container
*********************************************************************************/

/**
 * @brief Construct a new orphan container::orphan container object
 *
 */
orphan_container::orphan_container()
    : _malloc_allocator(std::allocator<uint8_t>()),
      _free_allocator(std::allocator<uint8_t>()) {
  char* env_out_file_max_size = getenv("out_file_max_size");
  if (env_out_file_max_size && atoll(env_out_file_max_size) > 0)
    _max_file_size = atoll(env_out_file_max_size);
  else
    _max_file_size = 0x100000000;

  char* env_out_file_path = getenv("out_file_path");
  if (env_out_file_path && strlen(env_out_file_path) > 0)
    _out_file_path = env_out_file_path;

  else
    _out_file_path = "/tmp/malloc-trace.csv";

  char* malloc_second_peremption = getenv("malloc_second_peremption");
  if (malloc_second_peremption && atoi(malloc_second_peremption) > 0)
    _malloc_peremption = std::chrono::seconds(atoi(malloc_second_peremption));
  else
    _malloc_peremption = std::chrono::minutes(1);
}

/**
 * @brief register a malloc action, it can be a malloc or a realloc
 *
 * @param addr  address allocated
 * @param allocated_size size allocated
 * @param thread_id
 * @param funct_name
 * @param backtrace
 * @param backtrace_offset we will ignore backtrace_offset first frames
 */
void orphan_container::add_malloc(
    const void* addr,
    size_t allocated_size,
    pid_t thread_id,
    const std::string_view& funct_name,
    const boost::stacktrace::stacktrace& backtrace,
    size_t backtrace_offset) {
  std::lock_guard l(_protect);
  orphan_malloc* new_node = _malloc_allocator.allocate();
  new (new_node) orphan_malloc(addr, allocated_size, thread_id, funct_name,
                               backtrace, backtrace_offset);

  if (!_address_to_malloc.insert(*new_node).second) {
    _malloc_allocator.deallocate(new_node);
  } else {
    _time_to_malloc.insert(*new_node);
  }
}

/**
 * @brief when program call free we try to unregister previous malloc at addr
 *
 * @param addr address to free
 * @return true malloc was found and unregistered
 * @return false  no malloc found for this address
 */
bool orphan_container::free(const void* addr) {
  std::lock_guard l(_protect);
  auto found = _address_to_malloc.find(addr);
  if (found != _address_to_malloc.end()) {
    orphan_malloc& to_erase = *found;

    //_time_to_malloc is only indexed by alloc timestamp (ms)
    auto where_to_search =
        _time_to_malloc.equal_range(to_erase.get_last_allocated());
    for (; where_to_search.first != where_to_search.second;
         ++where_to_search.first) {
      if (&*where_to_search.first == &to_erase) {
        _time_to_malloc.erase(where_to_search.first);
        break;
      }
    }

    _address_to_malloc.erase(found);
    _malloc_allocator.deallocate(&to_erase);
    return true;
  } else {
    return false;
  }
}

/**
 * @brief in case or free has returned false, we have to add free orphan in this
 * container
 *
 * @param addr  address freed
 * @param thread_id
 * @param funct_name
 * @param backtrace
 * @param backtrace_offset we will ignore backtrace_offset first frames
 */
void orphan_container::add_free(const void* addr,
                                pid_t thread_id,
                                const std::string_view& funct_name,
                                const boost::stacktrace::stacktrace& backtrace,
                                size_t backtrace_offset) {
  std::lock_guard l(_protect);
  orphan_free* new_free = _free_allocator.allocate();
  new (new_free)
      orphan_free(addr, thread_id, funct_name, backtrace, backtrace_offset);
  _free.push_back(*new_free);
}

/**
 * @brief flush contents to disk
 * all malloc older than _malloc_peremption are flushed
 * more recent mallocs are not flushed because we hope a free that will
 * unregister its all orphan free are flushed all data flushed are remove from
 * container
 *
 */
void orphan_container::flush_to_file() {
  std::lock_guard l(_protect);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (_last_flush + _malloc_peremption > now) {
    return;
  }

  int fd = open_file();

  _last_flush = now;

  // we flush to disk oldest malloc and remove its from the container
  if (!_time_to_malloc.empty()) {
    orphan_malloc_time_set::iterator upper = _time_to_malloc.upper_bound(
        std::chrono::system_clock::now() - _malloc_peremption);
    for (orphan_malloc_time_set::iterator to_flush = _time_to_malloc.begin();
         to_flush != upper; ++to_flush) {
      to_flush->to_file(fd, _funct_info_cache);
      _address_to_malloc.erase(to_flush->get_allocated());
    }
    _time_to_malloc.erase_and_dispose(
        _time_to_malloc.begin(), upper, [this](orphan_malloc* to_dispose) {
          _malloc_allocator.deallocate(to_dispose);
        });
  }

  // we flush all free
  for (const orphan_free& to_flush : _free) {
    to_flush.to_file(fd, _funct_info_cache);
  }
  _free.clear_and_dispose([this](orphan_free* to_dispose) {
    _free_allocator.deallocate(to_dispose);
  });
  ::close(fd);
}

/**
 * @brief this function open out file if it hasn't be done
 * if file size exceed 256Mo, file is truncated
 *
 * @return int file descriptor
 */
int orphan_container::open_file() {
  int out_file_fd = open(_out_file_path.data(), O_APPEND | O_CREAT | O_RDWR,
                         S_IRUSR | S_IWUSR);
  if (out_file_fd > 0) {
    struct stat file_stat;
    if (!fstat(out_file_fd, &file_stat)) {
      if (file_stat.st_size > _max_file_size) {
        ftruncate(out_file_fd, 0);
      }
    }
  }
  return out_file_fd;
}
