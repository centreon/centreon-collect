/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_GLOBAL_CACHE_DATA_HH
#define CCB_GLOBAL_CACHE_DATA_HH

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/flat_map.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_set.hpp>
#include <com/centreon/common/node_allocator.hh>

#include "global_cache.hh"

CCB_BEGIN()

namespace cache {

namespace multi_index = boost::multi_index;

/**
 * @brief string_string_view_equal and string_string_view_hash are mandatory to
 * find a string_view in a hashed string container
 *
 */
struct string_string_view_equal {
  bool operator()(const string& left, const absl::string_view& right) const {
    return left.compare(0, right.length(), right.data()) == 0;
  }
  bool operator()(const absl::string_view& left, const string& right) const {
    return right.compare(0, left.length(), left.data()) == 0;
  }
};

struct string_string_view_hash {
  size_t operator()(const string& left) const {
    return absl::Hash<absl::string_view>()(
        absl::string_view(left.c_str(), left.length()));
  }

  size_t operator()(const absl::string_view& left) const {
    return absl::Hash<absl::string_view>()(left);
  }
};

/**
 * @brief container of all datas of global cache
 *
 */
class global_cache_data : public global_cache {
  using id_to_metric_info = interprocess::flat_map<
      uint64_t,
      interprocess::offset_ptr<metric_info>,
      std::less<uint64_t>,
      interprocess::allocator<
          std::pair<uint64_t, interprocess::offset_ptr<metric_info>>,
          segment_manager>>;

  using metric_info_allocator = com::centreon::common::
      node_allocator<metric_info, char_allocator, 0x10000>;

  using index_id_mapping =
      interprocess::flat_map<uint64_t,
                             host_serv_pair,
                             std::less<uint64_t>,
                             managed_mapped_file::allocator<
                                 std::pair<uint64_t, host_serv_pair>>::type>;

  using id_to_string = interprocess::flat_map<
      uint64_t,
      string,
      std::less<uint64_t>,
      interprocess::allocator<std::pair<uint64_t, string>, segment_manager>>;

  using id_to_host = interprocess::flat_map<
      uint64_t,
      resource_info,
      std::less<uint64_t>,
      managed_mapped_file::allocator<std::pair<uint64_t, resource_info>>::type>;

  using id_to_serv = interprocess::flat_map<
      host_serv_pair,
      resource_info,
      std::less<host_serv_pair>,
      managed_mapped_file::allocator<
          std::pair<host_serv_pair, resource_info>>::type>;

  // groups
  struct host_group_element {
    uint64_t host;
    uint64_t group;
    inline bool operator<(const host_group_element& right) const {
      if (host != right.host) {
        return host < right.host;
      }
      return group < right.group;
    }
  };

  using host_group = multi_index::multi_index_container<
      host_group_element,
      multi_index::indexed_by<
          multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(host_group_element, uint64_t, host)>,
          multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(host_group_element, uint64_t, group)>,
          multi_index::ordered_unique<
              multi_index::identity<host_group_element>>>,
      managed_mapped_file::allocator<host_group_element>::type>;

  struct service_group_element {
    host_serv_pair serv;
    uint64_t group;
    inline bool operator<(const service_group_element& right) const {
      if (serv != right.serv) {
        return serv < right.serv;
      }
      return group < right.group;
    }
  };

  using service_group = multi_index::multi_index_container<
      service_group_element,
      multi_index::indexed_by<
          multi_index::ordered_non_unique<
              multi_index::member<service_group_element,
                                  host_serv_pair,
                                  &service_group_element::serv>>,
          multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(service_group_element, uint64_t, group)>,
          multi_index::ordered_unique<
              multi_index::identity<service_group_element>>>,
      managed_mapped_file::allocator<service_group_element>::type>;

  // tags
  struct resource_tag {
    TagType tag_type;
    string name;
    uint64_t poller_id;
  };

  using id_to_tag = interprocess::flat_map<
      uint64_t,
      resource_tag,
      std::less<uint64_t>,
      interprocess::allocator<std::pair<uint64_t, resource_tag>,
                              segment_manager>>;

  struct host_tag_element {
    uint64_t host;
    uint64_t tag;
    inline bool operator<(const host_tag_element& right) const {
      if (host != right.host) {
        return host < right.host;
      }
      return tag < right.tag;
    }
  };

  using host_tag = multi_index::multi_index_container<
      host_tag_element,
      multi_index::indexed_by<
          multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(host_tag_element, uint64_t, host)>,
          multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(host_tag_element, uint64_t, tag)>,
          multi_index::ordered_unique<multi_index::identity<host_tag_element>>>,
      managed_mapped_file::allocator<host_tag_element>::type>;

  struct service_tag_element {
    host_serv_pair host_serv;
    uint64_t tag;
    inline bool operator<(const service_tag_element& right) const {
      if (host_serv != right.host_serv) {
        return host_serv < right.host_serv;
      }
      return tag < right.tag;
    }
  };

  using service_tag = multi_index::multi_index_container<
      service_tag_element,
      multi_index::indexed_by<
          multi_index::ordered_non_unique<
              multi_index::member<service_tag_element,
                                  host_serv_pair,
                                  &service_tag_element::host_serv>>,
          multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(service_tag_element, uint64_t, tag)>,
          multi_index::ordered_unique<
              multi_index::identity<service_tag_element>>>,
      managed_mapped_file::allocator<service_tag_element>::type>;

  id_to_metric_info* _metric_info;
  metric_info_allocator* _metric_info_allocator;
  index_id_mapping* _index_id_mapping;

  id_to_host* _id_to_host;
  id_to_serv* _id_to_service;
  id_to_string* _id_to_instance;
  host_group* _host_group;
  service_group* _service_group;
  id_to_tag* _id_to_tag;
  host_tag* _host_tag;
  service_tag* _serv_tag;

  void managed_map(bool create) override;

 public:
  global_cache_data(const std::string& file_path) : global_cache(file_path) {}

  void set_metric_info(uint64_t metric_id,
                       uint64_t index_id,
                       const absl::string_view& name,
                       const absl::string_view& unit,
                       double min,
                       double max) override;

  void store_instance(uint64_t instance_id,
                      const absl::string_view& instance_name) override;

  void store_host(uint64_t host_id,
                  const absl::string_view& host_name,
                  uint64_t resource_id,
                  uint64_t severity_id) override;

  void store_service(uint64_t host_id,
                     uint64_t service_id,
                     const absl::string_view& service_description,
                     uint64_t resource_id,
                     uint64_t severity_id) override;

  void set_index_mapping(uint64_t index_id,
                         uint64_t host_id,
                         uint64_t service_id) override;

  const metric_info* get_metric_info(uint32_t metric_id) const override;

  const resource_info* get_host(uint64_t host_id) const override;
  const resource_info* get_host_from_index_id(uint64_t index_id) const override;
  const resource_info* get_service(uint64_t host_id,
                                   uint64_t service_id) const override;
  const resource_info* get_service_from_index_id(
      uint64_t index_id) const override;

  const host_serv_pair* get_host_serv_id(uint64_t index_id) const override;

  const string* get_instance_name(uint64_t instance_id) const override;

  virtual void add_host_group(uint64_t group, uint64_t host) override;
  virtual void remove_host_from_group(uint64_t group, uint64_t host) override;
  virtual void remove_host_group(uint64_t group) override;

  virtual void add_service_group(uint64_t group,
                                 uint64_t host,
                                 uint64_t service) override;
  virtual void remove_service_from_group(uint64_t group,
                                         uint64_t host,
                                         uint64_t service) override;
  virtual void remove_service_group(uint64_t group) override;
  virtual void append_service_group(uint64_t host,
                                    uint64_t service,
                                    std::ostream& request_body) override;
  virtual void append_host_group(uint64_t host,
                                 std::ostream& request_body) override;

  void add_tag(uint64_t tag_id,
               const absl::string_view& tag_name,
               TagType tag_type,
               uint64_t poller_id) override;

  void remove_tag(uint64_t tag_id) override;
  void set_host_tag(uint64_t host, tag_id_enumerator&& tag_filler) override;
  void set_serv_tag(uint64_t host,
                    uint64_t serv,
                    tag_id_enumerator&& tag_filler) override;
  void append_host_tag_id(uint64_t host,
                          TagType tag_type,
                          std::ostream& request_body) override;
  void append_serv_tag_id(uint64_t host,
                          uint64_t serv,
                          TagType tag_type,
                          std::ostream& request_body) override;
  void append_host_tag_name(uint64_t host,
                            TagType tag_type,
                            std::ostream& request_body) override;
  void append_serv_tag_name(uint64_t host,
                            uint64_t serv,
                            TagType tag_type,
                            std::ostream& request_body) override;
};

};  // namespace cache

CCB_END()

#endif
