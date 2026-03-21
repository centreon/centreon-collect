/**
 * Copyright 2022-2024 Centreon
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

#ifndef CCB_GLOBAL_CACHE_DATA_HH
#define CCB_GLOBAL_CACHE_DATA_HH

#include "com/centreon/broker/cache/protobuf.hh"

#include "global_cache.hh"

namespace com::centreon::broker::cache {

namespace multi_index = boost::multi_index;
namespace interprocess = boost::interprocess;

/**
 * @brief string_string_view_equal and string_string_view_hash are mandatory to
 * find a string_view in a hashed string container
 *
 */
struct string_string_view_equal {
  bool operator()(const string& left, const std::string_view& right) const {
    return left.compare(0, right.length(), right.data()) == 0;
  }
  bool operator()(const std::string_view& left, const string& right) const {
    return right.compare(0, left.length(), left.data()) == 0;
  }
};

struct string_string_view_hash {
  size_t operator()(const string& left) const {
    return absl::Hash<std::string_view>()(
        std::string_view(left.c_str(), left.length()));
  }

  size_t operator()(const std::string_view& left) const {
    return absl::Hash<std::string_view>()(left);
  }
};

/**
 * @brief container of all datas of global cache
 *
 */
class global_cache_data : public global_cache {
  using index_id_mapping =
      interprocess::flat_map<uint64_t /* metric_id */,
                             host_serv_pair,
                             std::less<uint64_t>,
                             managed_mapped_file::allocator<
                                 std::pair<uint64_t, host_serv_pair>>::type>;

  using metric_id_mapping = interprocess::flat_map<
      uint64_t /* metric_id */,
      uint64_t /* index_id */,
      std::less<uint64_t>,
      managed_mapped_file::allocator<std::pair<uint64_t, uint64_t>>::type>;

  /**
   * @brief severity is also given by a custom variable "CRITICALITY_LEVEL"
   * So we store both in this pair
   */
  using host_custom_var_pair =
      std::pair<interprocess::offset_ptr<host>,
                int32_t /* severity given by custom var*/>;

  using id_to_host = interprocess::flat_map<
      uint64_t,
      host_custom_var_pair,
      std::less<uint64_t>,
      managed_mapped_file::allocator<
          std::pair<uint64_t, host_custom_var_pair>>::type>;

  /**
   * @brief severity is also given by a custom variable "CRITICALITY_LEVEL"
   * So we store both in this pair
   */
  using service_custom_var_pair =
      std::pair<interprocess::offset_ptr<service>,
                int32_t /* severity given by custom var*/>;
  using id_to_serv = interprocess::flat_map<
      host_serv_pair,
      service_custom_var_pair,
      std::less<host_serv_pair>,
      managed_mapped_file::allocator<
          std::pair<host_serv_pair, service_custom_var_pair>>::type>;

  using id_to_instance = interprocess::flat_map<
      uint64_t,
      interprocess::offset_ptr<instance>,
      std::less<uint64_t>,
      managed_mapped_file::allocator<
          std::pair<uint64_t, interprocess::offset_ptr<instance>>>::type>;

  // groups
  using id_to_host_group = interprocess::flat_map<
      uint64_t,
      interprocess::offset_ptr<host_group>,
      std::less<uint64_t>,
      managed_mapped_file::allocator<
          std::pair<uint64_t, interprocess::offset_ptr<host_group>>>::type>;

  struct host_group_member {
    host_group_member() {}
    host_group_member(uint64_t host, uint64_t group, uint64_t poller)
        : host_id(host), group_id(group), poller_id(poller) {}
    uint64_t host_id;
    uint64_t group_id;
    uint64_t poller_id;

    bool operator<(const host_group_member& right) const {
      if (host_id != right.host_id) {
        return host_id < right.host_id;
      }
      if (group_id != right.group_id) {
        return group_id < right.group_id;
      }
      return poller_id < right.poller_id;
    }
  };

  struct host_group_member_group_id_getter {
    using result_type = uint64_t;

    result_type operator()(const host_group_member& data) const {
      return data.group_id;
    }
  };

  struct host_group_member_group_id_poller_id_getter {
    using result_type = std::pair<uint64_t, uint64_t>;

    result_type operator()(const host_group_member& data) const {
      return std::make_pair(data.group_id, data.poller_id);
    }
  };

  struct host_group_member_host_id_getter {
    using result_type = uint64_t;

    result_type operator()(const host_group_member& data) const {
      return data.host_id;
    }
  };

  using host_group_cont = multi_index::multi_index_container<
      host_group_member,
      multi_index::indexed_by<
          multi_index::ordered_non_unique<host_group_member_group_id_getter>,
          multi_index::ordered_non_unique<
              host_group_member_group_id_poller_id_getter>,
          multi_index::ordered_non_unique<host_group_member_host_id_getter>,
          multi_index::ordered_unique<
              multi_index::identity<host_group_member>>>,
      managed_mapped_file::allocator<host_group_member>::type>;

  using id_to_serv_group = interprocess::flat_map<
      uint64_t,
      interprocess::offset_ptr<service_group>,
      std::less<uint64_t>,
      managed_mapped_file::allocator<
          std::pair<uint64_t, interprocess::offset_ptr<service_group>>>::type>;

  struct service_group_member {
    service_group_member() {}
    service_group_member(uint64_t host_id,
                         uint64_t service_id,
                         uint64_t grp_id,
                         uint64_t poller)
        : host_serv_id(host_id, service_id),
          group_id(grp_id),
          poller_id(poller) {}

    host_serv_pair host_serv_id;
    uint64_t group_id;
    uint64_t poller_id;

    bool operator<(const service_group_member& right) const {
      if (host_serv_id != right.host_serv_id) {
        return host_serv_id < right.host_serv_id;
      }
      if (group_id != right.group_id) {
        return group_id < right.group_id;
      }
      return poller_id < right.poller_id;
    }
  };

  struct service_group_member_group_id_getter {
    using result_type = uint64_t;

    result_type operator()(const service_group_member& data) const {
      return data.group_id;
    }
  };

  struct service_group_member_group_id_poller_id_getter {
    using result_type = std::pair<uint64_t, uint64_t>;

    result_type operator()(const service_group_member& data) const {
      return std::make_pair(data.group_id, data.poller_id);
    }
  };

  struct service_group_member_host_service_id_getter {
    using result_type = host_serv_pair;

    const host_serv_pair& operator()(const service_group_member& data) const {
      return data.host_serv_id;
    }
  };

  using service_group_cont = multi_index::multi_index_container<
      service_group_member,
      multi_index::indexed_by<
          multi_index::ordered_non_unique<service_group_member_group_id_getter>,
          multi_index::ordered_non_unique<
              service_group_member_group_id_poller_id_getter>,
          multi_index::ordered_non_unique<
              service_group_member_host_service_id_getter>,
          multi_index::ordered_unique<
              multi_index::identity<service_group_member>>>,
      managed_mapped_file::allocator<service_group_member>::type>;

  using id_to_dimension_ba_event = interprocess::flat_map<
      uint64_t,
      interprocess::offset_ptr<dimension_ba_event>,
      std::less<uint64_t>,
      managed_mapped_file::allocator<
          std::pair<uint64_t,
                    interprocess::offset_ptr<dimension_ba_event>>>::type>;

  using id_to_dimension_bv_event = interprocess::flat_map<
      uint64_t,
      interprocess::offset_ptr<dimension_bv_event>,
      std::less<uint64_t>,
      managed_mapped_file::allocator<
          std::pair<uint64_t,
                    interprocess::offset_ptr<dimension_bv_event>>>::type>;

  using id_to_dimension_ba_bv_relation = interprocess::flat_multimap<
      uint64_t /* ba_id */,
      uint64_t /* bv_id */,
      std::less<uint64_t>,
      managed_mapped_file::allocator<std::pair<uint64_t, uint64_t>>::type>;

  struct tag_poller {
    tag_poller(tag* src, allocators& alloc)
        : data(src), pollers(alloc.segm_manager) {
      pollers.insert(src->poller_id());
    }
    interprocess::offset_ptr<tag> data;
    interprocess::flat_set<uint64_t,
                           std::less<uint64_t>,
                           managed_mapped_file::allocator<uint64_t>::type>
        pollers;
  };

  using id_to_tag = interprocess::flat_map<
      std::pair<uint64_t, TagType>,
      tag_poller,
      std::less<std::pair<uint64_t, TagType>>,
      managed_mapped_file::allocator<
          std::pair<std::pair<uint64_t, TagType>, tag_poller>>::type>;

  std::unique_ptr<allocators> _allocators;

  index_id_mapping* _index_id_mapping ABSL_GUARDED_BY(_protect);
  id_to_host* _id_to_host ABSL_GUARDED_BY(_protect);
  id_to_serv* _id_to_service ABSL_GUARDED_BY(_protect);
  id_to_instance* _id_to_instance ABSL_GUARDED_BY(_protect);
  id_to_host_group* _id_to_host_group ABSL_GUARDED_BY(_protect);
  id_to_serv_group* _id_to_serv_group ABSL_GUARDED_BY(_protect);
  host_group_cont* _host_group_members ABSL_GUARDED_BY(_protect);
  service_group_cont* _service_group_members ABSL_GUARDED_BY(_protect);
  metric_id_mapping* _metric_id_mapping ABSL_GUARDED_BY(_protect);
  id_to_dimension_ba_event* _id_to_dimension_ba_event ABSL_GUARDED_BY(_protect);
  id_to_dimension_bv_event* _id_to_dimension_bv_event ABSL_GUARDED_BY(_protect);
  id_to_dimension_ba_bv_relation* _id_to_dimension_ba_bv_relation
      ABSL_GUARDED_BY(_protect);
  id_to_tag* _id_to_tag ABSL_GUARDED_BY(_protect);

  void managed_map(bool create) override;

  void _process_pb_instance(std::shared_ptr<io::data> const& data);
  void _process_pb_host(std::shared_ptr<io::data> const& data);
  void _process_pb_host_status(std::shared_ptr<io::data> const& data);
  void _process_pb_adaptive_host_status(const std::shared_ptr<io::data>& data);
  void _process_pb_adaptive_host(std::shared_ptr<io::data> const& data);
  void _process_pb_host_group(std::shared_ptr<io::data> const& data);
  void _process_pb_host_group_member(std::shared_ptr<io::data> const& data);
  void _process_pb_custom_variable(std::shared_ptr<io::data> const& data);
  void _process_pb_service(std::shared_ptr<io::data> const& data);
  void _process_pb_service_status(const std::shared_ptr<io::data>& data);
  void _process_pb_adaptive_service_status(
      const std::shared_ptr<io::data>& data);
  void _process_pb_adaptive_service(std::shared_ptr<io::data> const& data);
  void _process_pb_service_group(std::shared_ptr<io::data> const& data);
  void _process_pb_service_group_member(std::shared_ptr<io::data> const& data);
  void _process_index_mapping(std::shared_ptr<io::data> const& data);
  void _process_metric_mapping(std::shared_ptr<io::data> const& data);
  void _process_dimension_ba_event(std::shared_ptr<io::data> const& data);
  void _process_dimension_ba_bv_relation_event(
      std::shared_ptr<io::data> const& data);
  void _process_dimension_bv_event(std::shared_ptr<io::data> const& data);
  void _process_pb_dimension_truncate_table_signal(
      std::shared_ptr<io::data> const& data);
  void _process_pb_tag(std::shared_ptr<io::data> const& data);

  void _write_conf(const std::shared_ptr<io::data>& data);
  void _write_rt(const std::shared_ptr<io::data>& data);

  void _write_impl(const std::shared_ptr<io::data>& d,
                   bool first_attempt) override;

  host_custom_var_pair _get_host(uint64_t host_id, lock& l);

  service_custom_var_pair _get_service(uint64_t host_id,
                                       uint64_t service_id,
                                       lock& l);

 public:
  global_cache_data(const std::shared_ptr<asio::io_context> io_context,
                    const std::string& file_path,
                    e_cache_type cache_type,
                    const std::shared_ptr<global_cache>& conf_cache,
                    unsigned grow_step = 0x20000000,
                    unsigned nb_update_before_save = 1000,
                    std::chrono::system_clock::duration save_interval =
                        std::chrono::seconds(10));

  const host* get_host(uint64_t host_id, lock& l) override;
  const service* get_service(uint64_t host_id,
                             uint64_t service_id,
                             lock& l) override;

  std::optional<host_serv_pair> get_host_serv_id(uint64_t index_id) override;

  const instance* get_instance(uint64_t instance_id, lock& l) override;

  const host_group* get_host_group(uint64_t group_id, lock& l) const override;
  const service_group* get_service_group(uint64_t group_id,
                                         lock& l) const override;
  void append_service_group(uint64_t host,
                            uint64_t service,
                            std::ostream& request_body) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void append_host_group(uint64_t host,
                         std::ostream& request_body) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void append_host_tag_id(uint64_t host,
                          TagType tag_type,
                          std::ostream& request_body) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void append_serv_tag_id(uint64_t host,
                          uint64_t serv,
                          TagType tag_type,
                          std::ostream& request_body) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void append_host_tag_name(uint64_t host,
                            TagType tag_type,
                            std::ostream& request_body) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void append_serv_tag_name(uint64_t host,
                            uint64_t serv,
                            TagType tag_type,
                            std::ostream& request_body) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  uint64_t get_index_id_from_metric_id(uint64_t metric_id) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  std::optional<int32_t> get_severity(uint64_t host_id,
                                      uint64_t service_id) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  const dimension_ba_event* get_dimension_ba_event(uint64_t ba_id,
                                                   lock& l) const override;
  const dimension_bv_event* get_dimension_bv_event(uint64_t bv_id,
                                                   lock& l) const override;
  void enumerate_bvs(uint64_t ba_id, bv_enumerator&& enumerator) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void enumerate_host_group(uint64_t host_id,
                            group_enumerator&& enumerator) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
  void enumerate_service_group(uint64_t host_id,
                               uint64_t service_id,
                               group_enumerator&& enumerator) const override
      ABSL_SHARED_LOCKS_REQUIRED(_protect);
};

}  // namespace com::centreon::broker::cache

#endif
