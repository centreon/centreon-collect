/*
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

#include <google/protobuf/util/message_differencer.h>
#include <fstream>
#include <list>
#include <memory>

#include "com/centreon/broker/bbdo/stream.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/lua/macro_cache.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/misc/variant.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/persistent_file.hh"
#include "com/centreon/broker/unified_sql/internal.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::misc;
using namespace google::protobuf::util;

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
 public:
  void SetUp() override {
    io::data::broker_id = 0;
    try {
      config::applier::init(0, "broker_test");
    } catch (std::exception const& e) {
      (void)e;
    }
    std::shared_ptr<persistent_cache> pcache(
        std::make_shared<persistent_cache>("/tmp/broker_test_cache"));
  }

  void TearDown() override {
    // The cache must be destroyed before the applier deinit() call.
    config::applier::deinit();
    ::remove("/tmp/broker_test_cache");
    ::remove(log_v2::instance().log_name().c_str());
  }
};

// When a script is correctly loaded and a neb event has to be sent
// Then this event is translated into a Lua table and sent to the lua write()
// function.
TEST_F(UnifiedSqlRebuild2Test, WriteReadRebuild2) {
  config::applier::modules modules;
  modules.load_file("./unified_sql/20-unified_sql.so");

  std::shared_ptr<unified_sql::pb_rebuild> r(
      std::make_shared<unified_sql::pb_rebuild>());
  r->obj.mutable_metric()->set_metric_id(1234);
  r->obj.mutable_metric()->set_value_type(0);
  for (int i = 0; i < 20; i++) {
    Point* p = r->obj.add_data();
    p->set_ctime(i);
    p->set_value(i * i);
  }

  std::shared_ptr<into_memory> memory_stream(std::make_shared<into_memory>());
  bbdo::stream stm(true);
  stm.set_substream(memory_stream);
  stm.set_coarse(false);
  stm.set_negotiate(false);
  stm.negotiate(bbdo::stream::negotiate_first);
  stm.write(r);
  std::vector<char> const& mem1 = memory_stream->get_memory();

  constexpr size_t size = 270u;
  ASSERT_EQ(mem1.size(), size);
  // The size is size - 16: 16 is the header size.
  for (uint32_t i = 0; i < size; i++) {
    printf("%02x ", static_cast<unsigned int>(0xff & mem1[i]));
    if ((i & 0x1f) == 0)
      puts("");
  }
  puts("");

  ASSERT_EQ(htons(*reinterpret_cast<uint16_t const*>(&mem1[0] + 2)), size - 16);

  std::shared_ptr<io::data> e;
  stm.read(e, time(nullptr) + 1000);
  std::shared_ptr<unified_sql::pb_rebuild> new_r =
      std::static_pointer_cast<unified_sql::pb_rebuild>(e);
  ASSERT_TRUE(MessageDifferencer::Equals(r->obj, new_r->obj));
}

// When a script is correctly loaded and a neb event has to be sent
// Then this event is translated into a Lua table and sent to the lua write()
// function.
TEST_F(UnifiedSqlRebuild2Test, LongWriteReadRebuild2) {
  config::applier::modules modules;
  modules.load_file("./unified_sql/20-unified_sql.so");

  std::shared_ptr<unified_sql::pb_rebuild> r(
      std::make_shared<unified_sql::pb_rebuild>());
  r->obj.mutable_metric()->set_metric_id(1234);
  r->obj.mutable_metric()->set_value_type(0);
  for (int i = 0; i < 20000; i++) {
    Point* p = r->obj.add_data();
    p->set_ctime(i);
    p->set_value(i * i);
  }

  std::shared_ptr<into_memory> memory_stream(std::make_shared<into_memory>());
  bbdo::stream stm(true);
  stm.set_substream(memory_stream);
  stm.set_coarse(false);
  stm.set_negotiate(false);
  stm.negotiate(bbdo::stream::negotiate_first);
  stm.write(r);
  std::vector<char> const& mem1 = memory_stream->get_memory();

  constexpr size_t size = 283562;
  ASSERT_EQ(mem1.size(), size);

  const char* tmp = &mem1[0];

  for (int i = 0; i < 4; i++) {
    std::cout << "test " << i << std::endl;
    ASSERT_EQ(htons(*reinterpret_cast<const uint16_t*>(tmp + 2)), 0xffff);
    tmp += 16 + 0xffff;
  }
  ASSERT_EQ(htons(*reinterpret_cast<const uint16_t*>(tmp + 2)), 21342);

  std::shared_ptr<io::data> e;
  stm.read(e, time(nullptr) + 1000);
  std::shared_ptr<unified_sql::pb_rebuild> new_r =
      std::static_pointer_cast<unified_sql::pb_rebuild>(e);
  ASSERT_TRUE(MessageDifferencer::Equals(r->obj, new_r->obj));
}
