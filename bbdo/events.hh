/*
 * Copyright 2021-2023 Centreon
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

#ifndef CC_BROKER_EVENTS_HH
#define CC_BROKER_EVENTS_HH

#include <cstdint>
#include <cstring>

namespace com::centreon::broker {
namespace io {
enum data_category {
  none = 0,
  neb = 1,
  bbdo = 2,
  storage = 3,
  dumper = 5,
  bam = 6,
  extcmd = 7,
  generator = 8,
  local = 9,
  max_data_category = 10,
  internal = 65535
};
constexpr uint16_t category_id(const char* name) {
  if (std::string_view("neb", 3) == name)
    return neb;
  if (std::string_view("bbdo", 4) == name)
    return bbdo;
  if (std::string_view("storage", 7) == name)
    return storage;
  if (std::string_view("internal", 8) == name)
    return internal;
  if (std::string_view("bam", 3) == name)
    return bam;
  if (std::string_view("extcmd", 6) == name)
    return extcmd;
  if (std::string_view("dumper", 6) == name)
    return dumper;
  if (std::string_view("generator", 9) == name)
    return generator;
  if (std::string_view("local", 5) == name)
    return local;
  return none;
}
constexpr const char* category_name(data_category cat) {
  switch (cat) {
    case neb:
      return "neb";
    case bbdo:
      return "bbdo";
    case storage:
      return "storage";
    case dumper:
      return "dumper";
    case bam:
      return "bam";
    case extcmd:
      return "extcmd";
    case generator:
      return "generator";
    case internal:
      return "internal";
    case local:
      return "local";
    default:
      return "unknown category";
  }
}
}  // namespace io
namespace bbdo {
enum data_element {
  de_version_response = 1,
  de_ack = 2,
  de_stop = 3,
  de_rebuild_graphs = 4,
  de_remove_graphs = 5,
  de_remove_poller = 6,
  de_welcome = 7,
  de_pb_ack = 8,
  de_pb_stop = 9,
  de_pb_diff_state = 10,
  de_pb_diff_state_ack = 11,
};
}
namespace neb {
// Data elements.
enum data_element {
  de_acknowledgement = 1,
  de_comment,
  de_custom_variable,
  de_custom_variable_status,
  de_downtime,
  de_event_handler = 6,    // unused
  de_flapping_status = 7,  // unused
  de_host_check = 8,
  de_host_dependency = 9,
  de_host_group,
  de_host_group_member = 11,
  de_host,
  de_host_parent,
  de_host_status,
  de_instance,
  de_instance_status,
  de_log_entry,
  de_module,
  de_service_check,
  de_service_dependency,
  de_service_group,
  de_service_group_member,
  de_service,
  de_service_status,
  de_instance_configuration = 25,
  de_responsive_instance = 26,
  de_pb_service = 27,
  de_pb_adaptive_service = 28,
  de_pb_service_status = 29,
  de_pb_host = 30,
  de_pb_adaptive_host = 31,
  de_pb_host_status = 32,
  de_pb_severity = 33,
  de_pb_tag = 34,
  de_pb_comment = 35,
  de_pb_downtime = 36,
  de_pb_custom_variable = 37,
  de_pb_custom_variable_status = 38,
  de_pb_host_check = 39,
  de_pb_service_check = 40,
  de_pb_log_entry = 41,
  de_pb_instance_status = 42,
  de_pb_global_diff_state = 43,
  de_pb_instance = 44,
  de_pb_acknowledgement = 45,
  de_pb_responsive_instance = 46,
  de_pb_host_dependency = 47,
  de_pb_service_dependency = 48,
  de_pb_host_group = 49,
  de_pb_host_group_member = 50,
  de_pb_service_group = 51,
  de_pb_service_group_member = 52,
  de_pb_host_parent = 53,
  de_pb_instance_configuration = 54,
  de_pb_adaptive_service_status = 55,
  de_pb_adaptive_host_status = 56,
  de_pb_agent_stats = 57
};
}  // namespace neb
namespace storage {
enum data_element {
  de_metric = 1,
  de_rebuild = 2,
  de_remove_graph = 3,
  de_status = 4,
  de_index_mapping = 5,
  de_metric_mapping = 6,
  de_rebuild_message = 7,
  de_remove_graph_message = 8,
  de_pb_metric = 9,
  de_pb_status = 10,
  de_pb_index_mapping = 11,
  de_pb_metric_mapping = 12,
  de_pb_otl_metrics =
      13  // contain an
          // ::opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest
};
}
namespace bam {
enum data_element {
  de_ba_status = 1,
  de_kpi_status,
  de_meta_service_status,
  de_ba_event,
  de_kpi_event,
  de_ba_duration_event,
  de_dimension_ba_event,
  de_dimension_kpi_event,
  de_dimension_ba_bv_relation_event,
  de_dimension_bv_event,
  de_dimension_truncate_table_signal,
  de_rebuild,
  de_dimension_timeperiod,
  de_dimension_ba_timeperiod_relation,
  de_dimension_timeperiod_exception,  // not used since 2022-11-28
  de_dimension_timeperiod_exclusion,  // not used since 2022-11-28
  de_inherited_downtime,
  de_pb_inherited_downtime = 18,
  de_pb_ba_status = 19,
  de_pb_ba_event = 20,
  de_pb_kpi_event = 21,
  de_pb_dimension_bv_event = 22,
  de_pb_dimension_ba_bv_relation_event = 23,
  de_pb_dimension_timeperiod = 24,
  de_pb_dimension_ba_event = 25,
  de_pb_dimension_kpi_event = 26,
  de_pb_kpi_status = 27,
  de_pb_ba_duration_event = 28,
  de_pb_dimension_ba_timeperiod_relation = 29,
  de_pb_dimension_truncate_table_signal = 30,
  de_pb_services_book_state = 31,
};
}

namespace extcmd {
enum data_element { de_pb_bench = 1, de_ba_info = 2 };
}

namespace local {
enum data_element {
  de_pb_stop = 1,
};
}

constexpr uint32_t make_type(io::data_category cat, uint32_t elem) {
  return (cat << 16) | elem;
}
constexpr uint16_t category_of_type(uint32_t type) {
  return static_cast<uint16_t>(type >> 16);
}
constexpr uint16_t element_of_type(uint32_t type) {
  return static_cast<uint16_t>(type);
}
}  // namespace com::centreon::broker

#endif /* !CC_BROKER_EVENTS_HH */
