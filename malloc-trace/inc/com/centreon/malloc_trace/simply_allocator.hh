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

#ifndef CMT_SIMPLY_ALLOCATOR_HH
#define CMT_SIMPLY_ALLOCATOR_HH

namespace com::centreon::malloc_trace {

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

}  // namespace com::centreon::malloc_trace

#endif
