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
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_set.hpp>

#include "global_cache.hh"

CCB_BEGIN()

namespace cache {

namespace multi_index = boost::multi_index;

/**
 * @brief string_string_view_equal and string_string_view_hash are mandatory to
 * find a string_view in a string container
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

class global_cache_data : public global_cache {
  using id_to_metric_info = interprocess::flat_map<
      uint64_t,
      metric_info,
      std::less<uint64_t>,
      interprocess::allocator<std::pair<uint64_t, metric_info>,
                              segment_manager>>;

  using int64_int64_map = interprocess::flat_map<
      uint64_t,
      uint64_t,
      std::less<uint64_t>,
      interprocess::allocator<std::pair<uint64_t, uint64_t>, segment_manager>>;

  using id_to_string = interprocess::flat_map<
      uint64_t,
      string,
      std::less<uint64_t>,
      interprocess::allocator<std::pair<uint64_t, string>, segment_manager>>;

  using dictionnary =
      boost::unordered_set<string,
                           string_string_view_hash,
                           std::equal_to<string>,
                           interprocess::allocator<string, segment_manager>>;

  id_to_metric_info* _metric_info;
  int64_int64_map _index_id_to_metric_id;

  struct index_data {
    uint64_t index_id;
    std::pair<uint64_t /*host_id*/, uint64_t /*service_id*/> host_serv;
  };

  /**
   * @brief index
   *
   */
  using index_mapping_table = multi_index::multi_index_container<
      index_data,
      multi_index::indexed_by<
          multi_index::ordered_unique<
              BOOST_MULTI_INDEX_MEMBER(index_data, uint64_t, index_id)>,
          multi_index::ordered_unique<
              multi_index::member<index_data,
                                  std::pair<uint64_t, uint64_t>,
                                  &index_data::host_serv>>>,
      interprocess::allocator<index_data, segment_manager>>;

  index_mapping_table* _index_mapping;
  id_to_string* _id_to_host;
  id_to_string* _id_to_service_description;

  // unit and name are many times identicals, so these dictionnary reduce file
  // size and allocation
  dictionnary* _metric_name;
  dictionnary* _metric_unit;

  void managed_map(bool create) override;

  const string& get_from_dictionnary(dictionnary& dico,
                                     const absl::string_view& value);
  const string& get_metric_name(const absl::string_view& name) {
    return get_from_dictionnary(*_metric_name, name);
  }
  const string& get_metric_unit(const absl::string_view& unit) {
    return get_from_dictionnary(*_metric_unit, unit);
  }

 public:
  global_cache_data(const std::string& file_path) : global_cache(file_path) {}
  void set_metric_info(uint64_t metric_id,
                       uint64_t index_id,
                       const absl::string_view& name,
                       const absl::string_view& unit,
                       double min,
                       double max) override;

  void store_host(uint64_t host_id,
                  const absl::string_view& host_name) override;

  void store_service(uint64_t host_id,
                     uint64_t service_id,
                     const absl::string_view& service_description) override;

  void set_index_mapping(uint64_t index_id,
                         uint64_t host_id,
                         uint64_t service_id) override;

  const metric_info* get_metric_info(uint32_t metric_id) const override;
};

};  // namespace cache

CCB_END()

#endif
