/*
** Copyright 2021 Centreon
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

#ifndef CCB_STORAGE_REBUILD2_HH
#define CCB_STORAGE_REBUILD2_HH

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/event_info.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/storage/internal.hh"
#include "rebuild.pb.h"

CCB_BEGIN()

namespace storage {
/**
 *  @class rebuild2 rebuild2.hh "com/centreon/broker/storage/rebuild2.hh"
 *  @brief Rebuild event.
 *
 *  This event is generated when some graph need to be rebuild.
 */
class rebuild2 : public io::data {
 public:
  Rebuild obj;

  rebuild2();
  rebuild2(const rebuild2&) = delete;
  ~rebuild2() noexcept = default;
  rebuild2& operator=(const rebuild2&) = delete;
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::events::protobuf,
                                 storage::de_rebuild2>::value;
  }

  static io::event_info::event_operations const operations;
};
}  // namespace storage

CCB_END()

#endif  // !CCB_STORAGE_REBUILD2_HH
