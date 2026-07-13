/**
 * Copyright 2020-2024 Centreon
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

#ifndef CCB_UNIFIED_SQL_INTERNAL_HH
#define CCB_UNIFIED_SQL_INTERNAL_HH

#include "bbdo/bbdo.pb.h"
#include "bbdo/events.hh"
#include "bbdo/neb.pb.h"
#include "bbdo/rebuild_message.pb.h"
#include "bbdo/remove_graph_message.pb.h"
#include "bbdo/storage.pb.h"
#include "broker/core/src/broker.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/neb/internal.hh"

namespace com::centreon::broker {

namespace bbdo {
/**
 * Here is a declaration of pb_rebuild_metrics which is a bbdo event we use to
 * ask rebuild of metrics. MetricIds is a vector of metric ids to rebuild. */
using pb_rebuild_graphs =
    io::protobuf<IndexIds, make_type(io::bbdo, bbdo::de_rebuild_graphs)>;
using pb_remove_graphs =
    io::protobuf<ToRemove, make_type(io::bbdo, bbdo::de_remove_graphs)>;
using pb_remove_poller =
    io::protobuf<GenericNameOrIndex,
                 make_type(io::bbdo, bbdo::de_remove_poller)>;
}  // namespace bbdo

namespace local {
using pb_stop = io::protobuf<Stop, make_type(io::local, local::de_pb_stop)>;
}  // namespace local

namespace storage {
/**
 * Here is the declaration of the message sent by unified_sql to rrd to rebuild
 * metrics. */
using pb_rebuild_message =
    io::protobuf<RebuildMessage,
                 make_type(io::storage, storage::de_rebuild_message)>;
using pb_remove_graph_message =
    io::protobuf<RemoveGraphMessage,
                 make_type(io::storage, storage::de_remove_graph_message)>;
using pb_metric =
    io::protobuf<Metric, make_type(io::storage, storage::de_pb_metric)>;
using pb_status =
    io::protobuf<Status, make_type(io::storage, storage::de_pb_status)>;
using pb_index_mapping =
    io::protobuf<IndexMapping,
                 make_type(io::storage, storage::de_pb_index_mapping)>;
using pb_metric_mapping =
    io::protobuf<MetricMapping,
                 make_type(io::storage, storage::de_pb_metric_mapping)>;

}  // namespace storage
}  // namespace com::centreon::broker

#endif  // !CCB_UNIFIED_SQL_INTERNAL_HH
