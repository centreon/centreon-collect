/**
 * Copyright 2026 Centreon
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
#include <optional>

#include "protobuf_test_classes.hh"

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/cache/global_cache.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::broker;
using namespace com::centreon::broker::cache;

class simple_global_cache : public global_cache {
 public:
  std::unique_ptr<allocators> _allocators;

  managed_mapped_file& file() { return *_file; }

  void managed_map(bool create) override {
    global_cache::managed_map(create);
    _allocators = std::make_unique<allocators>(_file->get_segment_manager());
  }

  simple_global_cache(const std::string& file_path)
      : global_cache(g_io_context,
                     file_path,
                     spdlog::default_logger(),
                     e_cache_type::real_time,
                     nullptr) {}

  static std::shared_ptr<simple_global_cache> load(
      const std::string& file_path) {
    std::shared_ptr<simple_global_cache> ret =
        std::make_shared<simple_global_cache>(file_path);
    ret->_open(5000);
    return ret;
  }

  std::optional<host_serv_pair> get_host_serv_id(uint64_t) override {
    return {};
  }

  void _write_impl(const std::shared_ptr<io::data>&, bool) override {}

  const host* get_host(uint64_t, lock&) override { return nullptr; }
  virtual const service* get_service(uint64_t, uint64_t, lock&) override {
    return nullptr;
  }

  virtual const instance* get_instance(uint64_t, lock&) override {
    return nullptr;
  }

  const host_group* get_host_group(uint64_t, lock&) const override {
    return nullptr;
  }
  const service_group* get_service_group(uint64_t, lock&) const override {
    return nullptr;
  }

  virtual void append_service_group(uint64_t,
                                    uint64_t,
                                    std::ostream&) const override {}
  virtual void append_host_group(uint64_t, std::ostream&) const override {}
  virtual void append_host_tag_id(uint64_t,
                                  TagType,
                                  std::ostream&) const override {}
  virtual void append_serv_tag_id(uint64_t,
                                  uint64_t,
                                  TagType,
                                  std::ostream&) const override {}
  virtual void append_host_tag_name(uint64_t,
                                    TagType,
                                    std::ostream&) const override {}
  virtual void append_serv_tag_name(uint64_t,
                                    uint64_t,
                                    TagType,
                                    std::ostream&) const override {}

  virtual uint64_t get_index_id_from_metric_id(uint64_t) const override {
    return 0;
  }

  virtual std::optional<int32_t> get_severity(const uint64_t,
                                              const uint64_t) const override {
    return 0;
  }

  const dimension_ba_event* get_dimension_ba_event(uint64_t,
                                                   lock&) const override {
    return nullptr;
  }
  const dimension_bv_event* get_dimension_bv_event(uint64_t,
                                                   lock&) const override {
    return nullptr;
  }
  void enumerate_bvs(uint64_t, bv_enumerator&&) const override {}

  void enumerate_host_group(uint64_t, group_enumerator&&) const override {}
  void enumerate_service_group(uint64_t,
                               uint64_t,
                               group_enumerator&&) const override {}
};

static std::string file_path = "/tmp/protobuf_test.bin";

class protobuf_test : public testing::Test {
 public:
  static void SetUpTestSuite() {
    spdlog::default_logger()->set_level(spdlog::level::trace);
  }
  void SetUp() override { ::remove((file_path + ".rt").c_str()); }
};

static std::string random_string() {
  std::string ret;
  unsigned length = rand() % 50;
  for (; length; --length) {
    ret.push_back(rand() % 64 + ' ');
  }
  return ret;
}

AgentInfo create_random_agent_info() {
  AgentInfo pb;
  pb.set_major(rand());
  pb.set_minor(rand());
  pb.set_patch(rand());
  pb.set_reverse(rand() % 2);
  pb.set_os(random_string());
  pb.set_os_version(random_string());
  pb.set_nb_agent(rand());
  return pb;
}

#define EXPECT_EQ_MARK(val1, val2) \
  if (val1 != val2) {              \
    g_test_failed = true;          \
  }                                \
  EXPECT_EQ(val1, val2);

static std::string to_string(const com::centreon::broker::cache::string& src) {
  return std::string(src.c_str(), src.length());
}

bool compare_agent_info(const AgentInfo& left, const agent_info& right) {
  bool g_test_failed = false;
  EXPECT_EQ_MARK(left.major(), right.major());
  EXPECT_EQ_MARK(left.minor(), right.minor());
  EXPECT_EQ_MARK(left.patch(), right.patch());
  EXPECT_EQ_MARK(left.reverse(), right.reverse());
  EXPECT_EQ_MARK(left.os(), to_string(right.os()));
  EXPECT_EQ_MARK(left.os_version(), to_string(right.os_version()));
  EXPECT_EQ_MARK(left.nb_agent(), right.nb_agent());

  return !g_test_failed;
}

TEST_F(protobuf_test, agent_info) {
  srand(time(nullptr));
  AgentInfo pb = create_random_agent_info();

  auto file_map = simple_global_cache::load(file_path);

  agent_info* converted = file_map->file().construct<agent_info>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_TRUE(compare_agent_info(pb, *converted));

  AgentInfo pb2 = create_random_agent_info();
  converted->update(pb2, *file_map->_allocators);
  ASSERT_TRUE(compare_agent_info(pb2, *converted));
}

TEST_F(protobuf_test, agent_stats) {
  srand(time(nullptr));
  AgentStats pb;
  pb.set_poller_id(rand());
  unsigned nb_agent = rand() % 10;
  for (; nb_agent; --nb_agent) {
    *pb.add_stats() = create_random_agent_info();
  }

  auto file_map = simple_global_cache::load(file_path);

  agent_stats* converted = file_map->file().construct<agent_stats>("my_object")(
      pb, *file_map->_allocators);

  ASSERT_EQ(pb.poller_id(), converted->poller_id());
  ASSERT_EQ(pb.stats().size(), converted->stats().size());
  auto pb_iter = pb.stats().begin();
  auto conv_iter = converted->stats().begin();
  for (; pb_iter != pb.stats().end(); ++pb_iter, ++conv_iter) {
    ASSERT_TRUE(compare_agent_info(
        *pb_iter, static_cast<const agent_info&>(**conv_iter)));
  }

  AgentStats pb2;
  pb2.set_poller_id(rand());
  nb_agent = rand() % 10;
  for (; nb_agent; --nb_agent) {
    *pb2.add_stats() = create_random_agent_info();
  }
  converted->update(pb2, *file_map->_allocators);

  ASSERT_EQ(pb2.poller_id(), converted->poller_id());
  ASSERT_EQ(pb2.stats().size(), converted->stats().size());
  pb_iter = pb2.stats().begin();
  conv_iter = converted->stats().begin();
  for (; pb_iter != pb2.stats().end(); ++pb_iter, ++conv_iter) {
    ASSERT_TRUE(compare_agent_info(
        *pb_iter, static_cast<const agent_info&>(**conv_iter)));
  }
}

TEST_F(protobuf_test, comment) {
  srand(time(nullptr));
  Comment pb;
  pb.mutable_header()->set_conf_version(rand());
  pb.set_author(random_string());
  pb.set_type(Comment_Type(rand() % 3));
  pb.set_data(random_string());
  pb.set_deletion_time(rand());
  pb.set_entry_time(rand());
  pb.set_entry_type(Comment_EntryType(rand() % 5));
  pb.set_expire_time(rand());
  pb.set_expires(rand() % 2);
  pb.set_host_id(rand());
  pb.set_internal_id(rand());
  pb.set_persistent(rand() % 2);
  pb.set_instance_id(rand());
  pb.set_service_id(rand());
  pb.set_source(Comment_Src(rand() % 2));

  auto file_map = simple_global_cache::load(file_path);

  comment* converted = file_map->file().construct<comment>("my_object")(
      pb, *file_map->_allocators);

  ASSERT_EQ(pb.header().conf_version(),
            static_cast<const bbdo_header*>(converted->header().get())
                ->conf_version());

  ASSERT_EQ(pb.author(), to_string(converted->author()));
  ASSERT_EQ(pb.type(), converted->type());
  ASSERT_EQ(pb.data(), to_string(converted->data()));
  ASSERT_EQ(pb.deletion_time(), converted->deletion_time());
  ASSERT_EQ(pb.entry_time(), converted->entry_time());
  ASSERT_EQ(pb.entry_type(), converted->entry_type());
  ASSERT_EQ(pb.expire_time(), converted->expire_time());
  ASSERT_EQ(pb.expires(), converted->expires());
  ASSERT_EQ(pb.host_id(), converted->host_id());
  ASSERT_EQ(pb.internal_id(), converted->internal_id());
  ASSERT_EQ(pb.persistent(), converted->persistent());
  ASSERT_EQ(pb.instance_id(), converted->instance_id());
  ASSERT_EQ(pb.service_id(), converted->service_id());
  ASSERT_EQ(pb.source(), converted->source());

  Comment pb2;
  pb2.mutable_header()->set_conf_version(rand());
  pb2.set_author(random_string());
  pb2.set_type(Comment_Type(rand() % 3));
  pb2.set_data(random_string());
  pb2.set_deletion_time(rand());
  pb2.set_entry_time(rand());
  pb2.set_entry_type(Comment_EntryType(rand() % 5));
  pb2.set_expire_time(rand());
  pb2.set_expires(rand() % 2);
  pb2.set_host_id(rand());
  pb2.set_internal_id(rand());
  pb2.set_persistent(rand() % 2);
  pb2.set_instance_id(rand());
  pb2.set_service_id(rand());
  pb2.set_source(Comment_Src(rand() % 2));

  converted->update(pb2, *file_map->_allocators);

  ASSERT_EQ(pb2.header().conf_version(),
            static_cast<const bbdo_header*>(converted->header().get())
                ->conf_version());

  ASSERT_EQ(pb2.author(), to_string(converted->author()));
  ASSERT_EQ(pb2.type(), converted->type());
  ASSERT_EQ(pb2.data(), to_string(converted->data()));
  ASSERT_EQ(pb2.deletion_time(), converted->deletion_time());
  ASSERT_EQ(pb2.entry_time(), converted->entry_time());
  ASSERT_EQ(pb2.entry_type(), converted->entry_type());
  ASSERT_EQ(pb2.expire_time(), converted->expire_time());
  ASSERT_EQ(pb2.expires(), converted->expires());
  ASSERT_EQ(pb2.host_id(), converted->host_id());
  ASSERT_EQ(pb2.internal_id(), converted->internal_id());
  ASSERT_EQ(pb2.persistent(), converted->persistent());
  ASSERT_EQ(pb2.instance_id(), converted->instance_id());
  ASSERT_EQ(pb2.service_id(), converted->service_id());
  ASSERT_EQ(pb2.source(), converted->source());
}

#define SET_OPTIONAL_UINT(pb_name, field, max_value) \
  if (rand() % 2) {                                  \
    pb_name.set_##field(rand() % max_value);         \
  }

#define SET_OPTIONAL_STRING(pb_name, field) \
  if (rand() % 2) {                         \
    pb_name.set_##field(random_string());   \
  }

#define COMPARE_OPTIONAL(pb_name, field, type, enum_index)          \
  ASSERT_EQ(pb_name.has_##field(), converted->field().has_value()); \
  if (pb_name.has_##field()) {                                      \
    ASSERT_EQ(pb_name.field(), converted->field().value());         \
  }

#define COMPARE_OPTIONAL_STRING(pb_name, field, enum_index)                \
  ASSERT_EQ(pb_name.has_##field(), static_cast<bool>(converted->field())); \
  if (pb_name.has_##field()) {                                             \
    ASSERT_EQ(pb_name.field(), to_string(*converted->field()));            \
  }

TEST_F(protobuf_test, adaptive_service) {
  srand(time(nullptr));
  AdaptiveService pb;
  pb.set_host_id(rand());
  pb.set_service_id(rand());

  SET_OPTIONAL_UINT(pb, notify, 2)
  SET_OPTIONAL_UINT(pb, active_checks, 2)
  SET_OPTIONAL_UINT(pb, should_be_scheduled, 2)
  SET_OPTIONAL_UINT(pb, passive_checks, 2)
  SET_OPTIONAL_STRING(pb, event_handler);
  SET_OPTIONAL_UINT(pb, check_interval, std::numeric_limits<uint32_t>::max())
  SET_OPTIONAL_UINT(pb, retry_interval, std::numeric_limits<uint32_t>::max())
  SET_OPTIONAL_UINT(pb, max_check_attempts,
                    std::numeric_limits<uint32_t>::max())
  SET_OPTIONAL_STRING(pb, check_command);
  SET_OPTIONAL_STRING(pb, check_period);

  auto file_map = simple_global_cache::load(file_path);

  adaptive_service* converted = file_map->file().construct<adaptive_service>(
      "my_object")(pb, *file_map->_allocators);

  ASSERT_EQ(pb.host_id(), converted->host_id());
  ASSERT_EQ(pb.service_id(), converted->service_id());
  COMPARE_OPTIONAL(pb, notify, bool, 2);
  COMPARE_OPTIONAL(pb, active_checks, bool, 3);
  COMPARE_OPTIONAL(pb, should_be_scheduled, bool, 4);
  COMPARE_OPTIONAL(pb, passive_checks, bool, 5);
  COMPARE_OPTIONAL_STRING(pb, event_handler, 9)
  COMPARE_OPTIONAL(pb, check_interval, uint32_t, 11);
  COMPARE_OPTIONAL(pb, retry_interval, uint32_t, 12);
  COMPARE_OPTIONAL(pb, max_check_attempts, uint32_t, 13);
  COMPARE_OPTIONAL_STRING(pb, check_command, 10);
  COMPARE_OPTIONAL_STRING(pb, check_period, 15);

  AdaptiveService pb2;
  pb2.set_host_id(rand());
  pb2.set_service_id(rand());

  SET_OPTIONAL_UINT(pb2, notify, 2)
  SET_OPTIONAL_UINT(pb2, active_checks, 2)
  SET_OPTIONAL_UINT(pb2, should_be_scheduled, 2)
  SET_OPTIONAL_UINT(pb2, passive_checks, 2)
  SET_OPTIONAL_STRING(pb2, event_handler);
  SET_OPTIONAL_UINT(pb2, check_interval, std::numeric_limits<uint32_t>::max())
  SET_OPTIONAL_UINT(pb2, retry_interval, std::numeric_limits<uint32_t>::max())
  SET_OPTIONAL_UINT(pb2, max_check_attempts,
                    std::numeric_limits<uint32_t>::max())
  SET_OPTIONAL_STRING(pb2, check_command);
  SET_OPTIONAL_STRING(pb2, check_period);

  converted->update(pb2, *file_map->_allocators);

  ASSERT_EQ(pb2.host_id(), converted->host_id());
  ASSERT_EQ(pb2.service_id(), converted->service_id());
  COMPARE_OPTIONAL(pb2, notify, bool, 2);
  COMPARE_OPTIONAL(pb2, active_checks, bool, 3);
  COMPARE_OPTIONAL(pb2, should_be_scheduled, bool, 4);
  COMPARE_OPTIONAL(pb2, passive_checks, bool, 5);
  COMPARE_OPTIONAL_STRING(pb2, event_handler, 9)
  COMPARE_OPTIONAL(pb2, check_interval, uint32_t, 11);
  COMPARE_OPTIONAL(pb2, retry_interval, uint32_t, 12);
  COMPARE_OPTIONAL(pb2, max_check_attempts, uint32_t, 13);
  COMPARE_OPTIONAL_STRING(pb2, check_command, 10);
  COMPARE_OPTIONAL_STRING(pb2, check_period, 15);
}

// ---------------------------------------------------------------------------
// Helpers for to_protobuf round-trip tests
// ---------------------------------------------------------------------------

static BBDOHeader create_random_bbdo_header() {
  BBDOHeader h;
  h.set_conf_version(rand());
  return h;
}

static TagInfo create_random_tag_info() {
  TagInfo ti;
  ti.set_id(rand() + 1);
  ti.set_type(TagType(rand() % 4));
  return ti;
}

// Compare the result of to_protobuf() with the original protobuf object using
// serialization
#define ASSERT_PROTO_EQ(pb, converted_ptr) \
  ASSERT_EQ((pb).SerializeAsString(),      \
            (converted_ptr)->to_protobuf().SerializeAsString())

// ---------------------------------------------------------------------------
// to_protobuf round-trip tests
// ---------------------------------------------------------------------------

TEST_F(protobuf_test, service_to_protobuf) {
  srand(time(nullptr));
  Service pb;
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_acknowledged(rand() % 2);
  pb.set_acknowledgement_type(AckType(rand() % 3));
  pb.set_active_checks(rand() % 2);
  pb.set_enabled(rand() % 2);
  pb.set_scheduled_downtime_depth(rand() % 100 + 1);
  pb.set_check_command(random_string());
  pb.set_check_interval(rand() % 100 + 1);
  pb.set_check_period(random_string());
  pb.set_check_type(Service_CheckType(rand() % 2));
  pb.set_check_attempt(rand() % 10 + 1);
  pb.set_state(Service_State(rand() % 5));
  pb.set_event_handler_enabled(rand() % 2);
  pb.set_event_handler(random_string());
  pb.set_execution_time((double)(rand() % 100 + 1));
  pb.set_flap_detection(rand() % 2);
  pb.set_checked(rand() % 2);
  pb.set_flapping(rand() % 2);
  pb.set_last_check(rand() + 1);
  pb.set_last_hard_state(Service_State(rand() % 5));
  pb.set_last_hard_state_change(rand() + 1);
  pb.set_last_notification(rand() + 1);
  pb.set_notification_number(rand() % 100 + 1);
  pb.set_last_state_change(rand() + 1);
  pb.set_last_time_ok(rand() + 1);
  pb.set_last_time_warning(rand() + 1);
  pb.set_last_time_critical(rand() + 1);
  pb.set_last_time_unknown(rand() + 1);
  pb.set_last_update(rand() + 1);
  pb.set_latency((double)(rand() % 100 + 1));
  pb.set_max_check_attempts(rand() % 10 + 1);
  pb.set_next_check(rand() + 1);
  pb.set_next_notification(rand() + 1);
  pb.set_no_more_notifications(rand() % 2);
  pb.set_notify(rand() % 2);
  pb.set_output(random_string());
  pb.set_long_output(random_string());
  pb.set_passive_checks(rand() % 2);
  pb.set_percent_state_change((double)(rand() % 100 + 1));
  pb.set_perfdata(random_string());
  pb.set_retry_interval((double)(rand() % 100 + 1));
  pb.set_host_name(random_string());
  pb.set_description(random_string());
  pb.set_should_be_scheduled(rand() % 2);
  pb.set_obsess_over_service(rand() % 2);
  pb.set_state_type(Service_StateType(rand() % 2));
  pb.set_action_url(random_string());
  pb.set_check_freshness(rand() % 2);
  pb.set_default_active_checks(rand() % 2);
  pb.set_default_event_handler_enabled(rand() % 2);
  pb.set_default_flap_detection(rand() % 2);
  pb.set_default_notify(rand() % 2);
  pb.set_default_passive_checks(rand() % 2);
  pb.set_display_name(random_string());
  pb.set_first_notification_delay((double)(rand() % 100 + 1));
  pb.set_flap_detection_on_critical(rand() % 2);
  pb.set_flap_detection_on_ok(rand() % 2);
  pb.set_flap_detection_on_unknown(rand() % 2);
  pb.set_flap_detection_on_warning(rand() % 2);
  pb.set_freshness_threshold((double)(rand() % 100 + 1));
  pb.set_high_flap_threshold((double)(rand() % 100 + 1));
  pb.set_icon_image(random_string());
  pb.set_icon_image_alt(random_string());
  pb.set_is_volatile(rand() % 2);
  pb.set_low_flap_threshold((double)(rand() % 100 + 1));
  pb.set_notes(random_string());
  pb.set_notes_url(random_string());
  pb.set_notification_interval((double)(rand() % 100 + 1));
  pb.set_notification_period(random_string());
  pb.set_notify_on_critical(rand() % 2);
  pb.set_notify_on_downtime(rand() % 2);
  pb.set_notify_on_flapping(rand() % 2);
  pb.set_notify_on_recovery(rand() % 2);
  pb.set_notify_on_unknown(rand() % 2);
  pb.set_notify_on_warning(rand() % 2);
  pb.set_stalk_on_critical(rand() % 2);
  pb.set_stalk_on_ok(rand() % 2);
  pb.set_stalk_on_unknown(rand() % 2);
  pb.set_stalk_on_warning(rand() % 2);
  pb.set_retain_nonstatus_information(rand() % 2);
  pb.set_retain_status_information(rand() % 2);
  pb.set_severity_id(rand() + 1);
  pb.set_type(ServiceType(rand() % 5));
  pb.set_internal_id(rand() + 1);
  pb.set_icon_id(rand() + 1);
  for (int i = 1 + rand() % 2; i > 0; --i)
    *pb.add_tags() = create_random_tag_info();

  auto file_map = simple_global_cache::load(file_path);
  service* converted = file_map->file().construct<service>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, adaptive_service_status_to_protobuf) {
  srand(time(nullptr));
  AdaptiveServiceStatus pb;
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_type(ServiceType(rand() % 5));
  pb.set_internal_id(rand() + 1);
  if (rand() % 2)
    pb.set_scheduled_downtime_depth(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_acknowledgement_type(AckType(rand() % 3));
  if (rand() % 2)
    pb.set_notification_number(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_next_check(rand() + 1);
  if (rand() % 2)
    pb.set_should_be_scheduled(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  adaptive_service_status* converted =
      file_map->file().construct<adaptive_service_status>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, service_status_to_protobuf) {
  srand(time(nullptr));
  ServiceStatus pb;
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_checked(rand() % 2);
  pb.set_check_type(ServiceStatus_CheckType(rand() % 2));
  pb.set_state(ServiceStatus_State(rand() % 5));
  pb.set_state_type(ServiceStatus_StateType(rand() % 2));
  pb.set_last_state_change(rand() + 1);
  pb.set_last_hard_state(ServiceStatus_State(rand() % 5));
  pb.set_last_hard_state_change(rand() + 1);
  pb.set_last_time_ok(rand() + 1);
  pb.set_last_time_warning(rand() + 1);
  pb.set_last_time_critical(rand() + 1);
  pb.set_last_time_unknown(rand() + 1);
  pb.set_output(random_string());
  pb.set_long_output(random_string());
  pb.set_perfdata(random_string());
  pb.set_flapping(rand() % 2);
  pb.set_percent_state_change((double)(rand() % 100 + 1));
  pb.set_latency((double)(rand() % 100 + 1));
  pb.set_execution_time((double)(rand() % 100 + 1));
  pb.set_last_check(rand() + 1);
  pb.set_next_check(rand() + 1);
  pb.set_should_be_scheduled(rand() % 2);
  pb.set_check_attempt(rand() % 10 + 1);
  pb.set_notification_number(rand() % 100 + 1);
  pb.set_no_more_notifications(rand() % 2);
  pb.set_last_notification(rand() + 1);
  pb.set_next_notification(rand() + 1);
  pb.set_acknowledgement_type(AckType(rand() % 3));
  pb.set_scheduled_downtime_depth(rand() % 100 + 1);
  pb.set_type(ServiceType(rand() % 5));
  pb.set_internal_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  service_status* converted = file_map->file().construct<service_status>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, adaptive_service_to_protobuf) {
  srand(time(nullptr));
  AdaptiveService pb;
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  if (rand() % 2)
    pb.set_notify(rand() % 2);
  if (rand() % 2)
    pb.set_active_checks(rand() % 2);
  if (rand() % 2)
    pb.set_should_be_scheduled(rand() % 2);
  if (rand() % 2)
    pb.set_passive_checks(rand() % 2);
  if (rand() % 2)
    pb.set_event_handler_enabled(rand() % 2);
  if (rand() % 2)
    pb.set_flap_detection_enabled(rand() % 2);
  if (rand() % 2)
    pb.set_obsess_over_service(rand() % 2);
  if (rand() % 2)
    pb.set_event_handler(random_string());
  if (rand() % 2)
    pb.set_check_command(random_string());
  if (rand() % 2)
    pb.set_check_interval(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_retry_interval(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_max_check_attempts(rand() % 10 + 1);
  if (rand() % 2)
    pb.set_check_freshness(rand() % 2);
  if (rand() % 2)
    pb.set_check_period(random_string());
  if (rand() % 2)
    pb.set_notification_period(random_string());

  auto file_map = simple_global_cache::load(file_path);
  adaptive_service* converted = file_map->file().construct<adaptive_service>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, host_to_protobuf) {
  srand(time(nullptr));
  Host pb;
  pb.set_host_id(rand() + 1);
  pb.set_acknowledged(rand() % 2);
  pb.set_acknowledgement_type(AckType(rand() % 3));
  pb.set_active_checks(rand() % 2);
  pb.set_enabled(rand() % 2);
  pb.set_scheduled_downtime_depth(rand() % 100 + 1);
  pb.set_check_command(random_string());
  pb.set_check_interval(rand() % 100 + 1);
  pb.set_check_period(random_string());
  pb.set_check_type(Host_CheckType(rand() % 2));
  pb.set_check_attempt(rand() % 10 + 1);
  pb.set_state(Host_State(rand() % 3));
  pb.set_event_handler_enabled(rand() % 2);
  pb.set_event_handler(random_string());
  pb.set_execution_time((double)(rand() % 100 + 1));
  pb.set_flap_detection(rand() % 2);
  pb.set_checked(rand() % 2);
  pb.set_flapping(rand() % 2);
  pb.set_last_check(rand() + 1);
  pb.set_last_hard_state(Host_State(rand() % 3));
  pb.set_last_hard_state_change(rand() + 1);
  pb.set_last_notification(rand() + 1);
  pb.set_notification_number(rand() % 100 + 1);
  pb.set_last_state_change(rand() + 1);
  pb.set_last_time_down(rand() + 1);
  pb.set_last_time_unreachable(rand() + 1);
  pb.set_last_time_up(rand() + 1);
  pb.set_last_update(rand() + 1);
  pb.set_latency((double)(rand() % 100 + 1));
  pb.set_max_check_attempts(rand() % 10 + 1);
  pb.set_next_check(rand() + 1);
  pb.set_next_host_notification(rand() + 1);
  pb.set_no_more_notifications(rand() % 2);
  pb.set_notify(rand() % 2);
  pb.set_output(random_string());
  pb.set_passive_checks(rand() % 2);
  pb.set_percent_state_change((double)(rand() % 100 + 1));
  pb.set_perfdata(random_string());
  pb.set_retry_interval((double)(rand() % 100 + 1));
  pb.set_should_be_scheduled(rand() % 2);
  pb.set_obsess_over_host(rand() % 2);
  pb.set_state_type(Host_StateType(rand() % 2));
  pb.set_action_url(random_string());
  pb.set_address(random_string());
  pb.set_alias(random_string());
  pb.set_check_freshness(rand() % 2);
  pb.set_default_active_checks(rand() % 2);
  pb.set_default_event_handler_enabled(rand() % 2);
  pb.set_default_flap_detection(rand() % 2);
  pb.set_default_notify(rand() % 2);
  pb.set_default_passive_checks(rand() % 2);
  pb.set_display_name(random_string());
  pb.set_first_notification_delay((double)(rand() % 100 + 1));
  pb.set_flap_detection_on_down(rand() % 2);
  pb.set_flap_detection_on_unreachable(rand() % 2);
  pb.set_flap_detection_on_up(rand() % 2);
  pb.set_freshness_threshold((double)(rand() % 100 + 1));
  pb.set_high_flap_threshold((double)(rand() % 100 + 1));
  pb.set_name(random_string());
  pb.set_icon_image(random_string());
  pb.set_icon_image_alt(random_string());
  pb.set_instance_id(rand() % 1000 + 1);
  pb.set_low_flap_threshold((double)(rand() % 100 + 1));
  pb.set_notes(random_string());
  pb.set_notes_url(random_string());
  pb.set_notification_interval((double)(rand() % 100 + 1));
  pb.set_notification_period(random_string());
  pb.set_notify_on_down(rand() % 2);
  pb.set_notify_on_downtime(rand() % 2);
  pb.set_notify_on_flapping(rand() % 2);
  pb.set_notify_on_recovery(rand() % 2);
  pb.set_notify_on_unreachable(rand() % 2);
  pb.set_stalk_on_down(rand() % 2);
  pb.set_stalk_on_unreachable(rand() % 2);
  pb.set_stalk_on_up(rand() % 2);
  pb.set_statusmap_image(random_string());
  pb.set_retain_nonstatus_information(rand() % 2);
  pb.set_retain_status_information(rand() % 2);
  pb.set_timezone(random_string());
  pb.set_severity_id(rand() + 1);
  pb.set_icon_id(rand() + 1);
  for (int i = 1 + rand() % 2; i > 0; --i)
    *pb.add_tags() = create_random_tag_info();

  auto file_map = simple_global_cache::load(file_path);
  host* converted =
      file_map->file().construct<host>("my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, host_status_to_protobuf) {
  srand(time(nullptr));
  HostStatus pb;
  pb.set_host_id(rand() + 1);
  pb.set_checked(rand() % 2);
  pb.set_check_type(HostStatus_CheckType(rand() % 2));
  pb.set_state(HostStatus_State(rand() % 3));
  pb.set_state_type(HostStatus_StateType(rand() % 2));
  pb.set_last_state_change(rand() + 1);
  pb.set_last_hard_state(HostStatus_State(rand() % 3));
  pb.set_last_hard_state_change(rand() + 1);
  pb.set_last_time_up(rand() + 1);
  pb.set_last_time_down(rand() + 1);
  pb.set_last_time_unreachable(rand() + 1);
  pb.set_output(random_string());
  pb.set_long_output(random_string());
  pb.set_perfdata(random_string());
  pb.set_flapping(rand() % 2);
  pb.set_percent_state_change((double)(rand() % 100 + 1));
  pb.set_latency((double)(rand() % 100 + 1));
  pb.set_execution_time((double)(rand() % 100 + 1));
  pb.set_last_check(rand() + 1);
  pb.set_next_check(rand() + 1);
  pb.set_should_be_scheduled(rand() % 2);
  pb.set_check_attempt(rand() % 10 + 1);
  pb.set_notification_number(rand() % 100 + 1);
  pb.set_no_more_notifications(rand() % 2);
  pb.set_last_notification(rand() + 1);
  pb.set_next_host_notification(rand() + 1);
  pb.set_acknowledgement_type(AckType(rand() % 3));
  pb.set_scheduled_downtime_depth(rand() % 100 + 1);

  auto file_map = simple_global_cache::load(file_path);
  host_status* converted = file_map->file().construct<host_status>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, adaptive_host_status_to_protobuf) {
  srand(time(nullptr));
  AdaptiveHostStatus pb;
  pb.set_host_id(rand() + 1);
  if (rand() % 2)
    pb.set_scheduled_downtime_depth(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_acknowledgement_type(AckType(rand() % 3));
  if (rand() % 2)
    pb.set_notification_number(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_next_check(rand() + 1);
  if (rand() % 2)
    pb.set_should_be_scheduled(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  adaptive_host_status* converted =
      file_map->file().construct<adaptive_host_status>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, adaptive_host_to_protobuf) {
  srand(time(nullptr));
  AdaptiveHost pb;
  pb.set_host_id(rand() + 1);
  if (rand() % 2)
    pb.set_notify(rand() % 2);
  if (rand() % 2)
    pb.set_active_checks(rand() % 2);
  if (rand() % 2)
    pb.set_should_be_scheduled(rand() % 2);
  if (rand() % 2)
    pb.set_passive_checks(rand() % 2);
  if (rand() % 2)
    pb.set_event_handler_enabled(rand() % 2);
  if (rand() % 2)
    pb.set_flap_detection(rand() % 2);
  if (rand() % 2)
    pb.set_obsess_over_host(rand() % 2);
  if (rand() % 2)
    pb.set_event_handler(random_string());
  if (rand() % 2)
    pb.set_check_command(random_string());
  if (rand() % 2)
    pb.set_check_interval(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_retry_interval(rand() % 100 + 1);
  if (rand() % 2)
    pb.set_max_check_attempts(rand() % 10 + 1);
  if (rand() % 2)
    pb.set_check_freshness(rand() % 2);
  if (rand() % 2)
    pb.set_check_period(random_string());
  if (rand() % 2)
    pb.set_notification_period(random_string());

  auto file_map = simple_global_cache::load(file_path);
  adaptive_host* converted = file_map->file().construct<adaptive_host>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, comment_to_protobuf) {
  srand(time(nullptr));
  Comment pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_author(random_string());
  pb.set_type(Comment_Type(rand() % 3));
  pb.set_data(random_string());
  pb.set_deletion_time(rand() + 1);
  pb.set_entry_time(rand() + 1);
  pb.set_entry_type(Comment_EntryType(rand() % 5));
  pb.set_expire_time(rand() + 1);
  pb.set_expires(rand() % 2);
  pb.set_host_id(rand() + 1);
  pb.set_internal_id(rand() + 1);
  pb.set_persistent(rand() % 2);
  pb.set_instance_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_source(Comment_Src(rand() % 2));

  auto file_map = simple_global_cache::load(file_path);
  comment* converted = file_map->file().construct<comment>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, downtime_to_protobuf) {
  srand(time(nullptr));
  Downtime pb;
  pb.set_id(rand() + 1);
  pb.set_instance_id(rand() + 1);
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_author(random_string());
  pb.set_comment_data(random_string());
  pb.set_type(Downtime_DowntimeType(rand() % 4));
  pb.set_duration(rand() % 3600 + 1);
  pb.set_triggered_by(rand() + 1);
  pb.set_entry_time(rand() + 1);
  pb.set_actual_start_time(rand() + 1);
  pb.set_actual_end_time(rand() + 1);
  pb.set_start_time(rand() + 1);
  pb.set_deletion_time(rand() + 1);
  pb.set_end_time(rand() + 1);
  pb.set_started(rand() % 2);
  pb.set_cancelled(rand() % 2);
  pb.set_fixed(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  downtime* converted = file_map->file().construct<downtime>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, custom_variable_to_protobuf) {
  srand(time(nullptr));
  CustomVariable pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_modified(rand() % 2);
  pb.set_name(random_string());
  pb.set_update_time(rand() + 1);
  pb.set_value(random_string());
  pb.set_default_value(random_string());
  pb.set_enabled(rand() % 2);
  pb.set_password(rand() % 2);
  pb.set_type(CustomVariable_VarType(rand() % 2));

  auto file_map = simple_global_cache::load(file_path);
  custom_variable* converted = file_map->file().construct<custom_variable>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, host_check_to_protobuf) {
  srand(time(nullptr));
  HostCheck pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_active_checks_enabled(rand() % 2);
  pb.set_check_type(CheckType(rand() % 2));
  pb.set_command_line(random_string());
  pb.set_host_id(rand() + 1);
  pb.set_next_check(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  host_check* converted = file_map->file().construct<host_check>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, service_check_to_protobuf) {
  srand(time(nullptr));
  ServiceCheck pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_active_checks_enabled(rand() % 2);
  pb.set_check_type(CheckType(rand() % 2));
  pb.set_command_line(random_string());
  pb.set_host_id(rand() + 1);
  pb.set_next_check(rand() + 1);
  pb.set_service_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  service_check* converted = file_map->file().construct<service_check>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, log_entry_to_protobuf) {
  srand(time(nullptr));
  LogEntry pb;
  pb.set_ctime(rand() + 1);
  pb.set_instance_name(random_string());
  pb.set_output(random_string());
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_host_name(random_string());
  pb.set_service_description(random_string());
  pb.set_notification_contact(random_string());
  pb.set_notification_cmd(random_string());
  pb.set_type(LogEntry_LogType(rand() % 2));
  pb.set_msg_type(LogEntry_MsgType(rand() % 16));
  pb.set_status(rand() % 5);
  pb.set_retry(rand() % 10);

  auto file_map = simple_global_cache::load(file_path);
  log_entry* converted = file_map->file().construct<log_entry>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, instance_status_to_protobuf) {
  srand(time(nullptr));
  InstanceStatus pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_event_handlers(rand() % 2);
  pb.set_flap_detection(rand() % 2);
  pb.set_notifications(rand() % 2);
  pb.set_active_host_checks(rand() % 2);
  pb.set_active_service_checks(rand() % 2);
  pb.set_check_hosts_freshness(rand() % 2);
  pb.set_check_services_freshness(rand() % 2);
  pb.set_global_host_event_handler(random_string());
  pb.set_global_service_event_handler(random_string());
  pb.set_last_alive(rand() + 1);
  pb.set_last_command_check(rand() + 1);
  pb.set_obsess_over_hosts(rand() % 2);
  pb.set_obsess_over_services(rand() % 2);
  pb.set_passive_host_checks(rand() % 2);
  pb.set_passive_service_checks(rand() % 2);
  pb.set_instance_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  instance_status* converted = file_map->file().construct<instance_status>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, instance_to_protobuf) {
  srand(time(nullptr));
  Instance pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_engine(random_string());
  pb.set_running(rand() % 2);
  pb.set_name(random_string());
  pb.set_pid(rand() + 1);
  pb.set_instance_id(rand() + 1);
  pb.set_end_time(rand() + 1);
  pb.set_start_time(rand() + 1);
  pb.set_version(random_string());

  auto file_map = simple_global_cache::load(file_path);
  instance* converted = file_map->file().construct<instance>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, responsive_instance_to_protobuf) {
  srand(time(nullptr));
  ResponsiveInstance pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_poller_id(rand() + 1);
  pb.set_responsive(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  responsive_instance* converted =
      file_map->file().construct<responsive_instance>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, acknowledgement_to_protobuf) {
  srand(time(nullptr));
  Acknowledgement pb;
  pb.set_host_id(rand() + 1);
  pb.set_service_id(rand() + 1);
  pb.set_instance_id(rand() + 1);
  pb.set_type(Acknowledgement_ResourceType(rand() % 2));
  pb.set_author(random_string());
  pb.set_comment_data(random_string());
  pb.set_sticky(rand() % 2);
  pb.set_notify_contacts(rand() % 2);
  pb.set_entry_time(rand() + 1);
  pb.set_deletion_time(rand() + 1);
  pb.set_persistent_comment(rand() % 2);
  pb.set_state(rand() % 4);

  auto file_map = simple_global_cache::load(file_path);
  acknowledgement* converted = file_map->file().construct<acknowledgement>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, host_group_to_protobuf) {
  srand(time(nullptr));
  HostGroup pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_enabled(rand() % 2);
  pb.set_hostgroup_id(rand() + 1);
  pb.set_name(random_string());
  pb.set_poller_id(rand() + 1);
  pb.set_alias(random_string());

  auto file_map = simple_global_cache::load(file_path);
  host_group* converted = file_map->file().construct<host_group>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, service_group_to_protobuf) {
  srand(time(nullptr));
  ServiceGroup pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_enabled(rand() % 2);
  pb.set_servicegroup_id(rand() + 1);
  pb.set_name(random_string());
  pb.set_poller_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  service_group* converted = file_map->file().construct<service_group>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, host_group_member_to_protobuf) {
  srand(time(nullptr));
  HostGroupMember pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_enabled(rand() % 2);
  pb.set_hostgroup_id(rand() + 1);
  pb.set_name(random_string());
  pb.set_host_id(rand() + 1);
  pb.set_poller_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  host_group_member* converted = file_map->file().construct<host_group_member>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, service_group_member_to_protobuf) {
  srand(time(nullptr));
  ServiceGroupMember pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_enabled(rand() % 2);
  pb.set_servicegroup_id(rand() + 1);
  pb.set_name(random_string());
  pb.set_host_id(rand() + 1);
  pb.set_poller_id(rand() + 1);
  pb.set_service_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  service_group_member* converted =
      file_map->file().construct<service_group_member>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, host_parent_to_protobuf) {
  srand(time(nullptr));
  HostParent pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_enabled(rand() % 2);
  pb.set_child_id(rand() + 1);
  pb.set_parent_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  host_parent* converted = file_map->file().construct<host_parent>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, instance_configuration_to_protobuf) {
  srand(time(nullptr));
  InstanceConfiguration pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_loaded(rand() % 2);
  pb.set_poller_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  instance_configuration* converted =
      file_map->file().construct<instance_configuration>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, severity_to_protobuf) {
  srand(time(nullptr));
  Severity pb;
  pb.set_id(rand() + 1);
  pb.set_action(Severity_Action(rand() % 3));
  pb.set_level(rand() % 100 + 1);
  pb.set_icon_id(rand() + 1);
  pb.set_name(random_string());
  pb.set_type(Severity_Type(rand() % 2));
  pb.set_poller_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  severity* converted = file_map->file().construct<severity>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, tag_to_protobuf) {
  srand(time(nullptr));
  Tag pb;
  pb.set_id(rand() + 1);
  pb.set_action(Tag_Action(rand() % 3));
  pb.set_type(TagType(rand() % 4));
  pb.set_name(random_string());
  pb.set_poller_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  tag* converted =
      file_map->file().construct<tag>("my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, tag_info_to_protobuf) {
  srand(time(nullptr));
  TagInfo pb;
  pb.set_id(rand() + 1);
  pb.set_type(TagType(rand() % 4));

  auto file_map = simple_global_cache::load(file_path);
  tag_info* converted = file_map->file().construct<tag_info>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, agent_info_to_protobuf) {
  srand(time(nullptr));
  AgentInfo pb = create_random_agent_info();

  auto file_map = simple_global_cache::load(file_path);
  agent_info* converted = file_map->file().construct<agent_info>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, agent_stats_to_protobuf) {
  srand(time(nullptr));
  AgentStats pb;
  pb.set_poller_id(rand() + 1);
  for (int i = 1 + rand() % 2; i > 0; --i)
    *pb.add_stats() = create_random_agent_info();

  auto file_map = simple_global_cache::load(file_path);
  agent_stats* converted = file_map->file().construct<agent_stats>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, bbdo_header_to_protobuf) {
  srand(time(nullptr));
  BBDOHeader pb;
  pb.set_conf_version(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  bbdo_header* converted = file_map->file().construct<bbdo_header>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, inherited_downtime_to_protobuf) {
  srand(time(nullptr));
  InheritedDowntime pb;
  *pb.mutable_header() = create_random_bbdo_header();
  pb.set_ba_id(rand() + 1);
  pb.set_in_downtime(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  inherited_downtime* converted =
      file_map->file().construct<inherited_downtime>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, ba_status_to_protobuf) {
  srand(time(nullptr));
  BaStatus pb;
  pb.set_ba_id(rand() + 1);
  pb.set_in_downtime(rand() % 2);
  pb.set_last_state_change(rand() + 1);
  pb.set_level_acknowledgement((double)(rand() % 100 + 1));
  pb.set_level_downtime((double)(rand() % 100 + 1));
  pb.set_level_nominal((double)(rand() % 100 + 1));
  pb.set_state(State(rand() % 4));
  pb.set_state_changed(rand() % 2);
  pb.set_output(random_string());

  auto file_map = simple_global_cache::load(file_path);
  ba_status* converted = file_map->file().construct<ba_status>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, ba_event_to_protobuf) {
  srand(time(nullptr));
  BaEvent pb;
  pb.set_ba_id(rand() + 1);
  pb.set_first_level((double)(rand() % 100 + 1));
  pb.set_end_time(rand() + 1);
  pb.set_in_downtime(rand() % 2);
  pb.set_start_time(rand() + 1);
  pb.set_status(State(rand() % 4));

  auto file_map = simple_global_cache::load(file_path);
  ba_event* converted = file_map->file().construct<ba_event>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, kpi_event_to_protobuf) {
  srand(time(nullptr));
  KpiEvent pb;
  pb.set_ba_id(rand() + 1);
  pb.set_start_time(rand() + 1);
  pb.set_end_time(rand() + 1);
  pb.set_kpi_id(rand() + 1);
  pb.set_impact_level(rand() % 100 + 1);
  pb.set_in_downtime(rand() % 2);
  pb.set_output(random_string());
  pb.set_perfdata(random_string());
  pb.set_status(State(rand() % 4));

  auto file_map = simple_global_cache::load(file_path);
  kpi_event* converted = file_map->file().construct<kpi_event>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_bv_event_to_protobuf) {
  srand(time(nullptr));
  DimensionBvEvent pb;
  pb.set_bv_id(rand() + 1);
  pb.set_bv_name(random_string());
  pb.set_bv_description(random_string());

  auto file_map = simple_global_cache::load(file_path);
  dimension_bv_event* converted =
      file_map->file().construct<dimension_bv_event>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_ba_bv_relation_event_to_protobuf) {
  srand(time(nullptr));
  DimensionBaBvRelationEvent pb;
  pb.set_ba_id(rand() + 1);
  pb.set_bv_id(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  dimension_ba_bv_relation_event* converted =
      file_map->file().construct<dimension_ba_bv_relation_event>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_timeperiod_to_protobuf) {
  srand(time(nullptr));
  DimensionTimeperiod pb;
  pb.set_id(rand() + 1);
  pb.set_name(random_string());
  pb.set_monday(random_string());
  pb.set_tuesday(random_string());
  pb.set_wednesday(random_string());
  pb.set_thursday(random_string());
  pb.set_friday(random_string());
  pb.set_saturday(random_string());
  pb.set_sunday(random_string());

  auto file_map = simple_global_cache::load(file_path);
  dimension_timeperiod* converted =
      file_map->file().construct<dimension_timeperiod>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_ba_event_to_protobuf) {
  srand(time(nullptr));
  DimensionBaEvent pb;
  pb.set_ba_id(rand() + 1);
  pb.set_ba_name(random_string());
  pb.set_ba_description(random_string());
  pb.set_sla_month_percent_crit((double)(rand() % 100 + 1));
  pb.set_sla_month_percent_warn((double)(rand() % 100 + 1));
  pb.set_sla_duration_crit(rand() + 1);
  pb.set_sla_duration_warn(rand() + 1);

  auto file_map = simple_global_cache::load(file_path);
  dimension_ba_event* converted =
      file_map->file().construct<dimension_ba_event>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_kpi_event_to_protobuf) {
  srand(time(nullptr));
  DimensionKpiEvent pb;
  pb.set_kpi_id(rand() + 1);
  pb.set_ba_id(rand() + 1);
  pb.set_ba_name(random_string());
  pb.set_host_id(rand() + 1);
  pb.set_host_name(random_string());
  pb.set_service_id(rand() + 1);
  pb.set_service_description(random_string());
  pb.set_kpi_ba_id(rand() + 1);
  pb.set_kpi_ba_name(random_string());
  pb.set_meta_service_id(rand() + 1);
  pb.set_meta_service_name(random_string());
  pb.set_boolean_id(rand() + 1);
  pb.set_boolean_name(random_string());
  pb.set_impact_warning((double)(rand() % 100 + 1));
  pb.set_impact_critical((double)(rand() % 100 + 1));
  pb.set_impact_unknown((double)(rand() % 100 + 1));

  auto file_map = simple_global_cache::load(file_path);
  dimension_kpi_event* converted =
      file_map->file().construct<dimension_kpi_event>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, kpi_status_to_protobuf) {
  srand(time(nullptr));
  KpiStatus pb;
  pb.set_kpi_id(rand() + 1);
  pb.set_in_downtime(rand() % 2);
  pb.set_level_acknowledgement_hard((double)(rand() % 100 + 1));
  pb.set_level_acknowledgement_soft((double)(rand() % 100 + 1));
  pb.set_level_downtime_hard((double)(rand() % 100 + 1));
  pb.set_level_downtime_soft((double)(rand() % 100 + 1));
  pb.set_level_nominal_hard((double)(rand() % 100 + 1));
  pb.set_level_nominal_soft((double)(rand() % 100 + 1));
  pb.set_state_hard(State(rand() % 4));
  pb.set_state_soft(State(rand() % 4));
  pb.set_last_state_change(rand() + 1);
  pb.set_last_impact((double)(rand() % 100 + 1));
  pb.set_valid(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  kpi_status* converted = file_map->file().construct<kpi_status>("my_object")(
      pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, ba_duration_event_to_protobuf) {
  srand(time(nullptr));
  BaDurationEvent pb;
  pb.set_ba_id(rand() + 1);
  pb.set_real_start_time(rand() + 1);
  pb.set_end_time(rand() + 1);
  pb.set_start_time(rand() + 1);
  pb.set_duration(rand() % 3600 + 1);
  pb.set_sla_duration(rand() % 3600 + 1);
  pb.set_timeperiod_id(rand() + 1);
  pb.set_timeperiod_is_default(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  ba_duration_event* converted = file_map->file().construct<ba_duration_event>(
      "my_object")(pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_ba_timeperiod_relation_to_protobuf) {
  srand(time(nullptr));
  DimensionBaTimeperiodRelation pb;
  pb.set_ba_id(rand() + 1);
  pb.set_timeperiod_id(rand() + 1);
  pb.set_is_default(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  dimension_ba_timeperiod_relation* converted =
      file_map->file().construct<dimension_ba_timeperiod_relation>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}

TEST_F(protobuf_test, dimension_truncate_table_signal_to_protobuf) {
  srand(time(nullptr));
  DimensionTruncateTableSignal pb;
  pb.set_update_started(rand() % 2);

  auto file_map = simple_global_cache::load(file_path);
  dimension_truncate_table_signal* converted =
      file_map->file().construct<dimension_truncate_table_signal>("my_object")(
          pb, *file_map->_allocators);
  ASSERT_PROTO_EQ(pb, converted);
}