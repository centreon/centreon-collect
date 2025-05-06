/**
 * Copyright 2009-2015, 2021-2024 Centreon
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

#ifndef CCB_NEB_INTERNAL_HH
#define CCB_NEB_INTERNAL_HH

#include <absl/hash/hash.h>
#include "bbdo/events.hh"
#include "bbdo/neb.pb.h"
#include "bbdo/storage.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/callback.hh"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"

namespace com::centreon::broker {

namespace neb {
// Forward declaration.
class acknowledgement;

// Sender object.
extern multiplexing::publisher gl_publisher;

// Registered callbacks.
extern std::list<std::unique_ptr<neb::callback>> gl_registered_callbacks;

using pb_downtime =
    io::protobuf<Downtime, make_type(io::neb, neb::de_pb_downtime)>;

using pb_host_status =
    io::protobuf<HostStatus, make_type(io::neb, neb::de_pb_host_status)>;
using pb_adaptive_host_status =
    io::protobuf<AdaptiveHostStatus,
                 make_type(io::neb, neb::de_pb_adaptive_host_status)>;
using pb_host = io::protobuf<Host, make_type(io::neb, neb::de_pb_host)>;
using pb_adaptive_host =
    io::protobuf<AdaptiveHost, make_type(io::neb, neb::de_pb_adaptive_host)>;

using pb_service =
    io::protobuf<Service, make_type(io::neb, neb::de_pb_service)>;
using pb_adaptive_service =
    io::protobuf<AdaptiveService,
                 make_type(io::neb, neb::de_pb_adaptive_service)>;

using pb_service_status =
    io::protobuf<ServiceStatus, make_type(io::neb, neb::de_pb_service_status)>;

using pb_adaptive_service_status =
    io::protobuf<AdaptiveServiceStatus,
                 make_type(io::neb, neb::de_pb_adaptive_service_status)>;

using pb_severity =
    io::protobuf<Severity, make_type(io::neb, neb::de_pb_severity)>;

using pb_tag = io::protobuf<Tag, make_type(io::neb, neb::de_pb_tag)>;

using pb_comment =
    io::protobuf<Comment, make_type(io::neb, neb::de_pb_comment)>;

using pb_custom_variable =
    io::protobuf<CustomVariable,
                 make_type(io::neb, neb::de_pb_custom_variable)>;
using pb_custom_variable_status =
    io::protobuf<CustomVariable,
                 make_type(io::neb, neb::de_pb_custom_variable_status)>;

using pb_host_check =
    io::protobuf<HostCheck, make_type(io::neb, neb::de_pb_host_check)>;

using pb_service_check =
    io::protobuf<ServiceCheck, make_type(io::neb, neb::de_pb_service_check)>;

using pb_log_entry =
    io::protobuf<LogEntry, make_type(io::neb, neb::de_pb_log_entry)>;

using pb_instance_status =
    io::protobuf<InstanceStatus,
                 make_type(io::neb, neb::de_pb_instance_status)>;

using pb_instance =
    io::protobuf<Instance, make_type(io::neb, neb::de_pb_instance)>;

using pb_responsive_instance =
    io::protobuf<ResponsiveInstance,
                 make_type(io::neb, neb::de_pb_responsive_instance)>;

using pb_acknowledgement =
    io::protobuf<Acknowledgement,
                 make_type(io::neb, neb::de_pb_acknowledgement)>;

using pb_host_group =
    io::protobuf<HostGroup, make_type(io::neb, neb::de_pb_host_group)>;

using pb_service_group =
    io::protobuf<ServiceGroup, make_type(io::neb, neb::de_pb_service_group)>;

using pb_host_group_member =
    io::protobuf<HostGroupMember,
                 make_type(io::neb, neb::de_pb_host_group_member)>;

using pb_service_group_member =
    io::protobuf<ServiceGroupMember,
                 make_type(io::neb, neb::de_pb_service_group_member)>;
using pb_host_parent =
    io::protobuf<HostParent, make_type(io::neb, neb::de_pb_host_parent)>;
using pb_instance_configuration =
    io::protobuf<InstanceConfiguration,
                 make_type(io::neb, neb::de_pb_instance_configuration)>;

using pb_otl_metrics = io::protobuf<
    opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest,
    make_type(io::storage, storage::de_pb_otl_metrics)>;

using pb_agent_stats =
    io::protobuf<AgentStats, make_type(io::neb, neb::de_pb_agent_stats)>;

}  // namespace neb

namespace storage {
using pb_index_mapping =
    io::protobuf<IndexMapping,
                 make_type(io::storage, storage::de_pb_index_mapping)>;

using pb_metric_mapping =
    io::protobuf<MetricMapping,
                 make_type(io::storage, storage::de_pb_metric_mapping)>;

}  // namespace storage

}  // namespace com::centreon::broker

#endif  // !CCB_NEB_INTERNAL_HH
