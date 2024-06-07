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
#ifndef CCE_MOD_OTL_SERVER_DATA_POINT_FIFO_HH
#define CCE_MOD_OTL_SERVER_DATA_POINT_FIFO_HH

#include "otl_data_point.hh"

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief This class is a multiset of opentelemetry otl_data_point ordered by
 * nano_timestamp
 *
 */
class data_point_fifo {
  struct time_unix_nano_compare {
    /**
     * @brief mandatory for heterogenous search (abseil or standard associative
     * (C++20))
     * https://en.cppreference.com/w/cpp/utility/functional
     *
     */
    using is_transparent = void;
    bool operator()(const otl_data_point& left,
                    const otl_data_point& right) const {
      return left.get_nano_timestamp() < right.get_nano_timestamp();
    }
    bool operator()(const otl_data_point& left,
                    uint64_t nano_timestamp_right) const {
      return left.get_nano_timestamp() < nano_timestamp_right;
    }
    bool operator()(uint64_t nano_timestamp_left,
                    const otl_data_point& right) const {
      return nano_timestamp_left < right.get_nano_timestamp();
    }
  };

 public:
  using container =
      absl::btree_multiset<otl_data_point, time_unix_nano_compare>;

 private:
  static time_t _second_datapoint_expiry;
  static size_t _max_size;

  container _fifo;

 public:
  const container& get_fifo() const { return _fifo; }

  bool empty() const { return _fifo.empty(); }

  void clear() { _fifo.clear(); }

  size_t size() const { return _fifo.size(); }

  void add_data_point(const otl_data_point& data_pt);

  void clean();

  void clean_oldest(uint64_t expiry);

  static void update_fifo_limit(time_t second_datapoint_expiry,
                                size_t max_size);
};

using metric_name_to_fifo = absl::flat_hash_map<std::string, data_point_fifo>;

}  // namespace com::centreon::engine::modules::opentelemetry

#endif
