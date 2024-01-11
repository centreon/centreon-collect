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

#include <sys/syscall.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <mutex>

#include <absl/strings/numbers.h>
#include <boost/intrusive/set.hpp>
#include <boost/stacktrace.hpp>

#include <fmt/format.h>

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
 * @brief The goal of this class is to provide map without allocation
 *
 * @tparam node_type node (key and data) that must inherit from
 * boost::intrusive::set_base_hook<>
 * @tparam key_extractor struct with an operator that extract key from node_type
 * @tparam node_arrray_size  size max of the container
 */
template <class node_type, class key_extractor, size_t node_array_size>
class intrusive_map {
 public:
  using key_type = typename key_extractor::type;

 private:
  node_type _nodes_array[node_array_size];
  node_type* _free_node;
  const node_type* _array_end;

  using node_map =
      boost::intrusive::set<node_type,
                            boost::intrusive::key_of_value<key_extractor> >;

  node_map _nodes;

 public:
  intrusive_map() {
    _free_node = _nodes_array;
    _array_end = _free_node + node_array_size;
  }

  const node_type* find(const key_type& key) const {
    auto found = _nodes.find(key);
    if (found == _nodes.end()) {
      return nullptr;
    } else {
      return &*found;
    }
  }

  node_type* insert_and_get(const key_type& key) {
    if (_free_node == _array_end) {
      return nullptr;
    }

    node_type* to_insert = _free_node++;
    new (to_insert) node_type(key);
    _nodes.insert(*to_insert);
    return to_insert;
  }
};

/**
 * @brief This class is used to store the tracing of a thread
 * if trace is active we don't log stacktrace
 *
 */
class thread_trace_active : public boost::intrusive::set_base_hook<> {
  pid_t _thread_id;
  mutable bool _malloc_trace_active;

 public:
  thread_trace_active() {}
  thread_trace_active(pid_t thread_id) : _thread_id(thread_id) {}

  pid_t get_thread_id() const { return _thread_id; }

  bool set_malloc_trace_active() const {
    if (_malloc_trace_active) {
      return true;
    }
    _malloc_trace_active = true;
    return false;
  }

  bool reset_malloc_trace_active() const {
    if (!_malloc_trace_active) {
      return false;
    }
    _malloc_trace_active = false;
    return true;
  }

  bool is_malloc_trace_active() const { return _malloc_trace_active; }

  struct key_extractor {
    using type = pid_t;
    type operator()(const thread_trace_active& node) const {
      return node._thread_id;
    }
  };
};

class thread_dump_active
    : protected intrusive_map<thread_trace_active,
                              thread_trace_active::key_extractor,
                              4096> {
  std::mutex _protect;

 public:
  bool set_dump_active();
  void reset_dump_active();
};

/**
 * @brief Set the flag to true in _by_thread_dump_active
 *
 * @return true the flag was not setted before call
 * @return false the flag was yet setted before call
 */
bool thread_dump_active::set_dump_active() {
  pid_t thread_id = m_gettid();
  std::lock_guard l(_protect);
  const thread_trace_active* exist = find(thread_id);

  if (!exist) {
    thread_trace_active* inserted = insert_and_get(thread_id);
    if (!inserted) {
      return false;
    }
    inserted->set_malloc_trace_active();
    return true;
  } else {
    return !exist->set_malloc_trace_active();
  }
}

void thread_dump_active::reset_dump_active() {
  pid_t thread_id = m_gettid();
  std::lock_guard l(_protect);
  const thread_trace_active* exist = find(thread_id);
  if (exist) {
    exist->reset_malloc_trace_active();
  }
}

static thread_dump_active _thread_dump_active;

/**
 * @brief symbol information research is very costly
 * so we store function informations in a cache
 *
 */
class funct_info {
  const std::string _funct_name;
  const std::string _source_file;
  const size_t _source_line;

 public:
  funct_info(std::string&& funct_name,
             std::string&& source_file,
             size_t source_line)
      : _funct_name(funct_name),
        _source_file(source_file),
        _source_line(source_line) {}

  const std::string& get_funct_name() const { return _funct_name; }
  const std::string& get_source_file() const { return _source_file; }
  size_t get_source_line() const { return _source_line; }
};

using funct_cache_map =
    std::map<boost::stacktrace::frame::native_frame_ptr_t, funct_info>;

/**
 * @brief this function open out file if it hasn't be done
 * if file size exceed 256Mo, file is truncated
 *
 * @return int file descriptor
 */
static int open_file() {
  const char* out_file_path = "/tmp/malloc-trace.csv";
  char* env_out_file_path = getenv("out_file_path");
  if (env_out_file_path) {
    out_file_path = env_out_file_path;
  }
  static std::mutex file_m;
  std::lock_guard l(file_m);
  static int out_file = -1;
  if (out_file < 0) {
    out_file =
        open(out_file_path, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  } else {
    struct stat file_stat;
    if (!fstat(out_file, &file_stat) && file_stat.st_size > 0x10000000) {
      ftruncate(out_file, 0);
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
  static std::mutex _funct_cache_m;

  constexpr unsigned size_buff = 0x10000;
  char buff[size_buff];
  char* end_buff = buff - 10;
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
        ::write(out_file, buff, work_pos - buff);
        work_pos = buff;
      }

      if (stack_cpt) {
        *work_pos++ = ',';
      }

      const auto& stack_layer = *stack_iter;
      auto addr = stack_layer.address();
      funct_cache_map::const_iterator cache_entry;
      {
        std::unique_lock l(_funct_cache_m);
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
              "{{\\\"f\\\":\\\"{}\\\",\\\"s\\\":\\\"{}\\\",\\\"l\\\":{}}}",
              cache_entry->second.get_funct_name(),
              cache_entry->second.get_source_file(),
              cache_entry->second.get_source_line())
              .out;
    }
  }
  *work_pos++ = ']';
  *work_pos++ = '"';
  *work_pos++ = '\n';
  ::write(out_file, buff, work_pos - buff);
}

constexpr unsigned block_size = 4096;
constexpr unsigned nb_block = 256;
/**
 * @brief basic allocator
 * At the beginning, we don't know original malloc
 * we must provide a simple malloc free for dlsym
 *
 */
class simply_allocator {
  class node_block {
    unsigned char _buff[block_size];
    bool _free = true;

   public:
    struct key_extractor {
      using type = unsigned char const*;
      type operator()(const node_block& block) const { return block._buff; }
    };

    bool is_free() const { return _free; }
    void set_free(bool free) { _free = free; }
    unsigned char* get_buff() { return _buff; }
  };

  node_block _blocks[nb_block];
  std::mutex _protect;

 public:
  void* malloc(size_t size);
  void* realloc(void* p, size_t size);
  bool free(void* p);
};

/**
 * @brief same as malloc
 *
 * @param size
 * @return void*
 */
void* simply_allocator::malloc(size_t size) {
  if (size > block_size) {
    return nullptr;
  }
  std::lock_guard l(_protect);
  for (node_block* search = _blocks; search != _blocks + nb_block; ++search) {
    if (search->is_free()) {
      search->set_free(false);
      return search->get_buff();
    }
  }
  return nullptr;
}

/**
 * @brief reallocate a pointer,
 * if size > block_size or p doesn't belong to simply_allocator, it returns
 * nullptr
 *
 * @param p
 * @param size
 * @return void*
 */
void* simply_allocator::realloc(void* p, size_t size) {
  if (p < _blocks || p >= _blocks + block_size)
    return nullptr;
  if (size > block_size) {
    return nullptr;
  }
  return p;
}

/**
 * @brief same as free
 *
 * @param p
 * @return true if the pointer belong to this allocator
 */
bool simply_allocator::free(void* p) {
  if (p < _blocks || p >= _blocks + block_size)
    return false;
  std::lock_guard l(_protect);
  for (node_block* search = _blocks; search != _blocks + nb_block; ++search) {
    if (search->get_buff() == p) {
      search->set_free(true);
      return true;
    }
  }
  return false;
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

void search_symbols() {
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
  bool have_to_dump = _thread_dump_active.set_dump_active();
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    dump_callstack(p, size, funct_name);
    _thread_dump_active.reset_dump_active();
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
  p = original_realloc(p, size);
  bool have_to_dump = _thread_dump_active.set_dump_active();
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    dump_callstack(p, size, "realloc");
    _thread_dump_active.reset_dump_active();
  }
  return p;
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
  bool have_to_dump = _thread_dump_active.set_dump_active();
  // if this thread is not yet dumping => call dump_callstack
  if (have_to_dump) {
    dump_callstack(p, 0, "free");
    _thread_dump_active.reset_dump_active();
  }
}