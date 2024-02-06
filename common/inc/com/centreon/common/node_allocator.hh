/**
 * Copyright 2023 Centreon
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

#ifndef CCCM_NODE_ALLOCATOR_HH
#define CCCM_NODE_ALLOCATOR_HH

#include <boost/align/aligned_allocator_adaptor.hpp>
#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>
#include <boost/throw_exception.hpp>
#include <memory>
#include <type_traits>

namespace boost::interprocess {

/**
 * @brief boost::interprocess::to_raw_pointer convert a
 * boost::interprocess::offset_ptr<T> to a T* this specialization allow us to
 * work with both offset_ptr and raw pointers
 *
 * @tparam T
 * @param p
 * @return T*
 */
template <class T>
T* to_raw_pointer(T* p) {
  return p;
}
}  // namespace boost::interprocess

namespace com::centreon::common {

using errinfo_pointer = boost::error_info<struct errinfo_pointer_, const void*>;

/**
 * @brief this node allocator allocate chunks of blocks with the size equal to
 * sizeof(T) classic allocators allocate arrays of objects, this allocator
 * allocate one object by allocate() call this is more simple and faster than
 * boost node allocators This allocator allocator passed in constructor param to
 * allocate block of memory When user calls allocate, it's pop a node from a
 * block of memory and returns it. If no free node is available, it allocates
 * another block of memory and uses it
 *
 * this class is not thread safe
 *
 * @code {.C++}
 * class toto {
 *  int int_value;
 *  std::string str_value;
 * };
 *
 * node_allocator<toto, std::allocator<uint8_t>, 0x10000>
 * allocator(std::allocator<uint8_t>());
 *
 * toto * instance = new (allocator.allocate()) toto({5,"hello"});
 * @endcode
 *
 *
 * @tparam T object to allocate
 * @tparam byte_allocator_type uint8_t allocator such as std::allocator<uint8_t>
 * @tparam block_size number of nodes by chunk a too small value make allocate
 * to call many times block allocation, at the opposite, a huge value may waste
 * space
 */
template <class T, class byte_allocator_type, size_t block_size>
class node_allocator {
 public:
  struct no_allocated_exception : public virtual boost::exception,
                                  public virtual std::exception {};

  struct bad_aligned_exception : public virtual boost::exception,
                                 public virtual std::exception {};

  struct yet_freed_exception : public virtual boost::exception,
                               public virtual std::exception {};

 private:
  using byte_pointer =
      typename std::allocator_traits<byte_allocator_type>::pointer;

  using allocator_value_type =
      typename std::allocator_traits<byte_allocator_type>::value_type;

  using allocator_value_type_pointer = allocator_value_type*;

  using aligned_allocator =
      boost::alignment::aligned_allocator_adaptor<byte_allocator_type,
                                                  std::alignment_of<T>::value>;

  static_assert(sizeof(allocator_value_type) == 1,
                "sizeof allocator value type must be equal to 1 (be a char or "
                "an unsigned char)");

  /**
   * @brief is a base memory block, it's the memory given to ONE object
   * it has 2 states
   *   free = true: next_free points to next free node or nullptr if it's the
   * last one free = false; buff_or_next_free is a memory block used by the
   * allocated object
   */
  struct node {
    union buff_or_next_free_ {
      alignas(T) uint8_t buffer[sizeof(T)];
      byte_pointer next_free;
      buff_or_next_free_() {}
    } buff_or_next_free;
    bool free;

    node() : free(true) {}
  };

  /**
   * @brief this struct is an array of nodes
   * his jobs is to find a free node for allocate() and reuse nodes after
   * deallocate calls He is chained with other by next attribute
   */
  struct chunk {
    node nodes[block_size];
    byte_pointer first_free;
    byte_pointer next;

    /**
     * @brief Construct a new chunk object it initializes nodes object  in a
     * "free" mode and chains them
     *
     */
    chunk()
        : first_free(reinterpret_cast<allocator_value_type_pointer>(nodes)) {
      for (node *to_init = nodes, *end = nodes + block_size; to_init < end;
           ++to_init) {
        to_init->buff_or_next_free.next_free =
            reinterpret_cast<allocator_value_type_pointer>(to_init + 1);
      }
      nodes[block_size - 1].buff_or_next_free.next_free = nullptr;
    }

    /**
     * @brief gives to the caller a free node if it exists
     * it marks selected nodes as not free and update first_free
     *
     * @return T*
     */
    T* allocate() {
      if (first_free) {
        node* to_use = reinterpret_cast<node*>(
            boost::interprocess::to_raw_pointer(first_free));
        first_free = to_use->buff_or_next_free.next_free;
        T* ret = reinterpret_cast<T*>(to_use->buff_or_next_free.buffer);
        to_use->free = false;
        return ret;
      }
      return nullptr;
    }

    /**
     * @brief if to_free belongs to nodes, it marks node as free and chains it
     * to other free nodes
     *
     * @param to_free
     * @return true if to_free belongs to nodes
     * @return false
     */
    bool deallocate(T* to_free) {
      uint8_t* to_test = reinterpret_cast<uint8_t*>(to_free);
      if (to_test < nodes[0].buff_or_next_free.buffer ||
          to_test > nodes[block_size - 1].buff_or_next_free.buffer) {
        return false;
      }

      if ((to_test - nodes[0].buff_or_next_free.buffer) % sizeof(node)) {
        BOOST_THROW_EXCEPTION(bad_aligned_exception()
                              << errinfo_pointer(to_free));
      }
      node* node_to_free = reinterpret_cast<node*>(to_free);
      if (node_to_free->free) {
        BOOST_THROW_EXCEPTION(yet_freed_exception()
                              << errinfo_pointer(to_free));
      }
      node_to_free->free = true;
      node_to_free->buff_or_next_free.next_free = first_free;
      first_free = reinterpret_cast<allocator_value_type_pointer>(node_to_free);
      return true;
    }

    void dump(std::ostream& str) const {
      // count free nodes
      unsigned free_cpt = 0;
      for (byte_pointer free_node = first_free; free_node; ++free_cpt) {
        free_node = reinterpret_cast<node*>(
                        boost::interprocess::to_raw_pointer(free_node))
                        ->buff_or_next_free.next_free;
      }

      str << "chunk_begin:" << this << " allocated:" << block_size - free_cpt
          << " free:" << free_cpt;
    }
  };

  byte_pointer _first;
  aligned_allocator _allocator;

  /**
   * @brief call _allocator.allocate to create a new chunk
   *
   * @return chunk*
   */
  chunk* _create_chunk() {
    void* addr =
        boost::interprocess::to_raw_pointer(_allocator.allocate(sizeof(chunk)));
    return new (addr) chunk();
  }

 public:
  node_allocator(const byte_allocator_type& allocator);

  T* allocate();
  void deallocate(T* to_free);

  void dump(std::ostream&) const;
};

/**
 * @brief Construct a new node allocator<T, byte allocator type, block
 * size>::node allocator object
 *
 * @tparam T
 * @tparam byte_allocator_type
 * @tparam block_size
 * @param allocator allocator that will be copied in _allocator attribute
 */
template <class T, class byte_allocator_type, size_t block_size>
node_allocator<T, byte_allocator_type, block_size>::node_allocator(
    const byte_allocator_type& allocator)
    : _allocator(allocator) {
  _first = reinterpret_cast<allocator_value_type_pointer>(_create_chunk());
}

/**
 * @brief call chunk::allocate until one returns a not null pointer, if not, it
 * creates another chunk
 *
 * @tparam T
 * @tparam byte_allocator_type
 * @tparam block_size
 * @return T*
 */
template <class T, class byte_allocator_type, size_t block_size>
T* node_allocator<T, byte_allocator_type, block_size>::allocate() {
  for (byte_pointer chunk_iter = _first; chunk_iter;) {
    chunk* chunk_ptr = reinterpret_cast<chunk*>(
        boost::interprocess::to_raw_pointer(chunk_iter));
    T* ret = chunk_ptr->allocate();
    if (ret) {
      return ret;
    }
    chunk_iter = chunk_ptr->next;
  }
  chunk* new_chunk = _create_chunk();
  new_chunk->next = _first;
  _first = reinterpret_cast<allocator_value_type_pointer>(new_chunk);
  return new_chunk->allocate();
}

/**
 * @brief call chunk::deallocate until it returns true
 *
 * @tparam T
 * @tparam byte_allocator_type
 * @tparam block_size
 * @param to_free
 */
template <class T, class byte_allocator_type, size_t block_size>
void node_allocator<T, byte_allocator_type, block_size>::deallocate(
    T* to_free) {
  for (byte_pointer chunk_iter = _first; chunk_iter;) {
    chunk* chunk_ptr = reinterpret_cast<chunk*>(
        boost::interprocess::to_raw_pointer(chunk_iter));
    if (chunk_ptr->deallocate(to_free)) {
      return;
    }
    chunk_iter = chunk_ptr->next;
  }
  BOOST_THROW_EXCEPTION(no_allocated_exception() << errinfo_pointer(to_free));
}

template <class T, class byte_allocator_type, size_t block_size>
void node_allocator<T, byte_allocator_type, block_size>::dump(
    std::ostream& str) const {
  for (byte_pointer to_dump = _first; to_dump;) {
    const chunk* chunk_ptr = reinterpret_cast<const chunk*>(
        boost::interprocess::to_raw_pointer(to_dump));
    chunk_ptr->dump(str);
    str << std::endl;
    to_dump = chunk_ptr->next;
  }
}

template <class T, class byte_allocator_type, size_t block_size>
std::ostream& operator<<(
    std::ostream& str,
    const node_allocator<T, byte_allocator_type, block_size>& to_dump) {
  to_dump.dump(str);
  return str;
}

}  // namespace com::centreon::common

#endif
