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

#include <gtest/gtest.h>
#include <boost/preprocessor/seq/for_each.hpp>

#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"

#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/bam/inherited_downtime.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/host_group_member.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_group.hh"
#include "com/centreon/broker/neb/service_group_member.hh"
#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker;

#define cmp_pb(attrib) EXPECT_EQ(pb.attrib(), bbdo2->attrib);
#define comp_pb(not_used_1, not_used2, seq_head) cmp_pb(seq_head)

TEST(bbdo2_to_bbdo3, instance) {
  std::shared_ptr<neb::instance> bbdo2 = std::make_shared<neb::instance>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  bbdo2->poller_id = rand();
  bbdo2->engine = "Centreon Engine";
  bbdo2->is_running = true;
  bbdo2->name = "Test Poller";
  bbdo2->version = "21.04.0";
  bbdo2->program_start = time(nullptr);
  bbdo2->program_end = bbdo2->program_start + 3600;
  bbdo2->pid = rand();

  std::shared_ptr<io::data> bbdo_3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo_3, nullptr);
  ASSERT_EQ(bbdo_3->type(), neb::pb_instance::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_instance*>(bbdo_3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);
  EXPECT_EQ(pb.instance_id(), bbdo2->poller_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (engine)(name)(version)(pid));
  EXPECT_EQ(pb.running(), bbdo2->is_running);
  EXPECT_EQ(pb.start_time(), bbdo2->program_start.get_time_t());
  EXPECT_EQ(pb.end_time(), bbdo2->program_end.get_time_t());
}

static void fill_status(neb::status& st) {
  st.event_handler_enabled = rand() % 2;
  st.flap_detection_enabled = rand() % 2;
  st.notifications_enabled = rand() % 2;
}

static void fill_host_service(neb::host_service& s) {
  s.action_url = "zefzefefef";
  s.check_freshness = rand() % 2;
  s.default_active_checks_enabled = rand() % 2;
  s.default_event_handler_enabled = rand() % 2;
  s.default_flap_detection_enabled = rand() % 2;
  s.default_notifications_enabled = rand() % 2;
  s.default_passive_checks_enabled = rand() % 2;
  s.display_name = "Test display name";
  s.first_notification_delay = static_cast<double>(rand()) / RAND_MAX;
  s.freshness_threshold = static_cast<double>(rand()) / RAND_MAX;
  s.high_flap_threshold = static_cast<double>(rand()) / RAND_MAX;
  s.icon_image = "Test icon image";
  s.icon_image_alt = "Test icon image alt";
  s.low_flap_threshold = static_cast<double>(rand()) / RAND_MAX;
  s.notes = "Test notes";
  s.notes_url = "Test notes url";
  s.notification_interval = static_cast<double>(rand()) / RAND_MAX;
  s.notification_period = "Test notification period";
  s.notify_on_downtime = rand() % 2;
  s.notify_on_flapping = rand() % 2;
  s.notify_on_recovery = rand() % 2;
  s.retain_nonstatus_information = rand() % 2;
  s.retain_status_information = rand() % 2;
}

static void fill_host_service_status(neb::host_service_status& st) {
  st.acknowledged = rand() % 2;
  st.acknowledgement_type = rand();
  st.active_checks_enabled = rand() % 2;
  st.check_command = "ezfeizorgfe;rzf";
  st.check_interval = rand();
  st.check_period = "iorfu,ezvzivnr";
  st.check_type = rand() % 2;
  st.current_check_attempt = rand() % 10;
  st.current_state = rand() % 4;
  st.downtime_depth = rand() % 10;
  st.enabled = rand() % 2;
  st.event_handler = "zefvze";
  st.execution_time = static_cast<double>(rand()) / RAND_MAX;
  st.has_been_checked = rand() % 2;
  st.host_id = rand();
  st.is_flapping = rand() % 2;
  st.last_check = time(nullptr);
  st.last_hard_state = rand() % 4;
  st.last_hard_state_change = time(nullptr);
  st.last_notification = time(nullptr) + 1;
  st.last_state_change = time(nullptr) + 2;
  st.last_update = time(nullptr) + 3;
  st.latency = static_cast<double>(rand()) / RAND_MAX;
  st.max_check_attempts = rand() % 10;
  st.next_check = time(nullptr);
  st.next_notification = time(nullptr) + 4;
  st.no_more_notifications = rand() % 2;
  st.notification_number = rand() % 10;
  st.obsess_over = rand() % 2;
  st.output = "Test output\ngzergzg;repgt\nfgsgerg";
  st.passive_checks_enabled = rand() % 2;
  st.percent_state_change = static_cast<double>(rand()) / RAND_MAX;
  st.perf_data = "Test perfdatasdfsqfzefefer";
  st.retry_interval = static_cast<double>(rand()) / RAND_MAX;
  st.should_be_scheduled = rand() % 2;
  st.state_type = rand() % 2;

  fill_status(st);
}

static void fill_host_status(neb::host_status& st) {
  st.last_time_down = time(nullptr) + 10;
  st.last_time_unreachable = time(nullptr) + 20;
  st.last_time_up = time(nullptr) + 30;
  fill_host_service_status(st);
}

static void fill_service_status(neb::service_status& st) {
  st.host_name = "zefezrvtrpot";
  st.last_time_critical = time(nullptr) + 40;
  st.last_time_ok = time(nullptr) + 50;
  st.last_time_unknown = time(nullptr) + 60;
  st.last_time_warning = time(nullptr) + 70;
  st.service_description = "Test service description86re4gz";
  st.service_id = rand();
  fill_host_service_status(st);
}

static void fill_group(neb::group& grp) {
  grp.name = "Test alias";
  grp.enabled = rand() % 2;
  grp.id = rand();
  grp.poller_id = rand();
}

static void fill_group_member(neb::group_member& grp) {
  grp.enabled = rand() % 2;
  grp.group_id = rand();
  grp.group_name = "Test alias";
  grp.host_id = rand();
  grp.poller_id = rand();
}

TEST(bbdo2_to_bbdo3, host) {
  std::shared_ptr<neb::host> bbdo2 = std::make_shared<neb::host>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_host_status(*bbdo2);
  fill_host_service(*bbdo2);

  bbdo2->address = "uf;paze jof";
  bbdo2->alias = "Test aliassdfzefz";
  bbdo2->flap_detection_on_down = rand() % 2;
  bbdo2->flap_detection_on_unreachable = rand() % 2;
  bbdo2->flap_detection_on_up = rand() % 2;
  bbdo2->host_name = "defzefopez;f";
  bbdo2->notify_on_down = rand() % 2;
  bbdo2->notify_on_unreachable = rand() % 2;
  bbdo2->poller_id = rand();
  bbdo2->stalk_on_down = rand() % 2;
  bbdo2->stalk_on_unreachable = rand() % 2;
  bbdo2->stalk_on_up = rand() % 2;
  bbdo2->statusmap_image = "erzitozev;ez";
  bbdo2->timezone = "zeiofi;c";

  std::shared_ptr<io::data> bbdo_3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo_3, nullptr);
  ASSERT_EQ(bbdo_3->type(), neb::pb_host::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_host*>(bbdo_3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(
      comp_pb, , (host_id)(acknowledged)(enabled)(check_command)(check_interval)(check_period)(event_handler_enabled)(event_handler)(execution_time)(last_check)(last_hard_state_change)(last_notification)(notification_number)(last_state_change)(last_time_down)(last_time_unreachable)(last_time_up)(last_update)(latency)(max_check_attempts)(next_check)(no_more_notifications)(output)(percent_state_change)(retry_interval)(should_be_scheduled)(action_url)(address)(alias)(check_freshness)(default_event_handler_enabled)(display_name)(first_notification_delay)(flap_detection_on_down)(flap_detection_on_unreachable)(flap_detection_on_up)(freshness_threshold)(high_flap_threshold)(icon_image)(icon_image_alt)(low_flap_threshold)(notes)(notes_url)(notification_interval)(notification_period)(notify_on_down)(notify_on_downtime)(notify_on_flapping)(notify_on_recovery)(notify_on_unreachable)(stalk_on_down)(stalk_on_unreachable)(stalk_on_up)(statusmap_image)(retain_nonstatus_information)(retain_status_information)(timezone));
  EXPECT_EQ(static_cast<unsigned>(pb.acknowledgement_type()),
            bbdo2->acknowledgement_type);
  EXPECT_EQ(pb.active_checks(), bbdo2->active_checks_enabled);
  EXPECT_EQ(pb.scheduled_downtime_depth(), bbdo2->downtime_depth);
  EXPECT_EQ(static_cast<unsigned>(pb.check_type()), bbdo2->check_type);
  EXPECT_EQ(pb.check_attempt(), bbdo2->current_check_attempt);
  EXPECT_EQ(static_cast<unsigned>(pb.state()), bbdo2->current_state);
  EXPECT_EQ(pb.flap_detection(), bbdo2->flap_detection_enabled);
  EXPECT_EQ(pb.checked(), bbdo2->has_been_checked);
  EXPECT_EQ(pb.flapping(), bbdo2->is_flapping);
  EXPECT_EQ(static_cast<unsigned>(pb.last_hard_state()),
            bbdo2->last_hard_state);
  EXPECT_EQ(pb.next_host_notification(), bbdo2->next_notification);
  EXPECT_EQ(pb.notify(), bbdo2->notifications_enabled);
  EXPECT_EQ(pb.passive_checks(), bbdo2->passive_checks_enabled);
  EXPECT_EQ(pb.perfdata(), bbdo2->perf_data);
  EXPECT_EQ(pb.obsess_over_host(), bbdo2->obsess_over);
  EXPECT_EQ(static_cast<unsigned>(pb.state_type()), bbdo2->state_type);
  EXPECT_EQ(pb.default_active_checks(), bbdo2->default_active_checks_enabled);
  EXPECT_EQ(pb.default_flap_detection(), bbdo2->default_flap_detection_enabled);
  EXPECT_EQ(pb.default_notify(), bbdo2->default_notifications_enabled);
  EXPECT_EQ(pb.default_passive_checks(), bbdo2->default_passive_checks_enabled);
  EXPECT_EQ(pb.name(), bbdo2->host_name);
  EXPECT_EQ(pb.instance_id(), bbdo2->poller_id);
}

TEST(bbdo2_to_bbdo3, host_group) {
  std::shared_ptr<neb::host_group> bbdo2 = std::make_shared<neb::host_group>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_group(*bbdo2);

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_host_group::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_host_group*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (enabled)(name)(poller_id));
  EXPECT_EQ(pb.hostgroup_id(), bbdo2->id);
}

TEST(bbdo2_to_bbdo3, host_group_member) {
  std::shared_ptr<neb::host_group_member> bbdo2 =
      std::make_shared<neb::host_group_member>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_group_member(*bbdo2);

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_host_group_member::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_host_group_member*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (enabled)(host_id)(poller_id));
  EXPECT_EQ(pb.hostgroup_id(), bbdo2->group_id);
  EXPECT_EQ(pb.name(), bbdo2->group_name);
}

TEST(bbdo2_to_bbdo3, host_status) {
  std::shared_ptr<neb::host_status> bbdo2 =
      std::make_shared<neb::host_status>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_host_service_status(*bbdo2);
  bbdo2->last_time_down = time(nullptr) + 100;
  bbdo2->last_time_unreachable = time(nullptr) + 200;
  bbdo2->last_time_up = time(nullptr) + 300;

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_host_status::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_host_status*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);
  EXPECT_EQ(pb.output(), bbdo2->output.substr(0, bbdo2->output.find('\n')));
  EXPECT_EQ(pb.long_output(),
            bbdo2->output.substr(bbdo2->output.find('\n') + 1));

  BOOST_PP_SEQ_FOR_EACH(comp_pb, ,
                        (
                            host_id)(state_type)(last_state_change)(last_hard_state)(last_hard_state_change)(last_time_up)(last_time_down)(last_time_unreachable)(percent_state_change)(latency)(execution_time)(last_check)(next_check)(should_be_scheduled)(notification_number)(no_more_notifications)(last_notification));
  EXPECT_EQ(pb.checked(), bbdo2->has_been_checked);
  EXPECT_EQ(pb.check_type(), bbdo2->check_type);
  EXPECT_EQ(pb.state(), bbdo2->current_state);
  EXPECT_EQ(pb.perfdata(), bbdo2->perf_data);
  EXPECT_EQ(pb.flapping(), bbdo2->is_flapping);
  EXPECT_EQ(pb.check_attempt(), bbdo2->current_check_attempt);
  EXPECT_EQ(pb.next_host_notification(), bbdo2->next_notification);
}

TEST(bbdo2_to_bbdo3, service) {
  std::shared_ptr<neb::service> bbdo2 = std::make_shared<neb::service>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_host_service(*bbdo2);
  fill_service_status(*bbdo2);
  bbdo2->flap_detection_on_critical = rand() % 2;
  bbdo2->flap_detection_on_ok = rand() % 2;
  bbdo2->flap_detection_on_unknown = rand() % 2;
  bbdo2->flap_detection_on_warning = rand() % 2;
  bbdo2->is_volatile = rand() % 2;
  bbdo2->notify_on_critical = rand() % 2;
  bbdo2->notify_on_unknown = rand() % 2;
  bbdo2->notify_on_warning = rand() % 2;
  bbdo2->stalk_on_critical = rand() % 2;
  bbdo2->stalk_on_ok = rand() % 2;
  bbdo2->stalk_on_unknown = rand() % 2;
  bbdo2->stalk_on_warning = rand() % 2;

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_service::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_service*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();

  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (host_id)(service_id)(acknowledged)(enabled)(check_command)(check_interval)(check_period)(event_handler_enabled)(execution_time)(last_check)(last_hard_state_change)(last_notification)(notification_number)(last_state_change)(last_time_ok)(last_time_warning)(last_time_critical)(last_time_unknown)(last_update)(latency)(max_check_attempts)(next_check)(next_notification)(no_more_notifications)(percent_state_change)(retry_interval)(host_name)(should_be_scheduled)(action_url)(check_freshness)(default_event_handler_enabled)(display_name)(first_notification_delay)(flap_detection_on_critical)(flap_detection_on_ok)(flap_detection_on_unknown)(flap_detection_on_warning)(freshness_threshold)(high_flap_threshold)(icon_image)(icon_image_alt)(is_volatile)(low_flap_threshold)(notes)(notes_url)(notification_interval)(notification_period)(notify_on_critical)(notify_on_downtime)(notify_on_flapping)(notify_on_recovery)(notify_on_unknown)(notify_on_warning)(stalk_on_critical)(stalk_on_ok)(stalk_on_unknown)(stalk_on_warning)(retain_nonstatus_information)(retain_status_information));
  EXPECT_EQ(static_cast<unsigned>(pb.acknowledgement_type()),
            bbdo2->acknowledgement_type);
  EXPECT_EQ(pb.active_checks(), bbdo2->active_checks_enabled);
  EXPECT_EQ(pb.scheduled_downtime_depth(), bbdo2->downtime_depth);
  EXPECT_EQ(static_cast<unsigned>(pb.check_type()), bbdo2->check_type);
  EXPECT_EQ(pb.check_attempt(), bbdo2->current_check_attempt);
  EXPECT_EQ(static_cast<unsigned>(pb.state()), bbdo2->current_state);
  EXPECT_EQ(pb.flap_detection(), bbdo2->flap_detection_enabled);
  EXPECT_EQ(pb.checked(), bbdo2->has_been_checked);
  EXPECT_EQ(pb.flapping(), bbdo2->is_flapping);
  EXPECT_EQ(static_cast<unsigned>(pb.last_hard_state()),
            bbdo2->last_hard_state);
  EXPECT_EQ(pb.notify(), bbdo2->notifications_enabled);
  EXPECT_EQ(pb.output(), bbdo2->output.substr(0, bbdo2->output.find('\n')));
  EXPECT_EQ(pb.long_output(),
            bbdo2->output.substr(bbdo2->output.find('\n') + 1));
  EXPECT_EQ(pb.passive_checks(), bbdo2->passive_checks_enabled);
  EXPECT_EQ(pb.perfdata(), bbdo2->perf_data);
  EXPECT_EQ(pb.description(), bbdo2->service_description);
  EXPECT_EQ(pb.obsess_over_service(), bbdo2->obsess_over);
  EXPECT_EQ(static_cast<unsigned>(pb.state_type()), bbdo2->state_type);
  EXPECT_EQ(pb.default_active_checks(), bbdo2->default_active_checks_enabled);
  EXPECT_EQ(pb.default_flap_detection(), bbdo2->default_flap_detection_enabled);
  EXPECT_EQ(pb.default_notify(), bbdo2->default_notifications_enabled);
  EXPECT_EQ(pb.default_passive_checks(), bbdo2->default_passive_checks_enabled);
}

TEST(bbdo2_to_bbdo3, service_group) {
  std::shared_ptr<neb::service_group> bbdo2 =
      std::make_shared<neb::service_group>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_group(*bbdo2);

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_service_group::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_service_group*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (enabled)(name)(poller_id));
  EXPECT_EQ(pb.servicegroup_id(), bbdo2->id);
}

TEST(bbdo2_to_bbdo3, service_group_member) {
  std::shared_ptr<neb::service_group_member> bbdo2 =
      std::make_shared<neb::service_group_member>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_group_member(*bbdo2);

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_service_group_member::static_type());

  const auto& pb_bbdo3 =
      *static_cast<neb::pb_service_group_member*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (enabled)(host_id)(poller_id)(service_id));
  EXPECT_EQ(pb.servicegroup_id(), bbdo2->group_id);
  EXPECT_EQ(pb.name(), bbdo2->group_name);
}

TEST(bbdo2_to_bbdo3, service_status) {
  std::shared_ptr<neb::service_status> bbdo2 =
      std::make_shared<neb::service_status>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  fill_host_service_status(*bbdo2);
  fill_service_status(*bbdo2);

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_service_status::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_service_status*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(
      comp_pb,
      , (host_id)(service_id)(last_state_change)(last_hard_state)(last_hard_state_change)(last_time_ok)(last_time_warning)(last_time_critical)(last_time_unknown)(percent_state_change)(latency)(execution_time)(last_check)(next_check)(should_be_scheduled)(notification_number)(no_more_notifications)(last_notification)(next_notification)(acknowledgement_type));
  EXPECT_EQ(pb.checked(), bbdo2->has_been_checked);
  EXPECT_EQ(pb.check_type(), bbdo2->check_type);
  EXPECT_EQ(pb.state(), bbdo2->current_state);
  EXPECT_EQ(pb.state_type(), bbdo2->state_type);
  EXPECT_EQ(pb.output(), bbdo2->output.substr(0, bbdo2->output.find('\n')));
  EXPECT_EQ(pb.long_output(),
            bbdo2->output.substr(bbdo2->output.find('\n') + 1));
  EXPECT_EQ(pb.perfdata(), bbdo2->perf_data);
  EXPECT_EQ(pb.flapping(), bbdo2->is_flapping);
  EXPECT_EQ(pb.check_attempt(), bbdo2->current_check_attempt);
  EXPECT_EQ(pb.scheduled_downtime_depth(), bbdo2->downtime_depth);
}

TEST(bbdo2_to_bbdo3, custom_variable) {
  std::shared_ptr<neb::custom_variable> bbdo2 =
      std::make_shared<neb::custom_variable>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->default_value = "ergueirzgu";
  bbdo2->enabled = rand() % 2;
  bbdo2->var_type = rand();
  bbdo2->host_id = rand();
  bbdo2->modified = rand() % 2;
  bbdo2->name = "zuerife";
  bbdo2->service_id = rand();
  bbdo2->update_time = time(nullptr);
  bbdo2->value = "zefzefer";

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), neb::pb_custom_variable::static_type());

  const auto& pb_bbdo3 = *static_cast<neb::pb_custom_variable*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(
      comp_pb, ,
      (enabled)(default_value)(enabled)(host_id)(modified)(name)(service_id)(update_time)(value));
  EXPECT_EQ(pb.type(), bbdo2->var_type);
}

TEST(bbdo2_to_bbdo3, index_mapping) {
  auto bbdo2 = std::make_shared<storage::index_mapping>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->index_id = rand();
  bbdo2->host_id = rand();
  bbdo2->service_id = rand();

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), storage::pb_index_mapping::static_type());

  const auto& pb_bbdo3 = *static_cast<storage::pb_index_mapping*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (index_id)(host_id)(service_id));
}

TEST(bbdo2_to_bbdo3, metric_mapping) {
  auto bbdo2 = std::make_shared<storage::metric_mapping>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->index_id = rand();
  bbdo2->metric_id = rand();

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), storage::pb_metric_mapping::static_type());

  const auto& pb_bbdo3 = *static_cast<storage::pb_metric_mapping*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (index_id)(metric_id));
}

TEST(bbdo2_to_bbdo3, dimension_ba_event) {
  auto bbdo2 = std::make_shared<bam::dimension_ba_event>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->ba_id = rand();
  bbdo2->ba_name = "Test ba name";
  bbdo2->ba_description = "Test ba description";
  bbdo2->sla_month_percent_crit = static_cast<double>(rand()) / RAND_MAX;
  bbdo2->sla_month_percent_warn = static_cast<double>(rand()) / RAND_MAX;
  bbdo2->sla_duration_crit = static_cast<double>(rand()) / RAND_MAX;
  bbdo2->sla_duration_warn = static_cast<double>(rand()) / RAND_MAX;

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), bam::pb_dimension_ba_event::static_type());

  const auto& pb_bbdo3 = *static_cast<bam::pb_dimension_ba_event*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(
      comp_pb, ,
      (ba_id)(ba_name)(ba_description)(sla_month_percent_crit)(sla_month_percent_warn)(sla_duration_crit)(sla_duration_warn));
}

TEST(bbdo2_to_bbdo3, dimension_ba_bv_relation_event) {
  auto bbdo2 = std::make_shared<bam::dimension_ba_bv_relation_event>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->ba_id = rand();
  bbdo2->bv_id = rand();

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(),
            bam::pb_dimension_ba_bv_relation_event::static_type());

  const auto& pb_bbdo3 =
      *static_cast<bam::pb_dimension_ba_bv_relation_event*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (ba_id)(bv_id));
}

TEST(bbdo2_to_bbdo3, dimension_bv_event) {
  auto bbdo2 = std::make_shared<bam::dimension_bv_event>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->bv_id = rand();
  bbdo2->bv_name = "Test ba name";
  bbdo2->bv_description = "Test ba description";

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), bam::pb_dimension_bv_event::static_type());

  const auto& pb_bbdo3 = *static_cast<bam::pb_dimension_bv_event*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (bv_id)(bv_name)(bv_description));
}

TEST(bbdo2_to_bbdo3, dimension_truncate_table_signal) {
  auto bbdo2 =
      std::make_shared<bam::dimension_truncate_table_signal>(rand() % 2);
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(),
            bam::pb_dimension_truncate_table_signal::static_type());

  const auto& pb_bbdo3 =
      *static_cast<bam::pb_dimension_truncate_table_signal*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (update_started));
}

TEST(bbdo2_to_bbdo3, inherited_downtime) {
  auto bbdo2 = std::make_shared<bam::inherited_downtime>();
  bbdo2->destination_id = rand();
  bbdo2->source_id = rand();
  bbdo2->ba_id = rand();
  bbdo2->in_downtime = rand() % 2;

  std::shared_ptr<io::data> bbdo3 = neb::bbdo2_to_bbdo3(bbdo2);

  ASSERT_NE(bbdo3, nullptr);
  ASSERT_EQ(bbdo3->type(), bam::pb_inherited_downtime::static_type());

  const auto& pb_bbdo3 = *static_cast<bam::pb_inherited_downtime*>(bbdo3.get());
  const auto& pb = pb_bbdo3.obj();
  EXPECT_EQ(pb_bbdo3.destination_id, bbdo2->destination_id);
  EXPECT_EQ(pb_bbdo3.source_id, bbdo2->source_id);

  BOOST_PP_SEQ_FOR_EACH(comp_pb, , (ba_id)(in_downtime));
}
