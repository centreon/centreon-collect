/**
 * Copyright 2018-2024 Centreon
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

#ifndef CCB_LUA_MACRO_CACHE_HH
#define CCB_LUA_MACRO_CACHE_HH

#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/lua/internal.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/host_group_member.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_group.hh"
#include "com/centreon/broker/neb/service_group_member.hh"
#include "com/centreon/broker/persistent_cache.hh"

namespace com::centreon::broker::lua {

/**
 *  @class macro_cache macro_cache.hh "com/centreon/broker/lua/macro_cache.hh"
 *  @brief Data cache for Lua macro.
 */
class macro_cache {
  std::shared_ptr<persistent_cache> _cache;
  absl::flat_hash_map<uint64_t, std::shared_ptr<io::data>> _instances;
  absl::flat_hash_map<uint64_t, std::shared_ptr<neb::pb_host>> _hosts;
  /* The host groups cache stores also a set with the pollers telling they need
   * the cache. So if no more poller needs a host group, we can remove it from
   * the cache. */
  absl::flat_hash_map<uint64_t,
                      std::pair<std::shared_ptr<neb::pb_host_group>,
                                absl::flat_hash_set<uint32_t>>>
      _host_groups;
  absl::btree_map<std::pair<uint64_t, uint64_t>, std::shared_ptr<io::data>>
      _host_group_members;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::shared_ptr<io::data>>
      _custom_vars;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::shared_ptr<neb::pb_service>>
      _services;
  /* The service groups cache stores also a set with the pollers telling they
   * need the cache. So if no more poller needs a service group, we can remove
   * it from the cache. */
  absl::flat_hash_map<uint64_t,
                      std::pair<std::shared_ptr<neb::pb_service_group>,
                                absl::flat_hash_set<uint32_t>>>
      _service_groups;
  absl::btree_map<std::tuple<uint64_t, uint64_t, uint64_t>,
                  std::shared_ptr<io::data>>
      _service_group_members;
  absl::flat_hash_map<uint64_t, std::shared_ptr<storage::pb_index_mapping>>
      _index_mappings;
  absl::flat_hash_map<uint64_t, std::shared_ptr<storage::pb_metric_mapping>>
      _metric_mappings;
  absl::flat_hash_map<uint64_t, std::shared_ptr<bam::pb_dimension_ba_event>>
      _dimension_ba_events;
  std::unordered_multimap<
      uint64_t,
      std::shared_ptr<bam::pb_dimension_ba_bv_relation_event>>
      _dimension_ba_bv_relation_events;
  absl::flat_hash_map<uint64_t, std::shared_ptr<bam::pb_dimension_bv_event>>
      _dimension_bv_events;

 public:
  macro_cache(const std::shared_ptr<persistent_cache>& cache);
  macro_cache(const macro_cache&) = delete;
  ~macro_cache();

  void write(std::shared_ptr<io::data> const& data);

  const storage::pb_index_mapping& get_index_mapping(uint64_t index_id) const;
  const std::shared_ptr<storage::pb_metric_mapping>& get_metric_mapping(
      uint64_t metric_id) const;
  const std::shared_ptr<neb::pb_host>& get_host(uint64_t host_id) const;
  const std::shared_ptr<neb::pb_service>& get_service(uint64_t host_id,
                                               uint64_t service_id) const;
  const std::string& get_host_name(uint64_t host_id) const;
  const std::string& get_notes_url(uint64_t host_id, uint64_t service_id) const;
  const std::string& get_notes(uint64_t host_id, uint64_t service_id) const;
  const std::string& get_action_url(uint64_t host_id,
                                    uint64_t service_id) const;
  int32_t get_severity(uint64_t host_id, uint64_t service_id) const;
  std::string_view get_check_command(uint64_t host_id,
                                     uint64_t service_id = 0) const;
  const std::string& get_host_group_name(uint64_t id) const;
  absl::btree_map<std::pair<uint64_t, uint64_t>,
                  std::shared_ptr<io::data>> const&
  get_host_group_members() const;
  const std::string& get_service_description(uint64_t host_id,
                                             uint64_t service_id) const;
  const std::string& get_service_group_name(uint64_t id) const;
  absl::btree_map<std::tuple<uint64_t, uint64_t, uint64_t>,
                  std::shared_ptr<io::data>> const&
  get_service_group_members() const;
  const std::string& get_instance(uint64_t instance_id) const;

  const std::unordered_multimap<
      uint64_t,
      std::shared_ptr<bam::pb_dimension_ba_bv_relation_event>>&
  get_dimension_ba_bv_relation_events() const;
  const std::shared_ptr<bam::pb_dimension_ba_event>& get_dimension_ba_event(
      uint64_t id) const;
  const std::shared_ptr<bam::pb_dimension_bv_event>& get_dimension_bv_event(
      uint64_t id) const;

 private:
  macro_cache& operator=(macro_cache const& f);

  void _process_instance(std::shared_ptr<io::data> const& data);
  void _process_pb_instance(std::shared_ptr<io::data> const& data);
  void _process_host(std::shared_ptr<io::data> const& data);
  void _process_pb_host(std::shared_ptr<io::data> const& data);
  void _process_pb_host_status(std::shared_ptr<io::data> const& data);
  void _process_pb_adaptive_host_status(const std::shared_ptr<io::data>& data);
  void _process_pb_adaptive_host(std::shared_ptr<io::data> const& data);
  void _process_host_group(std::shared_ptr<io::data> const& data);
  void _process_pb_host_group(std::shared_ptr<io::data> const& data);
  void _process_host_group_member(std::shared_ptr<io::data> const& data);
  void _process_pb_host_group_member(std::shared_ptr<io::data> const& data);
  void _process_custom_variable(std::shared_ptr<io::data> const& data);
  void _process_pb_custom_variable(std::shared_ptr<io::data> const& data);
  void _process_service(std::shared_ptr<io::data> const& data);
  void _process_pb_service(std::shared_ptr<io::data> const& data);
  void _process_pb_service_status(const std::shared_ptr<io::data>& data);
  void _process_pb_adaptive_service_status(
      const std::shared_ptr<io::data>& data);
  void _process_pb_adaptive_service(std::shared_ptr<io::data> const& data);
  void _process_service_group(std::shared_ptr<io::data> const& data);
  void _process_pb_service_group(std::shared_ptr<io::data> const& data);
  void _process_service_group_member(std::shared_ptr<io::data> const& data);
  void _process_pb_service_group_member(std::shared_ptr<io::data> const& data);
  void _process_index_mapping(std::shared_ptr<io::data> const& data);
  void _process_metric_mapping(std::shared_ptr<io::data> const& data);
  void _process_dimension_ba_event(std::shared_ptr<io::data> const& data);
  void _process_dimension_ba_bv_relation_event(
      std::shared_ptr<io::data> const& data);
  void _process_dimension_bv_event(std::shared_ptr<io::data> const& data);
  void _process_dimension_truncate_table_signal(
      std::shared_ptr<io::data> const& data);
  void _process_pb_dimension_truncate_table_signal(
      std::shared_ptr<io::data> const& data);

  void _save_to_disk();
};
}  // namespace com::centreon::broker::lua

#endif  // !CCB_LUA_MACRO_CACHE_HH
