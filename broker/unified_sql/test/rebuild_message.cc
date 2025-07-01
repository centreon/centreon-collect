/**
 * Copyright 2021 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <arpa/inet.h>
#include <gtest/gtest.h>

#include "broker/core/bbdo/stream.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/lua/macro_cache.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/misc/variant.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::misc;
using namespace google::protobuf::util;
using com::centreon::common::log_v2::log_v2;

class into_memory : public io::stream {
 public:
  into_memory() : io::stream("into_memory"), _memory() {}
  ~into_memory() override {}

  bool read(std::shared_ptr<io::data>& d,
            time_t deadline = (time_t)-1) override {
    (void)deadline;
    if (_memory.empty())
      return false;
    std::shared_ptr<io::raw> raw(new io::raw);
    raw->get_buffer() = std::move(_memory);
    _memory.clear();
    d = raw;
    return true;
  }

  int write(std::shared_ptr<io::data> const& d) override {
    _memory = std::static_pointer_cast<io::raw>(d)->get_buffer();
    return 1;
  }

  int32_t stop() override { return 0; }

  std::vector<char> const& get_memory() const { return _memory; }
  std::vector<char>& get_mutable_memory() { return _memory; }

 private:
  std::vector<char> _memory;
};

class UnifiedSqlRebuild2Test : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = log_v2::instance().get(log_v2::SQL);
    io::data::broker_id = 0;
    try {
      config::applier::init(com::centreon::common::BROKER, "", 0, "broker_test",
                            0);
    } catch (std::exception const& e) {
      (void)e;
    }
    std::shared_ptr<persistent_cache> pcache(std::make_shared<persistent_cache>(
        "/tmp/broker_test_cache", log_v2::instance().get(log_v2::SQL)));
  }

  void TearDown() override {
    // The cache must be destroyed before the applier deinit() call.
    config::applier::deinit();
    ::remove("/tmp/broker_test_cache");
    ::remove(log_v2::instance().filename().c_str());
  }
};

// When the first rebuild message is sent, it contains the START flag
// and a vector of metric ids.
// Then the receiver can deserialize it.
TEST_F(UnifiedSqlRebuild2Test, WriteRebuildMessage_START) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/lib/20-unified_sql.so");

  std::shared_ptr<storage::pb_rebuild_message> r(
      std::make_shared<storage::pb_rebuild_message>());
  r->mut_obj().set_state(RebuildMessage_State_START);
  (*r->mut_obj().mutable_metric_to_index_id())[1] = 1;
  (*r->mut_obj().mutable_metric_to_index_id())[2] = 1;
  (*r->mut_obj().mutable_metric_to_index_id())[5] = 1;

  std::shared_ptr<into_memory> memory_stream(std::make_shared<into_memory>());
  bbdo::stream stm(true);
  stm.set_substream(memory_stream);
  stm.set_coarse(false);
  stm.set_negotiate(false);
  stm.negotiate(bbdo::stream::negotiate_first);
  stm.write(r);
  std::vector<char> const& mem1 = memory_stream->get_memory();

  constexpr size_t size = 34u;
  ASSERT_EQ(mem1.size(), size);
  // The size of the protobuf part is size - 16: 16 is the header size.
  for (uint32_t i = 0; i < size; i++) {
    printf("%02x ", static_cast<unsigned int>(0xff & mem1[i]));
    if ((i & 0x1f) == 0)
      puts("");
  }
  puts("");

  ASSERT_EQ(htons(*reinterpret_cast<uint16_t const*>(&mem1[0] + 2)), size - 16);

  std::shared_ptr<io::data> e;
  stm.read(e, time(nullptr) + 1000);
  std::shared_ptr<storage::pb_rebuild_message> new_r =
      std::static_pointer_cast<storage::pb_rebuild_message>(e);
  ASSERT_TRUE(MessageDifferencer::Equals(r->obj(), new_r->obj()));
}

// When the second rebuild message is sent, it contains the DATA flag
// and a map contains data for each metric id.
// Then the receiver can deserialize it.
TEST_F(UnifiedSqlRebuild2Test, WriteRebuildMessage_DATA) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/lib/20-unified_sql.so");

  std::shared_ptr<storage::pb_rebuild_message> r(
      std::make_shared<storage::pb_rebuild_message>());
  r->mut_obj().set_state(RebuildMessage_State_DATA);
  for (int64_t i : {1, 2, 5}) {
    Timeserie& ts = (*r->mut_obj().mutable_timeserie())[i];
    ts.set_data_source_type(1);
    ts.set_check_interval(250);
    for (int j = 0; i < 20000; i++) {
      Point* pt = ts.add_pts();
      pt->set_ctime(j);
      pt->set_value(j * 0.1);
    }
  }

  std::shared_ptr<into_memory> memory_stream(std::make_shared<into_memory>());
  bbdo::stream stm(true);
  stm.set_substream(memory_stream);
  stm.set_coarse(false);
  stm.set_negotiate(false);
  stm.negotiate(bbdo::stream::negotiate_first);
  stm.write(r);
  const std::vector<char>& mem1 = memory_stream->get_memory();

  constexpr size_t size = 120063;
  ASSERT_EQ(mem1.size(), size);

  std::shared_ptr<io::data> e;
  stm.read(e, time(nullptr) + 1000);
  std::shared_ptr<storage::pb_rebuild_message> new_r =
      std::static_pointer_cast<storage::pb_rebuild_message>(e);
  ASSERT_TRUE(MessageDifferencer::Equals(r->obj(), new_r->obj()));
}
