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
#ifndef CMT_INTRUSIVE_MAP_HH
#define CMT_INTRUSIVE_MAP_HH

namespace com::centreon::malloc_trace {

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
  node_type* _free_node = _nodes_array;
  const node_type* _array_end = _free_node + node_array_size;

  using node_map =
      boost::intrusive::set<node_type,
                            boost::intrusive::key_of_value<key_extractor> >;

  node_map _nodes;

 public:
  ~intrusive_map() { _nodes.clear(); }

  const node_type* find(const key_type& key) const {
    auto found = _nodes.find(key);
    if (found == _nodes.end()) {
      return nullptr;
    } else {
      return &*found;
    }
  }

  const node_type* insert_and_get(const key_type& key) {
    if (_free_node >= _array_end) {
      return nullptr;
    }

    node_type* to_insert = _free_node++;
    new (to_insert) node_type(key);
    _nodes.insert(*to_insert);
    return to_insert;
  }

  /**
   * @brief sometimes method are called before object construction
   *
   * @return true constructor has been called
   * @return false
   */
  bool is_initialized() const { return _free_node; }
};

}  // namespace com::centreon::malloc_trace

#endif
