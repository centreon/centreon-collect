/**
 * Copyright 2014, 2021-2023 Centreon
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

#ifndef CCB_BAM_KPI_EVENT_HH
#define CCB_BAM_KPI_EVENT_HH

#include "bbdo/events.hh"
#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/event_info.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/timestamp.hh"

namespace com::centreon::broker::bam {
/**
 *  @class kpi_event kpi_event.hh "com/centreon/broker/bam/kpi_event.hh"
 *  @brief Kpi event
 *
 *  This is the base KPI event that will fill the kpi_events table.
 */
class kpi_event : public io::data {
 public:
  kpi_event(uint32_t kpi_id, uint32_t ba_id, time_t start_time);
  kpi_event(const kpi_event& other);
  ~kpi_event() noexcept = default;
  kpi_event& operator=(const kpi_event& other);
  bool operator==(const kpi_event& other) const;
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::bam, bam::de_kpi_event>::value;
  }

  timestamp end_time;
  uint32_t kpi_id;
  int impact_level;
  bool in_downtime;
  std::string output;
  std::string perfdata;
  timestamp start_time;
  short status;
  uint32_t ba_id;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_KPI_EVENT_HH
