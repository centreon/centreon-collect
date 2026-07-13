/**
 * Copyright 2022-2023 Centreon
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

#ifndef CCB_BAM_INTERNAL_HH
#define CCB_BAM_INTERNAL_HH

#include "bam.pb.h"
#include "bbdo/bam_state.pb.h"
#include "broker.pb.h"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"

namespace com::centreon::broker {

namespace bam {

using pb_inherited_downtime =
    io::protobuf<InheritedDowntime,
                 make_type(io::bam, bam::de_pb_inherited_downtime)>;

using pb_ba_status =
    io::protobuf<BaStatus, make_type(io::bam, bam::de_pb_ba_status)>;

using pb_ba_event =
    io::protobuf<BaEvent, make_type(io::bam, bam::de_pb_ba_event)>;

using pb_kpi_event =
    io::protobuf<KpiEvent, make_type(io::bam, bam::de_pb_kpi_event)>;
using pb_dimension_bv_event =
    io::protobuf<DimensionBvEvent,
                 make_type(io::bam, bam::de_pb_dimension_bv_event)>;

using pb_dimension_ba_bv_relation_event =
    io::protobuf<DimensionBaBvRelationEvent,
                 make_type(io::bam, bam::de_pb_dimension_ba_bv_relation_event)>;

using pb_dimension_timeperiod =
    io::protobuf<DimensionTimeperiod,
                 make_type(io::bam, bam::de_pb_dimension_timeperiod)>;

using pb_dimension_ba_event =
    io::protobuf<DimensionBaEvent,
                 make_type(io::bam, bam::de_pb_dimension_ba_event)>;

using pb_dimension_kpi_event =
    io::protobuf<DimensionKpiEvent,
                 make_type(io::bam, bam::de_pb_dimension_kpi_event)>;

using pb_kpi_status =
    io::protobuf<KpiStatus, make_type(io::bam, bam::de_pb_kpi_status)>;

using pb_ba_duration_event =
    io::protobuf<BaDurationEvent,
                 make_type(io::bam, bam::de_pb_ba_duration_event)>;

using pb_dimension_ba_timeperiod_relation =
    io::protobuf<DimensionBaTimeperiodRelation,
                 make_type(io::bam,
                           bam::de_pb_dimension_ba_timeperiod_relation)>;

using pb_dimension_truncate_table_signal =
    io::protobuf<DimensionTruncateTableSignal,
                 make_type(io::bam,
                           bam::de_pb_dimension_truncate_table_signal)>;

using pb_services_book_state =
    io::protobuf<ServicesBookState,
                 make_type(io::bam, bam::de_pb_services_book_state)>;

}  // namespace bam

/* We have to declare the pb_ba_info also here because we don't control the
 * order things are created. If the bam stream is created before brokerrpc, its
 * muxer will be declared with known events (so without pb_ba_info) and if we
 * want pb_ba_info to be known, thenwe have to force its declaration. */
namespace extcmd {
using pb_ba_info =
    io::protobuf<BaInfo, make_type(io::extcmd, extcmd::de_ba_info)>;
}  // namespace extcmd

}  // namespace com::centreon::broker

#endif
