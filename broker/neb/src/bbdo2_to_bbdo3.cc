/**
 * Copyright 2025 Centreon
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

#include <boost/preprocessor/seq/for_each.hpp>

#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/storage/index_mapping.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/host_group_member.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_group.hh"
#include "com/centreon/broker/neb/service_group_member.hh"
#include "neb.pb.h"
#include "storage/metric_mapping.hh"

#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"

using namespace com::centreon::broker::neb;
using namespace com::centreon::broker;

#define s_pb(attrib) obj.set_##attrib(in.attrib);

/**
 * @brief BOOST_PP_SEQ_FOR_EACH expands BOOST_PP_SEQ_FOR_EACH(traduct, ,
 * (name)(pid)); with: traduct(r, , BOOST_PP_SEQ_HEAD((name)(pid))) traduct(r, ,
 * BOOST_PP_SEQ_HEAD((pid)))
 * Then  BOOST_PP_SEQ_HEAD((name)(pid)) returns name
 * and BOOST_PP_SEQ_HEAD((pid) returns pid
 *
 * It's so traduced in s_pb(name) s_pb(pid)
 */
#define traduct(not_used_1, not_used2, seq_head) s_pb(seq_head)

static std::shared_ptr<io::data> _instance_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::instance>(d).get();
  auto pb = std::make_shared<neb::pb_instance>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  Instance& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (engine)(name)(pid)(version));
  obj.set_running(in.is_running);
  obj.set_instance_id(in.poller_id);
  obj.set_end_time(in.program_end.get_time_t());
  obj.set_start_time(in.program_start.get_time_t());

  return pb;
}

static std::shared_ptr<io::data> _host_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::host>(d).get();
  auto pb = std::make_shared<neb::pb_host>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  Host& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(
      traduct, ,
      (
          host_id)(acknowledged)(enabled)(check_command)(check_interval)(check_period)(event_handler_enabled)(event_handler)(execution_time)(last_check)(last_hard_state_change)(last_notification)(notification_number)(last_state_change)(last_time_down)(last_time_unreachable)(last_time_up)(last_update)(latency)(max_check_attempts)(next_check)(no_more_notifications)(output)(percent_state_change)(retry_interval)(should_be_scheduled)(action_url)(address)(alias)(check_freshness)(default_event_handler_enabled)(display_name)(first_notification_delay)(flap_detection_on_down)(flap_detection_on_unreachable)(flap_detection_on_up)(freshness_threshold)(high_flap_threshold)(low_flap_threshold)(icon_image)(icon_image_alt)(notes)(notes_url)(notification_interval)(notification_period)(notify_on_down)(notify_on_downtime)(notify_on_flapping)(notify_on_recovery)(notify_on_unreachable)(stalk_on_down)(stalk_on_unreachable)(stalk_on_up)(statusmap_image)(retain_nonstatus_information)(retain_status_information)(timezone));
  obj.set_acknowledgement_type(static_cast<AckType>(in.acknowledgement_type));
  obj.set_active_checks(in.active_checks_enabled);
  obj.set_scheduled_downtime_depth(in.downtime_depth);
  obj.set_check_type(static_cast<Host_CheckType>(in.check_type));
  obj.set_check_attempt(in.current_check_attempt);
  obj.set_state(static_cast<Host_State>(in.current_state));
  obj.set_flap_detection(in.default_flap_detection_enabled);
  obj.set_checked(in.has_been_checked);
  obj.set_flapping(in.is_flapping);
  obj.set_last_hard_state(static_cast<Host_State>(in.last_hard_state));
  obj.set_next_host_notification(in.next_notification);
  obj.set_notify(in.notifications_enabled);
  obj.set_passive_checks(in.passive_checks_enabled);
  obj.set_perfdata(in.perf_data);
  obj.set_obsess_over_host(in.obsess_over);
  obj.set_state_type(static_cast<Host_StateType>(in.state_type));
  obj.set_default_active_checks(in.default_active_checks_enabled);
  obj.set_default_flap_detection(in.default_flap_detection_enabled);
  obj.set_default_notify(in.default_notifications_enabled);
  obj.set_default_passive_checks(in.default_passive_checks_enabled);
  obj.set_name(in.host_name);
  obj.set_instance_id(in.poller_id);

  return pb;
}

static std::shared_ptr<io::data> _host_group_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::host_group>(d).get();
  auto pb = std::make_shared<neb::pb_host_group>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  HostGroup& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (enabled)(name)(poller_id));
  obj.set_hostgroup_id(in.id);
  return pb;
}

static std::shared_ptr<io::data> _host_group_member_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::host_group_member>(d).get();
  auto pb = std::make_shared<neb::pb_host_group_member>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  HostGroupMember& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (enabled)(host_id)(poller_id));
  obj.set_hostgroup_id(in.group_id);
  obj.set_name(in.group_name);
  return pb;
}

static std::shared_ptr<io::data> _service_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::service>(d).get();
  auto pb = std::make_shared<neb::pb_service>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  Service& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(
      traduct, ,
      (host_id)(service_id)(acknowledged)(enabled)(check_command)(check_interval)(check_period)(event_handler_enabled)(event_handler)(execution_time)(last_check)(last_hard_state_change)(last_notification)(notification_number)(last_state_change)(last_time_ok)(last_time_warning)(last_time_critical)(last_time_unknown)(last_update)(latency)(max_check_attempts)(next_check)(next_notification)(no_more_notifications)(percent_state_change)(retry_interval)(host_name)(should_be_scheduled)(action_url)(check_freshness)(default_event_handler_enabled)(display_name)(first_notification_delay)(flap_detection_on_critical)(flap_detection_on_ok)(flap_detection_on_unknown)(flap_detection_on_warning)(freshness_threshold)(high_flap_threshold)(low_flap_threshold)(icon_image)(icon_image_alt)(is_volatile)(notes)(notes_url)(notification_interval)(notification_period)(notify_on_critical)(notify_on_downtime)(notify_on_flapping)(notify_on_recovery)(notify_on_unknown)(notify_on_warning)(stalk_on_critical)(stalk_on_ok)(stalk_on_unknown)(stalk_on_warning)(retain_nonstatus_information)(retain_status_information));
  obj.set_acknowledgement_type(static_cast<AckType>(in.acknowledgement_type));
  obj.set_active_checks(in.active_checks_enabled);
  obj.set_scheduled_downtime_depth(in.downtime_depth);
  obj.set_check_type(static_cast<Service_CheckType>(in.check_type));
  obj.set_check_attempt(in.current_check_attempt);
  obj.set_state(static_cast<Service_State>(in.current_state));
  obj.set_flap_detection(in.default_flap_detection_enabled);
  obj.set_checked(in.has_been_checked);
  obj.set_flapping(in.is_flapping);
  obj.set_last_hard_state(static_cast<Service_State>(in.last_hard_state));
  obj.set_notify(in.notifications_enabled);
  std::string_view long_output = in.output;
  std::vector<std::string_view> output =
      absl::StrSplit(long_output, absl::MaxSplits('\n', 2));
  switch (output.size()) {
    case 2:
      obj.set_long_output(std::string(output[1]));
      [[fallthrough]];
    case 1:
      obj.set_output(std::string(output[0]));
      break;
  }
  obj.set_passive_checks(in.passive_checks_enabled);
  obj.set_perfdata(in.perf_data);
  obj.set_description(in.service_description);
  obj.set_obsess_over_service(in.obsess_over);
  obj.set_state_type(static_cast<Service_StateType>(in.state_type));
  obj.set_default_active_checks(in.default_active_checks_enabled);
  obj.set_default_flap_detection(in.default_flap_detection_enabled);
  obj.set_default_notify(in.default_notifications_enabled);
  obj.set_default_passive_checks(in.default_passive_checks_enabled);
  if (std::string_view(obj.host_name().data(), 12) == "_Module_Meta") {
    if (std::string_view(obj.description().data(), 5) == "meta_") {
      obj.set_type(METASERVICE);
      uint64_t iid;
      std::string_view id = std::string_view(obj.description()).substr(5);
      if (absl::SimpleAtoi(id, &iid))
        obj.set_internal_id(iid);
    }
  } else if (std::string_view(obj.host_name().data(), 11) == "_Module_BAM") {
    if (std::string_view(obj.description().data(), 3) == "ba_") {
      obj.set_type(BA);
      uint64_t iid;
      std::string_view id = std::string_view(obj.description()).substr(3);
      if (absl::SimpleAtoi(id, &iid))
        obj.set_internal_id(iid);
    }
  }

  return pb;
}

static std::shared_ptr<io::data> _service_group_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::service_group>(d).get();
  auto pb = std::make_shared<neb::pb_service_group>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  ServiceGroup& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (enabled)(name)(poller_id));
  obj.set_servicegroup_id(in.id);
  return pb;
}

static std::shared_ptr<io::data> _service_group_member_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in =
      *std::static_pointer_cast<neb::service_group_member>(d).get();
  auto pb = std::make_shared<neb::pb_service_group_member>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  ServiceGroupMember& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (enabled)(host_id)(poller_id)(service_id));
  obj.set_servicegroup_id(in.group_id);
  obj.set_name(in.group_name);
  return pb;
}

static std::shared_ptr<io::data> _custom_variable_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<neb::custom_variable>(d).get();
  auto pb = std::make_shared<neb::pb_custom_variable>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(
      traduct, ,
      (host_id)(service_id)(modified)(name)(update_time)(value)(default_value)(enabled));
  obj.set_type(static_cast<CustomVariable_VarType>(in.var_type));

  return pb;
}

static std::shared_ptr<io::data> _index_mapping_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<storage::index_mapping>(d).get();
  auto pb = std::make_shared<storage::pb_index_mapping>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (index_id)(host_id)(service_id));

  return pb;
}

static std::shared_ptr<io::data> _metric_mapping_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<storage::metric_mapping>(d).get();
  auto pb = std::make_shared<storage::pb_metric_mapping>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (index_id)(metric_id));

  return pb;
}

static std::shared_ptr<io::data> _dimension_ba_event_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<bam::dimension_ba_event>(d).get();
  auto pb = std::make_shared<bam::pb_dimension_ba_event>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(
      traduct, ,
      (ba_id)(ba_name)(ba_description)(sla_month_percent_crit)(sla_month_percent_warn)(sla_duration_crit)(sla_duration_warn));

  return pb;
}

static std::shared_ptr<io::data> _dimension_ba_bv_relation_event_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in =
      *std::static_pointer_cast<bam::dimension_ba_bv_relation_event>(d).get();
  auto pb = std::make_shared<bam::pb_dimension_ba_bv_relation_event>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (ba_id)(bv_id));

  return pb;
}

static std::shared_ptr<io::data> _dimension_bv_event_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in = *std::static_pointer_cast<bam::dimension_bv_event>(d).get();
  auto pb = std::make_shared<bam::pb_dimension_bv_event>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (bv_id)(bv_name)(bv_description));

  return pb;
}

static std::shared_ptr<io::data> _dimension_truncate_table_signal_to_pb(
    const std::shared_ptr<io::data>& d) {
  auto const& in =
      *std::static_pointer_cast<bam::dimension_truncate_table_signal>(d).get();
  auto pb = std::make_shared<bam::pb_dimension_truncate_table_signal>();
  pb->destination_id = d->destination_id;
  pb->source_id = d->source_id;
  auto& obj = pb->mut_obj();
  BOOST_PP_SEQ_FOR_EACH(traduct, , (update_started));

  return pb;
}

std::shared_ptr<io::data> com::centreon::broker::neb::bbdo2_to_bbdo3(
    const std::shared_ptr<io::data>& d) {
  switch (d->type()) {
    case neb::instance::static_type():
      return _instance_to_pb(d);
    case neb::host::static_type():
      return _host_to_pb(d);
    case neb::host_group::static_type():
      return _host_group_to_pb(d);
    case neb::host_group_member::static_type():
      return _host_group_member_to_pb(d);
    case neb::service::static_type():
      return _service_to_pb(d);
    case neb::service_group::static_type():
      return _service_group_to_pb(d);
    case neb::service_group_member::static_type():
      return _service_group_member_to_pb(d);
    case neb::custom_variable::static_type():
      return _custom_variable_to_pb(d);
    case storage::index_mapping::static_type():
      return _index_mapping_to_pb(d);
    case storage::metric_mapping::static_type():
      return _metric_mapping_to_pb(d);
    case bam::dimension_ba_event::static_type():
      return _dimension_ba_event_to_pb(d);
    case bam::dimension_ba_bv_relation_event::static_type():
      return _dimension_ba_bv_relation_event_to_pb(d);
    case bam::dimension_bv_event::static_type():
      return _dimension_bv_event_to_pb(d);
    case bam::dimension_truncate_table_signal::static_type():
      return _dimension_truncate_table_signal_to_pb(d);
    default:
      return d;
  }
}