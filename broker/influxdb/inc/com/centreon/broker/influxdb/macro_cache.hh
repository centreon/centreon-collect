/**
 * Copyright 2015 Centreon
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

#ifndef CCB_INFLUXDB_MACRO_CACHE_HH
#define CCB_INFLUXDB_MACRO_CACHE_HH

#include "com/centreon/broker/influxdb/internal.hh"
#include "com/centreon/broker/io/factory.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/persistent_cache.hh"

namespace com::centreon::broker::influxdb {

/**
 *  @class macro_cache macro_cache.hh
 * "com/centreon/broker/influxdb/macro_cache.hh"
 *  @brief Data cache for InfluxDB macro.
 */
class macro_cache {
  std::shared_ptr<persistent_cache> _cache;
  std::unordered_map<uint64_t, std::shared_ptr<io::data>> _instances;
  std::unordered_map<uint64_t, std::shared_ptr<io::data>> _hosts;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::shared_ptr<io::data>>
      _services;
  absl::flat_hash_map<uint64_t, std::shared_ptr<storage::pb_index_mapping>>
      _index_mappings;
  absl::flat_hash_map<uint64_t, std::shared_ptr<storage::pb_metric_mapping>>
      _metric_mappings;

  void _process_pb_instance(std::shared_ptr<io::data> const& data);
  void _process_pb_host(std::shared_ptr<io::data> const& data);
  void _process_pb_service(std::shared_ptr<io::data> const& data);
  void _process_index_mapping(std::shared_ptr<io::data> const& data);
  void _process_metric_mapping(std::shared_ptr<io::data> const& data);
  void _save_to_disk();

 public:
  macro_cache(std::shared_ptr<persistent_cache> const& cache);
  macro_cache(macro_cache const& f) = delete;
  macro_cache& operator=(macro_cache const& f) = delete;
  ~macro_cache();

  void write(std::shared_ptr<io::data> const& data);

  storage::pb_index_mapping const& get_index_mapping(uint64_t index_id) const;
  storage::pb_metric_mapping const& get_metric_mapping(
      uint64_t metric_id) const;
  std::string const& get_host_name(uint64_t host_id) const;
  std::string const& get_service_description(uint64_t host_id,
                                             uint64_t service_id) const;
  std::string const& get_instance(uint64_t instance_id) const;
};
}  // namespace com::centreon::broker::influxdb

#endif  // !CCB_INFLUXDB_MACRO_CACHE_HH
