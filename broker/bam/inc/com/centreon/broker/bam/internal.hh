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

#ifndef CCB_BAM_INTERNAL_HH
#define CCB_BAM_INTERNAL_HH

#include "bbdo/bam.pb.h"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

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

}  // namespace bam

CCB_END()

#endif
