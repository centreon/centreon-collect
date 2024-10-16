/**
 * Copyright 2014-2023 Centreon
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

#ifndef CCB_BAM_REBUILD_HH
#define CCB_BAM_REBUILD_HH

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker::bam {
/**
 *  @class rebuild rebuild.hh "com/centreon/broker/bam/rebuild.hh"
 *  @brief ask for a rebuild.
 *
 *  This data event represent a rebuild asked.
 */
class rebuild : public io::data {
 public:
  rebuild();
  rebuild(const std::string& lst);
  ~rebuild() noexcept = default;
  rebuild(rebuild const&);
  rebuild& operator=(rebuild const&) = delete;
  bool operator==(rebuild const& other) const;
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::bam, bam::de_rebuild>::value;
  }

  std::string bas_to_rebuild;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_REBUILD_HH
