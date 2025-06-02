/**
 * Copyright 2009-2013,2015-2024 Centreon
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

#include <string>

#include <string_view>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <google/protobuf/descriptor.h>

#include <boost/asio.hpp>

namespace asio = boost::asio;

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

using log_v2 = com::centreon::common::log_v2::log_v2;

static uint32_t neb_instances(0);

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
char const* broker_module_version = CENTREON_BROKER_VERSION;

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  if (!--neb_instances) {
    //      // Remove factory.
    //      io::protocols::instance().unreg("node_events");

    // Remove events.
    io::events::instance().unregister_category(io::neb);
  }
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg [[maybe_unused]]) {
  auto logger = log_v2::instance().get(log_v2::CONFIG);
  if (!neb_instances++) {
    logger->info("NEB: module for Centreon Broker {}", CENTREON_BROKER_VERSION);
    io::events& e(io::events::instance());

    // Register events.
    {
      e.register_event(make_type(io::neb, neb::de_acknowledgement),
                       "acknowledgement", &neb::acknowledgement::operations,
                       neb::acknowledgement::entries, "acknowledgements");
      e.register_event(make_type(io::neb, neb::de_comment), "comment",
                       &neb::comment::operations, neb::comment::entries,
                       "comments");
      e.register_event(make_type(io::neb, neb::de_custom_variable),
                       "custom_variable", &neb::custom_variable::operations,
                       neb::custom_variable::entries, "customvariables");
      e.register_event(make_type(io::neb, neb::de_custom_variable_status),
                       "custom_variable_status",
                       &neb::custom_variable_status::operations,
                       neb::custom_variable_status::entries, "customvariables");
      e.register_event(make_type(io::neb, neb::de_downtime), "downtime",
                       &neb::downtime::operations, neb::downtime::entries,
                       "downtimes");
      e.register_event(make_type(io::neb, neb::de_host_check), "host_check",
                       &neb::host_check::operations, neb::host_check::entries,
                       "hosts");
      e.register_event(make_type(io::neb, neb::de_host), "host",
                       &neb::host::operations, neb::host::entries, "hosts");
      e.register_event(make_type(io::neb, neb::de_host_group), "host_group",
                       &neb::host_group::operations, neb::host_group::entries,
                       "hostgroups");
      e.register_event(make_type(io::neb, neb::de_host_group_member),
                       "host_group_member", &neb::host_group_member::operations,
                       neb::host_group_member::entries, "hosts_hostgroups");
      e.register_event(make_type(io::neb, neb::de_host_parent), "host_parent",
                       &neb::host_parent::operations, neb::host_parent::entries,
                       "hosts_hosts_parents");
      e.register_event(make_type(io::neb, neb::de_host_status), "host_status",
                       &neb::host_status::operations, neb::host_status::entries,
                       "hosts");
      e.register_event(make_type(io::neb, neb::de_instance), "instance",
                       &neb::instance::operations, neb::instance::entries,
                       "instances");
      e.register_event(make_type(io::neb, neb::de_instance_status),
                       "instance_status", &neb::instance_status::operations,
                       neb::instance_status::entries, "instances");
      e.register_event(make_type(io::neb, neb::de_log_entry), "log_entry",
                       &neb::log_entry::operations, neb::log_entry::entries,
                       "logs");
      e.register_event(make_type(io::neb, neb::de_service_check),
                       "service_check", &neb::service_check::operations,
                       neb::service_check::entries, "services");
      e.register_event(make_type(io::neb, neb::de_service), "service",
                       &neb::service::operations, neb::service::entries,
                       "services");
      e.register_event(make_type(io::neb, neb::de_service_group),
                       "service_group", &neb::service_group::operations,
                       neb::service_group::entries, "servicegroups");
      e.register_event(
          make_type(io::neb, neb::de_service_group_member),
          "service_group_member", &neb::service_group_member::operations,
          neb::service_group_member::entries, "services_servicegroups");
      e.register_event(make_type(io::neb, neb::de_service_status),
                       "service_status", &neb::service_status::operations,
                       neb::service_status::entries, "services");
      e.register_event(make_type(io::neb, neb::de_instance_configuration),
                       "instance_configuration",
                       &neb::instance_configuration::operations,
                       neb::instance_configuration::entries);
      e.register_event(make_type(io::neb, neb::de_responsive_instance),
                       "responsive_instance",
                       &neb::responsive_instance::operations,
                       neb::responsive_instance::entries);

      e.register_event(make_type(io::neb, neb::de_pb_downtime), "Downtime",
                       &neb::pb_downtime::operations, "downtimes");
      e.register_event(make_type(io::neb, neb::de_pb_service), "Service",
                       &neb::pb_service::operations, "services");
      e.register_event(make_type(io::neb, neb::de_pb_adaptive_service),
                       "AdaptiveService", &neb::pb_adaptive_service::operations,
                       "services");
      e.register_event(make_type(io::neb, neb::de_pb_service_status),
                       "ServiceStatus", &neb::pb_service_status::operations,
                       "services");
      e.register_event(make_type(io::neb, neb::de_pb_adaptive_service_status),
                       "AdaptiveServiceStatus",
                       &neb::pb_adaptive_service_status::operations,
                       "services");

      e.register_event(make_type(io::neb, neb::de_pb_host), "Host",
                       &neb::pb_host::operations, "hosts");
      e.register_event(make_type(io::neb, neb::de_pb_adaptive_host),
                       "AdaptiveHost", &neb::pb_adaptive_host::operations,
                       "hosts");

      e.register_event(make_type(io::neb, neb::de_pb_host_status), "HostStatus",
                       &neb::pb_host_status::operations, "hosts");
      e.register_event(make_type(io::neb, neb::de_pb_adaptive_host_status),
                       "AdaptiveHostStatus",
                       &neb::pb_adaptive_host_status::operations, "hosts");

      e.register_event(make_type(io::neb, neb::de_pb_severity), "Severity",
                       &neb::pb_severity::operations, "severities");

      e.register_event(make_type(io::neb, neb::de_pb_tag), "Tag",
                       &neb::pb_tag::operations, "tags");

      e.register_event(make_type(io::neb, neb::de_pb_comment), "Comment",
                       &neb::pb_comment::operations, "comments");

      e.register_event(make_type(io::neb, neb::de_pb_custom_variable),
                       "CustomVariables", &neb::pb_custom_variable::operations,
                       "customvariables");

      e.register_event(make_type(io::neb, neb::de_pb_custom_variable_status),
                       "CustomVariablesStatus",
                       &neb::pb_custom_variable_status::operations,
                       "customvariables");

      e.register_event(make_type(io::neb, neb::de_pb_host_check), "HostCheck",
                       &neb::pb_host_check::operations, "hosts");

      e.register_event(make_type(io::neb, neb::de_pb_service_check),
                       "ServiceCheck", &neb::pb_service_check::operations,
                       "services");

      e.register_event(make_type(io::neb, neb::de_pb_log_entry), "LogEntry",
                       &neb::pb_log_entry::operations, "logs");

      e.register_event(make_type(io::neb, neb::de_pb_instance_status),
                       "InstanceStatus", &neb::pb_instance_status::operations,
                       "instances");

      e.register_event(make_type(io::neb, neb::de_pb_instance), "Instance",
                       &neb::pb_instance::operations, "instances");

      e.register_event(make_type(io::neb, neb::de_pb_responsive_instance),
                       "ResponsiveInstance",
                       &neb::pb_responsive_instance::operations, "instances");
      e.register_event(make_type(io::neb, neb::de_pb_acknowledgement),
                       "Acknowledgement", &neb::pb_acknowledgement::operations,
                       "acknowledgements");
      e.register_event(neb::pb_host_group::static_type(), "HostGroup",
                       &neb::pb_host_group::operations, "hostgroups");
      e.register_event(
          neb::pb_host_group_member::static_type(), "HostGroupMember",
          &neb::pb_host_group_member::operations, "hosts_hostgroups");
      e.register_event(neb::pb_service_group::static_type(), "ServiceGroup",
                       &neb::pb_service_group::operations, "servicegroups");
      e.register_event(
          neb::pb_service_group_member::static_type(), "ServiceGroupMember",
          &neb::pb_service_group_member::operations, "services_servicegroups");
      e.register_event(neb::pb_host_parent::static_type(), "HostParent",
                       &neb::pb_host_parent::operations, "hosts_hosts_parents");
      e.register_event(neb::pb_instance_configuration::static_type(),
                       "InstanceConfiguration",
                       &neb::pb_instance_configuration::operations, "no_table");
      e.register_event(neb::pb_otl_metrics::static_type(), "OTLMetrics",
                       &neb::pb_otl_metrics::operations, "otl_metrics");
      e.register_event(neb::pb_agent_stats::static_type(), "AgentStats",
                       &neb::pb_agent_stats::operations, "agent_information");
    }
  }
}
}
