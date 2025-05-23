/*
** Copyright 2011-2021 Centreon
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

#ifndef CCB_STORAGE_STATUS_HH
#define CCB_STORAGE_STATUS_HH

#include "bbdo/storage.pb.h"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker::storage {
/**
 *  @class status status.hh "com/centreon/broker/storage/status.hh"
 *  @brief Status data used to generate status graphs.
 *
 *  Status data event, mainly used to generate status graphs.
 */
class status : public io::data {
  void _internal_copy(status const& s);

 public:
  timestamp time;
  uint64_t index_id;
  uint32_t interval;
  bool is_for_rebuild;
  timestamp rrd_len;
  short state;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;

  status();
  status(timestamp const& time,
         uint64_t index_id,
         uint32_t interval,
         bool is_for_rebuild,
         timestamp const& rrd_len,
         int16_t state);
  status(status const& s);
  ~status();

  void convert_to_pb(Status& pb_status) const;

  status& operator=(status const& s);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::storage, storage::de_status>::value;
  }
};
}  // namespace com::centreon::broker::storage

#endif  // !CCB_STORAGE_STATUS_HH
