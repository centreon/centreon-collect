/**
 * Copyright 2015 - 2020 Centreon
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

#ifndef CCB_STORAGE_INDEX_MAPPING_HH
#define CCB_STORAGE_INDEX_MAPPING_HH

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker::storage {

/**
 *  @class index_mapping index_mapping.hh
 * "com/centreon/broker/storage/index_mapping.hh"
 *  @brief Information about an index stored in the database.
 *
 *  Used to provide more informations about the mapping of the index
 *  to its service/host.
 */
class index_mapping : public io::data {
 public:
  index_mapping();
  index_mapping(uint64_t index_id, uint32_t host_id, uint32_t service_id);
  index_mapping(index_mapping const& other) = delete;
  ~index_mapping() = default;
  index_mapping& operator=(index_mapping const& other) = delete;
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::storage, storage::de_index_mapping>::value;
  }

  uint64_t index_id;
  uint32_t host_id;
  uint32_t service_id;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::storage

#endif  // !CCB_STORAGE_INDEX_MAPPING_HH
