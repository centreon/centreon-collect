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

#include <map>
#include <memory>
#include <mutex>

#include <boost/intrusive/set.hpp>
#include <boost/stacktrace.hpp>

#include <fmt/format.h>

// inline void reverse(char s[]) {
//   int i, j;
//   char c;

//   for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
//     c = s[i];
//     s[i] = s[j];
//     s[j] = c;
//   }
// }

// inline unsigned itoa(unsigned n, char s[]) {
//   unsigned i = 0;
//   do {                     /* generate digits in reverse order */
//     s[i++] = n % 10 + '0'; /* get next digit */
//   } while ((n /= 10) > 0); /* delete it */
//   s[i] = '\0';
//   reverse(s);
//   return i;
// }

// inline unsigned pointer_to_hex(const void* pt, char s[]) {
//   auto i = reinterpret_cast<std::uintptr_t>(pt);
//   char* to_fill = s;
//   do {
//     uint8_t last_4_bits = i & 0x0F;
//     if (last_4_bits < 10) {
//       *to_fill++ = last_4_bits + '0';
//     } else {
//       *to_fill++ = last_4_bits + 'A' - 10;
//     }
//     i >>= 4;
//   } while (i);
//   *to_fill = 0;
//   reverse(s);
//   return to_fill - s;
// }

/**
 * @brief The goal of this class is to allocate memory only at construction of
 * object
 *
 * @tparam node_type node (key and data) that must inherit from
 * boost::intrusive::set_base_hook<>
 * @tparam key_extractor struct with an operator that extract key from node_type
 */
template <class node_type, class key_extractor>
class intrusive_map {
 public:
  using key_type = typename key_extractor::type;

 private:
  node_type* _nodes_array;
  node_type* _free_node;
  const node_type* _array_end;

  using node_map =
      boost::intrusive::set<node_type,
                            boost::intrusive::key_of_value<key_extractor> >;

  node_map _nodes;

 public:
  intrusive_map(size_t node_array_size) {
    _nodes_array = new node_type[node_array_size];
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
                              thread_trace_active::key_extractor> {
  std::mutex _protect;

 public:
  thread_dump_active(size_t node_array_size);
  bool set_dump_active();
  void reset_dump_active();
};

/**
 * @brief Construct a new thread dump active::thread dump active object
 *
 * @param node_array_size size of the array that will store datas
 */
thread_dump_active::thread_dump_active(size_t node_array_size)
    : intrusive_map<thread_trace_active, thread_trace_active::key_extractor>(
          node_array_size) {}

/**
 * @brief Set the flag to true in _by_thread_dump_active
 *
 * @return true the flag was not setted before call
 * @return false the flag was yet setted before call
 */
bool thread_dump_active::set_dump_active() {
  pid_t thread_id = gettid();
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
  pid_t thread_id = gettid();
  std::lock_guard l(_protect);
  const thread_trace_active* exist = find(thread_id);
  if (exist) {
    exist->reset_malloc_trace_active();
  }
}

static const char* _out_file_path = "/tmp/malloc-trace.csv";

static thread_dump_active _thread_dump_active(4096);

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

void dump_callstack(void* p,
                    size_t size,
                    const char* funct_name,
                    unsigned max_stack = 10) {
  static int out_file =
      open(_out_file_path, O_APPEND | O_CREAT | O_RDWR, S_IRWXU);
  static funct_cache_map* _funct_cache = new funct_cache_map;
  static std::mutex _funct_cache_m;

  if (out_file < 0)
    return;

  constexpr unsigned size_buff = 0x10000;
  char buff[size_buff];
  char* end_buff = buff - 10;
  *end_buff = 0;

  char* work_pos =
      fmt::format_to_n(buff, size_buff, "\"{}\";{};{};{};\"[", funct_name,
                       gettid(), reinterpret_cast<std::uintptr_t>(p), size)
          .out;

  boost::stacktrace::stacktrace call_stack;
  if (!call_stack.empty()) {
    unsigned stack_cpt = 0;
    auto stack_iter = call_stack.begin();
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
        std::lock_guard l(_funct_cache_m);
        cache_entry = _funct_cache->find(addr);
        if (cache_entry == _funct_cache->end()) {
          cache_entry =
              _funct_cache
                  ->emplace(addr, funct_info(stack_layer.name(),
                                             stack_layer.source_file(),
                                             stack_layer.source_line()))
                  .first;
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

void* malloc(size_t size) {
  typedef void* (*malloc_signature)(size_t);

  static malloc_signature original_malloc = nullptr;
  if (!original_malloc)
    original_malloc =
        reinterpret_cast<malloc_signature>(dlsym(RTLD_NEXT, "malloc"));

  void* p = original_malloc(size);
  bool have_to_dump = _thread_dump_active.set_dump_active();
  if (have_to_dump) {
    dump_callstack(p, size, "malloc");
    _thread_dump_active.reset_dump_active();
  }
  return p;
}

void free(void* p) {
  typedef void (*free_signature)(void*);
  static free_signature original_free = nullptr;
  if (!original_free)
    original_free = reinterpret_cast<free_signature>(dlsym(RTLD_NEXT, "free"));

  original_free(p);
  bool have_to_dump = _thread_dump_active.set_dump_active();
  if (have_to_dump) {
    dump_callstack(p, 0, "free");
    _thread_dump_active.reset_dump_active();
  }
}