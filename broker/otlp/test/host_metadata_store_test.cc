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

#include "com/centreon/broker/otlp/host_metadata_store.hh"

#include <gtest/gtest.h>

#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/persistent_cache.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using com::centreon::common::log_v2::log_v2;

namespace {

AgentHostInfo make_info(uint64_t poller_id,
                        const std::string& machine_id = "machine-1") {
  AgentHostInfo info;
  info.set_poller_id(poller_id);
  info.set_host_id(42);
  info.set_host_name("srv-web-01");
  info.set_os_type("linux");
  info.set_os_name("AlmaLinux");
  info.set_os_version("9.4");
  info.set_arch("amd64");
  info.set_machine_id(machine_id);
  info.add_ips("10.0.0.1");
  return info;
}

std::vector<std::string> ips_of(const AgentHostInfo& info) {
  return {info.ips().begin(), info.ips().end()};
}

}  // namespace

TEST(otlp_host_metadata_store, unknown_host) {
  host_metadata_store store;
  EXPECT_FALSE(store.get(42));
}

TEST(otlp_host_metadata_store, first_event_is_stored) {
  host_metadata_store store;
  store.set(make_info(1));
  auto meta = store.get(42);
  ASSERT_TRUE(meta);
  EXPECT_EQ(meta->os_type(), "linux");
  EXPECT_EQ(meta->os_name(), "AlmaLinux");
  EXPECT_EQ(meta->os_version(), "9.4");
  EXPECT_EQ(meta->arch(), "amd64");
  EXPECT_EQ(meta->machine_id(), "machine-1");
  EXPECT_EQ(ips_of(*meta), std::vector<std::string>{"10.0.0.1"});
  EXPECT_EQ(store.size(), 1u);
}

/* each event replaces the whole record, a field absent from the new event
 * is not kept from the old one */
TEST(otlp_host_metadata_store, event_replaces_the_whole_record) {
  host_metadata_store store;
  store.set(make_info(1));
  AgentHostInfo info = make_info(1);
  info.clear_ips();
  info.clear_os_name();
  store.set(info);
  auto meta = store.get(42);
  ASSERT_TRUE(meta);
  EXPECT_TRUE(meta->ips().empty());
  EXPECT_TRUE(meta->os_name().empty());
  EXPECT_EQ(store.size(), 1u);
}

/* engine re-sends the information periodically, an identical copy is not a
 * change */
TEST(otlp_host_metadata_store, set_tells_whether_the_record_changed) {
  host_metadata_store store;
  EXPECT_TRUE(store.set(make_info(1)));
  EXPECT_FALSE(store.set(make_info(1)));
  EXPECT_FALSE(store.set(make_info(2))) << "poller id is not stored";
  EXPECT_TRUE(store.set(make_info(1, "machine-2")));
}

/* no ordering between events: the last received one wins, whatever its
 * poller */
TEST(otlp_host_metadata_store, last_event_wins) {
  host_metadata_store store;
  store.set(make_info(2, "machine-2"));
  store.set(make_info(1, "machine-1"));
  EXPECT_EQ(store.get(42)->machine_id(), "machine-1");
}

namespace {

/**
 * @brief persistent cache in a fresh file, AgentHostInfo registered as neb
 * does so that it can be read back
 */
class otlp_host_metadata_cache : public ::testing::Test {
 protected:
  const std::string _path =
      (std::filesystem::temp_directory_path() / "otlp_host_metadata_test.cache")
          .string();

  void _remove_files() const {
    for (const char* suffix : {"", ".new", ".old"})
      std::filesystem::remove(_path + suffix);
  }

  std::shared_ptr<persistent_cache> _open_cache() const {
    return std::make_shared<persistent_cache>(
        _path, log_v2::instance().get(log_v2::OTL));
  }

 public:
  void SetUp() override {
    file::disk_accessor::load(100000);
    io::events::instance().register_event(
        neb::pb_agent_host_info::static_type(), "AgentHostInfo",
        &neb::pb_agent_host_info::operations, "no_table");
    _remove_files();
  }

  void TearDown() override {
    _remove_files();
    file::disk_accessor::unload();
  }
};

}  // namespace

TEST_F(otlp_host_metadata_cache, no_file_loads_nothing) {
  host_metadata_store store(_open_cache());
  EXPECT_EQ(store.load(), 0u);
  EXPECT_EQ(store.size(), 0u);
}

TEST(otlp_host_metadata_store, no_cache_saves_nothing) {
  host_metadata_store store;
  store.set(make_info(1));
  EXPECT_FALSE(store.save());
}

TEST_F(otlp_host_metadata_cache, saved_records_are_loaded) {
  host_metadata_store saved(_open_cache());
  saved.set(make_info(1));
  AgentHostInfo other = make_info(2, "machine-2");
  other.set_host_id(43);
  other.clear_os_name();
  other.add_ips("fe80::1");
  saved.set(other);
  EXPECT_TRUE(saved.save());

  host_metadata_store loaded(_open_cache());
  EXPECT_EQ(loaded.load(), 2u);
  EXPECT_EQ(loaded.size(), 2u);
  auto meta = loaded.get(42);
  ASSERT_TRUE(meta);
  EXPECT_EQ(meta->os_type(), "linux");
  EXPECT_EQ(meta->os_name(), "AlmaLinux");
  EXPECT_EQ(meta->os_version(), "9.4");
  EXPECT_EQ(meta->arch(), "amd64");
  EXPECT_EQ(meta->machine_id(), "machine-1");
  EXPECT_EQ(ips_of(*meta), std::vector<std::string>{"10.0.0.1"});
  meta = loaded.get(43);
  ASSERT_TRUE(meta);
  EXPECT_TRUE(meta->os_name().empty());
  EXPECT_EQ(meta->machine_id(), "machine-2");
  EXPECT_EQ(ips_of(*meta), (std::vector<std::string>{"10.0.0.1", "fe80::1"}));
}

/* the cache is only written when a record changed */
TEST_F(otlp_host_metadata_cache, only_changes_are_saved) {
  auto cache = _open_cache();
  host_metadata_store store(cache);
  EXPECT_FALSE(store.save()) << "nothing to save";
  store.set(make_info(1));
  EXPECT_TRUE(store.save());
  EXPECT_FALSE(store.save()) << "already saved";
  store.set(make_info(1));
  EXPECT_FALSE(store.save()) << "same information again";

  host_metadata_store loaded(_open_cache());
  EXPECT_EQ(loaded.load(), 1u);
  EXPECT_FALSE(loaded.save()) << "loaded records are not changes";
}

/* during a burst of changes, the cache is written at most every interval */
TEST_F(otlp_host_metadata_cache, saves_are_spaced_by_the_interval) {
  host_metadata_store store(_open_cache());
  const std::chrono::seconds interval(3600);
  store.set(make_info(1));
  EXPECT_TRUE(store.save(interval)) << "first change is saved at once";
  store.set(make_info(1, "machine-2"));
  EXPECT_FALSE(store.save(interval));
  EXPECT_TRUE(store.save()) << "the change is still pending";

  host_metadata_store loaded(_open_cache());
  loaded.load();
  EXPECT_EQ(loaded.get(42)->machine_id(), "machine-2");
}

/* a save replaces the whole cache, it is not appended to the previous one */
TEST_F(otlp_host_metadata_cache, save_replaces_the_previous_cache) {
  auto cache = _open_cache();
  host_metadata_store first(cache);
  first.set(make_info(1));
  first.save();

  host_metadata_store second(cache);
  AgentHostInfo other = make_info(1);
  other.set_host_id(43);
  second.set(other);
  second.save();

  host_metadata_store loaded(_open_cache());
  EXPECT_EQ(loaded.load(), 1u);
  EXPECT_FALSE(loaded.get(42));
  EXPECT_TRUE(loaded.get(43));
}
