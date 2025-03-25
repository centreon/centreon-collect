/**
 * Copyright 2011-2013 Centreon
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

#ifndef CCB_STORAGE_METRIC_HH
#define CCB_STORAGE_METRIC_HH

#include "bbdo/storage.pb.h"
#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/timestamp.hh"

namespace com::centreon::broker::storage {
/**
 *  @class metric metric.hh "com/centreon/broker/storage/metric.hh"
 *  @brief Metric information.
 *
 *  Metric information, mainly used to update RRD files.
 */
class metric : public io::data {
 public:
  metric();
  metric(uint32_t host_id, uint32_t service_id, const std::string& name,
         timestamp time, uint32_t interval, bool is_for_rebuild,
         uint32_t metric_id, int32_t rrd_len, double value, int16_t value_type);
  metric(const metric&) = delete;
  ~metric() noexcept = default;
  metric& operator=(const metric&) = delete;

  /**
   *  Get the type of this event.
   *
   *  @return  The event type.
   */
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::storage, storage::de_metric>::value;
  }

  void convert_to_pb(Metric& pb_metric) const;

  timestamp time;
  uint32_t interval;
  bool is_for_rebuild;
  uint32_t metric_id;
  std::string name;
  int rrd_len;
  double value;
  int16_t value_type;
  uint32_t host_id;
  uint32_t service_id;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::storage

#endif  // !CCB_STORAGE_METRIC_HH
