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

#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/cache/protobuf.hh"
#include "com/centreon/broker/cache/protobuf_utils.hh"
#include "neb.pb.h"

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

  const host_serv_pair* get_host_serv_id(uint64_t index_id) override {
    return nullptr;
  }

  void write(const std::shared_ptr<io::data>& d) override {}

  const host* get_host(uint64_t host_id, upgrade_lock& read_lock) override {
    return nullptr;
  }
  virtual const service* get_service(uint64_t host_id,
                                     uint64_t service_id,
                                     upgrade_lock& read_lock) override {
    return nullptr;
  }

  virtual const instance* get_instance(uint64_t instance_id,
                                       upgrade_lock& read_lock) override {
    return nullptr;
  }

  using tag_id_enumerator = std::function<uint64_t()>;
  virtual void append_service_group(uint64_t host,
                                    uint64_t service,
                                    std::ostream& request_body) override {}
  virtual void append_host_group(uint64_t host,
                                 std::ostream& request_body) override {}
  virtual void append_host_tag_id(uint64_t host,
                                  TagType tag_type,
                                  std::ostream& request_body) override {}
  virtual void append_serv_tag_id(uint64_t host,
                                  uint64_t serv,
                                  TagType tag_type,
                                  std::ostream& request_body) override {}
  virtual void append_host_tag_name(uint64_t host,
                                    TagType tag_type,
                                    std::ostream& request_body) override {}
  virtual void append_serv_tag_name(uint64_t host,
                                    uint64_t serv,
                                    TagType tag_type,
                                    std::ostream& request_body) override {}

  virtual uint64_t get_index_id_from_metric_id(uint64_t metric_id) override {
    return 0;
  }

  virtual int32_t get_severity(const uint64_t host_id,
                               const uint64_t service_id) override {
    return 0;
  }
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
    ret.push_back(rand() + 1);
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

std::string to_string(const string& src) {
  return std::string(src.c_str(), src.length());
}

bool compare_agent_info(const AgentInfo& left, const agent_info& right) {
  std::vector<variant> fields =
      static_cast<const message*>(&right)->enumerate_fields();

  bool g_test_failed = false;
  EXPECT_EQ_MARK(left.major(), right.major());
  EXPECT_EQ_MARK(left.major(), std::get<uint32_t>(fields[0]));
  EXPECT_EQ_MARK(left.minor(), right.minor());
  EXPECT_EQ_MARK(left.minor(), std::get<uint32_t>(fields[1]));
  EXPECT_EQ_MARK(left.patch(), right.patch());
  EXPECT_EQ_MARK(left.patch(), std::get<uint32_t>(fields[2]));
  EXPECT_EQ_MARK(left.reverse(), right.reverse());
  EXPECT_EQ_MARK(left.reverse(), std::get<bool>(fields[3]));
  EXPECT_EQ_MARK(left.os(), to_string(right.os()));
  EXPECT_EQ_MARK(left.os(), to_string(*std::get<const string*>(fields[4])));
  EXPECT_EQ_MARK(left.os_version(), to_string(right.os_version()));
  EXPECT_EQ_MARK(left.os_version(),
                 to_string(*std::get<const string*>(fields[5])));
  EXPECT_EQ_MARK(left.nb_agent(), right.nb_agent());
  EXPECT_EQ_MARK(left.nb_agent(), std::get<uint32_t>(fields[6]));

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

  std::vector<variant> fields =
      static_cast<message*>(converted)->enumerate_fields();

  ASSERT_EQ(pb.poller_id(), converted->poller_id());
  ASSERT_EQ(pb.poller_id(), std::get<int64_t>(fields[0]));
  ASSERT_EQ(pb.stats().size(), converted->stats().size());
  auto pb_iter = pb.stats().begin();
  auto conv_iter = converted->stats().begin();
  auto sub_field_iter = std::get<const mess_vect*>(fields[1])->begin();
  for (; pb_iter != pb.stats().end();
       ++pb_iter, ++conv_iter, ++sub_field_iter) {
    ASSERT_TRUE(compare_agent_info(
        *pb_iter, static_cast<const agent_info&>(**conv_iter)));
    ASSERT_EQ(sub_field_iter->get(), conv_iter->get());
  }

  AgentStats pb2;
  pb2.set_poller_id(rand());
  nb_agent = rand() % 10;
  for (; nb_agent; --nb_agent) {
    *pb2.add_stats() = create_random_agent_info();
  }
  converted->update(pb2, *file_map->_allocators);

  fields = static_cast<message*>(converted)->enumerate_fields();

  ASSERT_EQ(pb2.poller_id(), converted->poller_id());
  ASSERT_EQ(pb2.poller_id(), std::get<int64_t>(fields[0]));
  ASSERT_EQ(pb2.stats().size(), converted->stats().size());
  pb_iter = pb2.stats().begin();
  conv_iter = converted->stats().begin();
  sub_field_iter = std::get<const mess_vect*>(fields[1])->begin();
  for (; pb_iter != pb2.stats().end();
       ++pb_iter, ++conv_iter, ++sub_field_iter) {
    ASSERT_TRUE(compare_agent_info(
        *pb_iter, static_cast<const agent_info&>(**conv_iter)));
    ASSERT_EQ(sub_field_iter->get(), conv_iter->get());
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

  std::vector<variant> fields =
      static_cast<message*>(converted)->enumerate_fields();

  ASSERT_EQ(pb.header().conf_version(),
            static_cast<const bbdo_header*>(converted->header().get())
                ->conf_version());
  ASSERT_EQ(pb.header().conf_version(),
            static_cast<const bbdo_header*>(std::get<const message*>(fields[0]))
                ->conf_version());

  ASSERT_EQ(pb.author(), to_string(converted->author()));
  ASSERT_EQ(pb.author(), to_string(*std::get<const string*>(fields[1])));
  ASSERT_EQ(pb.type(), converted->type());
  ASSERT_EQ(pb.type(), std::get<uint32_t>(fields[2]));
  ASSERT_EQ(pb.data(), to_string(converted->data()));
  ASSERT_EQ(pb.data(), to_string(*std::get<const string*>(fields[3])));
  ASSERT_EQ(pb.deletion_time(), converted->deletion_time());
  ASSERT_EQ(pb.deletion_time(), std::get<uint64_t>(fields[4]));
  ASSERT_EQ(pb.entry_time(), converted->entry_time());
  ASSERT_EQ(pb.entry_time(), std::get<uint64_t>(fields[5]));
  ASSERT_EQ(pb.entry_type(), converted->entry_type());
  ASSERT_EQ(pb.entry_type(), std::get<uint32_t>(fields[6]));
  ASSERT_EQ(pb.expire_time(), converted->expire_time());
  ASSERT_EQ(pb.expire_time(), std::get<uint64_t>(fields[7]));
  ASSERT_EQ(pb.expires(), converted->expires());
  ASSERT_EQ(pb.expires(), std::get<bool>(fields[8]));
  ASSERT_EQ(pb.host_id(), converted->host_id());
  ASSERT_EQ(pb.host_id(), std::get<uint64_t>(fields[9]));
  ASSERT_EQ(pb.internal_id(), converted->internal_id());
  ASSERT_EQ(pb.internal_id(), std::get<uint64_t>(fields[10]));
  ASSERT_EQ(pb.persistent(), converted->persistent());
  ASSERT_EQ(pb.persistent(), std::get<bool>(fields[11]));
  ASSERT_EQ(pb.instance_id(), converted->instance_id());
  ASSERT_EQ(pb.instance_id(), std::get<uint64_t>(fields[12]));
  ASSERT_EQ(pb.service_id(), converted->service_id());
  ASSERT_EQ(pb.service_id(), std::get<uint64_t>(fields[13]));
  ASSERT_EQ(pb.source(), converted->source());
  ASSERT_EQ(pb.source(), std::get<uint32_t>(fields[14]));

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
  fields = static_cast<message*>(converted)->enumerate_fields();

  ASSERT_EQ(pb2.header().conf_version(),
            static_cast<const bbdo_header*>(converted->header().get())
                ->conf_version());
  ASSERT_EQ(pb2.header().conf_version(),
            static_cast<const bbdo_header*>(std::get<const message*>(fields[0]))
                ->conf_version());

  ASSERT_EQ(pb2.author(), to_string(converted->author()));
  ASSERT_EQ(pb2.author(), to_string(*std::get<const string*>(fields[1])));
  ASSERT_EQ(pb2.type(), converted->type());
  ASSERT_EQ(pb2.type(), std::get<uint32_t>(fields[2]));
  ASSERT_EQ(pb2.data(), to_string(converted->data()));
  ASSERT_EQ(pb2.data(), to_string(*std::get<const string*>(fields[3])));
  ASSERT_EQ(pb2.deletion_time(), converted->deletion_time());
  ASSERT_EQ(pb2.deletion_time(), std::get<uint64_t>(fields[4]));
  ASSERT_EQ(pb2.entry_time(), converted->entry_time());
  ASSERT_EQ(pb2.entry_time(), std::get<uint64_t>(fields[5]));
  ASSERT_EQ(pb2.entry_type(), converted->entry_type());
  ASSERT_EQ(pb2.entry_type(), std::get<uint32_t>(fields[6]));
  ASSERT_EQ(pb2.expire_time(), converted->expire_time());
  ASSERT_EQ(pb2.expire_time(), std::get<uint64_t>(fields[7]));
  ASSERT_EQ(pb2.expires(), converted->expires());
  ASSERT_EQ(pb2.expires(), std::get<bool>(fields[8]));
  ASSERT_EQ(pb2.host_id(), converted->host_id());
  ASSERT_EQ(pb2.host_id(), std::get<uint64_t>(fields[9]));
  ASSERT_EQ(pb2.internal_id(), converted->internal_id());
  ASSERT_EQ(pb2.internal_id(), std::get<uint64_t>(fields[10]));
  ASSERT_EQ(pb2.persistent(), converted->persistent());
  ASSERT_EQ(pb2.persistent(), std::get<bool>(fields[11]));
  ASSERT_EQ(pb2.instance_id(), converted->instance_id());
  ASSERT_EQ(pb2.instance_id(), std::get<uint64_t>(fields[12]));
  ASSERT_EQ(pb2.service_id(), converted->service_id());
  ASSERT_EQ(pb2.service_id(), std::get<uint64_t>(fields[13]));
  ASSERT_EQ(pb2.source(), converted->source());
  ASSERT_EQ(pb2.source(), std::get<uint32_t>(fields[14]));
}

#define SET_OPTIONAL_UINT(pb_name, field, max_value) \
  if (rand() % 2) {                                  \
    pb_name.set_##field(rand() % max_value);         \
  }

#define SET_OPTIONAL_STRING(pb_name, field) \
  if (rand() % 2) {                         \
    pb_name.set_##field(random_string());   \
  }

#define COMPARE_OPTIONAL(pb_name, field, type, enum_index)                  \
  ASSERT_EQ(pb_name.has_##field(), converted->field().has_value());         \
  ASSERT_EQ(pb_name.has_##field(),                                          \
            std::get<std::optional<type>>(fields[enum_index]).has_value()); \
  if (pb_name.has_##field()) {                                              \
    ASSERT_EQ(pb_name.field(), converted->field().value());                 \
    ASSERT_EQ(pb_name.field(),                                              \
              std::get<std::optional<type>>(fields[enum_index]).value());   \
  }

#define COMPARE_OPTIONAL_STRING(pb_name, field, enum_index)                  \
  ASSERT_EQ(pb_name.has_##field(), static_cast<bool>(converted->field()));   \
  ASSERT_EQ(pb_name.has_##field(),                                           \
            static_cast<bool>(std::get<const string*>(fields[enum_index]))); \
  if (pb_name.has_##field()) {                                               \
    ASSERT_EQ(pb_name.field(), to_string(*converted->field()));              \
    ASSERT_EQ(pb_name.field(),                                               \
              to_string(*std::get<const string*>(fields[enum_index])));      \
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

  std::vector<variant> fields =
      static_cast<message*>(converted)->enumerate_fields();

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
  fields = static_cast<message*>(converted)->enumerate_fields();

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