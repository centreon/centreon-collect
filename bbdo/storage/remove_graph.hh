/**
 * Copyright 2012-2023 Centreon
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

#ifndef CCB_STORAGE_REMOVE_GRAPH_HH
#define CCB_STORAGE_REMOVE_GRAPH_HH

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker::storage {
/**
 *  @class remove_graph remove_graph.hh
 * "com/centreon/broker/storage/remove_graph.hh"
 *  @brief Remove a RRD graph.
 *
 *  Remove a RRD graph.
 */
class remove_graph : public io::data {
 public:
  uint64_t id;
  bool is_index;

  remove_graph();
  remove_graph(uint64_t index_id, bool is_index);
  ~remove_graph() noexcept = default;
  remove_graph(remove_graph const&) = delete;
  remove_graph& operator=(remove_graph const&) = delete;
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::storage, storage::de_remove_graph>::value;
  }

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::storage

#endif  // !CCB_STORAGE_REMOVE_GRAPH_HH
