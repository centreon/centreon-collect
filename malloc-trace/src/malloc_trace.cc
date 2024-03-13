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

#include "by_thread_trace_active.hh"
#include "funct_info_cache.hh"
#include "orphan_container.hh"
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

/**
 * @brief when we enter in malloc or free, we store information of stack trace,
 * this will generate allocation that we don't want to store, this object allow
 * us to store the active tracing on the active thread in order to avoid store
 * recursion.
 *
 */
static thread_dump_active _thread_dump_active;

/**
 * @brief simply allocator used by dlsym
 *
 */
static simply_allocator _first_malloc;

/**
 * @brief the container that store every malloc and free
 *
 */
static orphan_container* _orphans = new orphan_container;

static void* first_malloc(size_t size) {
  return _first_malloc.malloc(size);
}

static void* first_realloc(void* p, size_t size) {
  return _first_malloc.realloc(p, size);
}

typedef void* (*malloc_signature)(size_t);

// will be filled by original malloc
static malloc_signature original_malloc = first_malloc;

typedef void* (*realloc_signature)(void*, size_t);

// will be filled by original realloc
static realloc_signature original_realloc = first_realloc;

static void first_free(void* p) {
  _first_malloc.free(p);
}

typedef void (*free_signature)(void*);
// will be filled by original free
static free_signature original_free = first_free;

/**
 * @brief there is 3 stages
 * on the first alloc, we don't know malloc, realloc and free address
 * So we call dlsym to get these address
 * As dlsym allocates memory, we are in dlsym_running state and we provide
 * allocation mechanism by simply_allocator.
 * Once dlsym are done, we are in hook state
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

  pid_t thread_id = m_gettid();
  bool have_to_dump = _thread_dump_active.set_dump_active(thread_id);

  // if this thread is not yet dumping => store it
  if (have_to_dump) {
    if (_orphans) {
      _orphans->add_malloc(p, size, thread_id, funct_name,
                           boost::stacktrace::stacktrace(), 2);
      _orphans->flush_to_file();
    }
    _thread_dump_active.reset_dump_active(thread_id);
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
  pid_t thread_id = m_gettid();
  bool have_to_dump = _thread_dump_active.set_dump_active(thread_id);
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    constexpr std::string_view realloc_funct_name("realloc");
    // if pointer has changed, we record a free
    if (new_p != p && p) {
      if (!_orphans->free(p)) {
        constexpr std::string_view free_funct_name("freerealloc");
        _orphans->add_free(p, thread_id, free_funct_name,
                           boost::stacktrace::stacktrace(), 2);
      }
    }
    _orphans->add_malloc(new_p, size, thread_id, realloc_funct_name,
                         boost::stacktrace::stacktrace(), 2);
    _orphans->flush_to_file();
    _thread_dump_active.reset_dump_active(thread_id);
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
 * call to malloc
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
  if (!p)
    return;

  pid_t thread_id = m_gettid();
  bool have_to_dump = _thread_dump_active.set_dump_active(thread_id);

  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    if (!_orphans->free(p)) {
      constexpr std::string_view free_funct_name("free");
      _orphans->add_free(p, thread_id, free_funct_name,
                         boost::stacktrace::stacktrace(), 2);
      _orphans->flush_to_file();
    }
    _thread_dump_active.reset_dump_active(thread_id);
  }
}
