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

#include <boost/stacktrace.hpp>

#include <fmt/format.h>

#include "by_thread_trace_active.hh"
#include "funct_info_cache.hh"
#include "simply_allocator.hh"

using namespace com::centreon::malloc_trace;

extern void* __libc_malloc(size_t size);

pid_t gettid() __attribute__((weak));

/**
 * @brief gettid is not available on alma8
 *
 * @return pid_t
 */
pid_t m_gettid() {
  if (gettid) {
    return gettid();
  } else {
    return syscall(__NR_gettid);
  }
}

static thread_dump_active _thread_dump_active;

static std::mutex _write_m;

/**
 * @brief this function open out file if it hasn't be done
 * if file size exceed 256Mo, file is truncated
 *
 * @return int file descriptor
 */
static int open_file() {
  static const char* out_file_path = nullptr;
  if (!out_file_path) {
    char* env_out_file_path = getenv("out_file_path");
    if (env_out_file_path && strlen(env_out_file_path) > 0)
      out_file_path = env_out_file_path;

    else
      out_file_path = "/tmp/malloc-trace.csv";
  }
  static unsigned long long max_file_size = 0;
  if (!max_file_size) {
    char* env_out_file_max_size = getenv("out_file_max_size");
    if (env_out_file_max_size && atoll(env_out_file_max_size) > 0)
      max_file_size = atoll(env_out_file_max_size);
    else
      max_file_size = 0x100000000;
  }

  std::lock_guard l(_write_m);
  static int out_file = -1;
  if (out_file < 0) {
    out_file =
        open(out_file_path, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  } else {
    struct stat file_stat;
    if (!fstat(out_file, &file_stat)) {
      struct stat real_file_stat;
      if (stat(out_file_path, &real_file_stat) ||
          real_file_stat.st_ino !=
              file_stat.st_ino) {  // file has been moved or deleted
        if (out_file > 0)
          close(out_file);
        out_file =
            open(out_file_path, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
      }
      if (file_stat.st_size > max_file_size) {
        ftruncate(out_file, 0);
      }
    }
  }
  return out_file;
}

/**
 * @brief append a log in out file
 * backtrace size is 10 buy default, this can be modified by max_stack
 * environment variable
 *
 * @param p pointer returned by malloc or passed to free
 * @param size size allocated, 0 for free
 * @param funct_name malloc or free
 */
static void dump_callstack(void* p, size_t size, const char* funct_name) {
  int out_file = open_file();
  if (out_file < 0)
    return;
  unsigned max_stack;
  char* env_max_stack = getenv("max_stack");
  if (env_max_stack && !absl::SimpleAtoi(env_max_stack, &max_stack)) {
    max_stack = 10;
  }

  static funct_cache_map* _funct_cache = new funct_cache_map;
  static std::mutex funct_cache_m;

  constexpr unsigned size_buff = 0x40000;
  char buff[size_buff];
  char* end_buff = buff + size_buff - 10;
  *end_buff = 0;

  char* work_pos =
      fmt::format_to_n(buff, size_buff, "\"{}\";{};{};{};\"[", funct_name,
                       m_gettid(), reinterpret_cast<std::uintptr_t>(p), size)
          .out;

  boost::stacktrace::stacktrace call_stack;
  if (!call_stack.empty()) {
    unsigned stack_cpt = 0;
    auto stack_iter = call_stack.begin();
    ++stack_iter;
    // one array element by stack layer
    for (++stack_iter; stack_iter != call_stack.end() && stack_cpt < max_stack;
         ++stack_iter, ++stack_cpt) {
      if (end_buff - work_pos < 1000) {
        break;
      }

      if (stack_cpt) {
        *work_pos++ = ',';
      }

      const auto& stack_layer = *stack_iter;
      auto addr = stack_layer.address();
      funct_cache_map::const_iterator cache_entry;
      {
        std::unique_lock l(funct_cache_m);
        cache_entry = _funct_cache->find(addr);
        if (cache_entry == _funct_cache->end()) {
          l.unlock();
          funct_info to_insert(stack_layer.name(), stack_layer.source_file(),
                               stack_layer.source_line());
          l.lock();
          cache_entry = _funct_cache->emplace(addr, to_insert).first;
        }
      }

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
  std::lock_guard l(_write_m);
  ::write(out_file, buff, work_pos - buff);
}

static simply_allocator _first_malloc;

static void* first_malloc(size_t size) {
  return _first_malloc.malloc(size);
}

static void* first_realloc(void* p, size_t size) {
  return _first_malloc.realloc(p, size);
}

typedef void* (*malloc_signature)(size_t);

static malloc_signature original_malloc = first_malloc;

typedef void* (*realloc_signature)(void*, size_t);

static realloc_signature original_realloc = first_realloc;

static void first_free(void* p) {
  _first_malloc.free(p);
}

typedef void (*free_signature)(void*);
static free_signature original_free = first_free;

/**
 * @brief there is 3 stages
 * on the first alloc, we don't know malloc, realloc and free address
 * So we call dlsym to get these address
 * As dlsym allocate memory, we are in dlsym_running state and we provide
 * allocation mechanism by simply_allocator once dlsym are done, we are in hook
 * state
 *
 */
enum class e_library_state { not_hooked, dlsym_running, hooked };
static e_library_state _state = e_library_state::not_hooked;

static void search_symbols() {
  original_malloc =
      reinterpret_cast<malloc_signature>(dlsym(RTLD_NEXT, "malloc"));
  original_free = reinterpret_cast<free_signature>(dlsym(RTLD_NEXT, "free"));
  original_realloc =
      reinterpret_cast<realloc_signature>(dlsym(RTLD_NEXT, "realloc"));
}

/**
 * @brief our malloc
 *
 * @param size
 * @param funct_name  function name logged
 * @return void*
 */
static void* malloc(size_t size, const char* funct_name) {
  switch (_state) {
    case e_library_state::not_hooked:
      _state = e_library_state::dlsym_running;
      search_symbols();
      _state = e_library_state::hooked;
      break;
    case e_library_state::dlsym_running:
      return first_malloc(size);
    default:
      break;
  }
  void* p = original_malloc(size);
  bool have_to_dump = _thread_dump_active.set_dump_active(m_gettid());
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    dump_callstack(p, size, funct_name);
    _thread_dump_active.reset_dump_active(m_gettid());
  }
  return p;
}

/**
 * @brief our realloc function
 *
 * @param p
 * @param size
 * @return void*
 */
void* realloc(void* p, size_t size) {
  switch (_state) {
    case e_library_state::not_hooked:
      _state = e_library_state::dlsym_running;
      search_symbols();
      _state = e_library_state::hooked;
      break;
    case e_library_state::dlsym_running:
      return first_realloc(p, size);
    default:
      break;
  }
  void* new_p = original_realloc(p, size);
  bool have_to_dump = _thread_dump_active.set_dump_active(m_gettid());
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    if (new_p != p)
      dump_callstack(p, size, "free_realloc");
    dump_callstack(new_p, size, "realloc");
    _thread_dump_active.reset_dump_active(m_gettid());
  }
  return new_p;
}

/**
 * @brief replacement of the original malloc
 *
 * @param size
 * @return void*
 */
void* malloc(size_t size) {
  return malloc(size, "malloc");
}

/**
 * @brief our calloc function
 *
 * @param num
 * @param size
 * @return void*
 */
void* calloc(size_t num, size_t size) {
  size_t total_size = num * size;
  void* p = malloc(total_size, "calloc");
  memset(p, 0, total_size);
  return p;
}

/**
 * @brief our free
 *
 * @param p
 */
void free(void* p) {
  if (_first_malloc.free(p))
    return;
  original_free(p);
  bool have_to_dump = _thread_dump_active.set_dump_active(m_gettid());
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    dump_callstack(p, 0, "free");
    _thread_dump_active.reset_dump_active(m_gettid());
  }
}