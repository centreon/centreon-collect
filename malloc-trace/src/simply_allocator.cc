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

#include "simply_allocator.hh"

using namespace com::centreon::malloc_trace;

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
