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

#ifndef CCB_LUA_INTERNAL_HH
#define CCB_LUA_INTERNAL_HH

#include "bbdo/events.hh"
#include "bbdo/storage.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/neb/internal.hh"

namespace com::centreon::broker::storage {

using pb_index_mapping =
    io::protobuf<IndexMapping,
                 make_type(io::storage, storage::de_pb_index_mapping)>;
using pb_metric_mapping =
    io::protobuf<MetricMapping,
                 make_type(io::storage, storage::de_pb_metric_mapping)>;

}  // namespace com::centreon::broker::storage

#endif  // !CCB_LUA_INTERNAL_HH
