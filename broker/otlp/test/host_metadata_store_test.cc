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

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

namespace {

AgentHostInfo make_info(uint64_t poller_id,
                        uint64_t observed_at,
                        const std::string& machine_id = "machine-1") {
  AgentHostInfo info;
  info.set_poller_id(poller_id);
  info.set_host_id(42);
  info.set_host_name("srv-web-01");
  info.set_observed_at(observed_at);
  info.set_os_type("linux");
  info.set_os_name("AlmaLinux");
  info.set_os_version("9.4");
  info.set_arch("amd64");
  info.set_machine_id(machine_id);
  info.add_ips("10.0.0.1");
  return info;
}

using result = host_metadata_store::update_result;

}  // namespace

TEST(otlp_host_metadata_store, unknown_host) {
  host_metadata_store store(std::chrono::seconds(900));
  EXPECT_FALSE(store.get(42, 1000));
}

TEST(otlp_host_metadata_store, first_event_is_stored) {
  host_metadata_store store(std::chrono::seconds(900));
  EXPECT_EQ(store.update(make_info(1, 100), 1000), result::updated);
  auto meta = store.get(42, 1000);
  ASSERT_TRUE(meta);
  EXPECT_EQ(meta->poller_id, 1u);
  EXPECT_EQ(meta->os_type, "linux");
  EXPECT_EQ(meta->os_name, "AlmaLinux");
  EXPECT_EQ(meta->os_version, "9.4");
  EXPECT_EQ(meta->arch, "amd64");
  EXPECT_EQ(meta->machine_id, "machine-1");
  EXPECT_EQ(meta->ips, std::vector<std::string>{"10.0.0.1"});
}

TEST(otlp_host_metadata_store, same_information_is_a_refresh) {
  host_metadata_store store(std::chrono::seconds(900));
  store.update(make_info(1, 100), 1000);
  EXPECT_EQ(store.update(make_info(1, 400), 1300), result::refreshed);
  /* the refresh postpones the expiration */
  EXPECT_TRUE(store.get(42, 2100));
}

/* each event replaces the whole record, a field absent from the new event
 * is not kept from the old one */
TEST(otlp_host_metadata_store, event_replaces_the_whole_record) {
  host_metadata_store store(std::chrono::seconds(900));
  store.update(make_info(1, 100), 1000);
  AgentHostInfo info = make_info(1, 200);
  info.clear_ips();
  info.clear_os_name();
  EXPECT_EQ(store.update(info, 1100), result::updated);
  auto meta = store.get(42, 1100);
  ASSERT_TRUE(meta);
  EXPECT_TRUE(meta->ips.empty());
  EXPECT_TRUE(meta->os_name.empty());
}

TEST(otlp_host_metadata_store, machine_change_is_an_identity_change) {
  host_metadata_store store(std::chrono::seconds(900));
  store.update(make_info(1, 100), 1000);
  EXPECT_EQ(store.update(make_info(1, 200, "machine-2"), 1100),
            result::identity_changed);
  EXPECT_EQ(store.get(42, 1100)->machine_id, "machine-2");
}

/* host moved from poller 1 to poller 2: a late event of poller 1 must not
 * overwrite the information of poller 2 */
TEST(otlp_host_metadata_store, outdated_event_is_ignored) {
  host_metadata_store store(std::chrono::seconds(900));
  store.update(make_info(2, 200, "machine-2"), 1000);
  EXPECT_EQ(store.update(make_info(1, 100, "machine-1"), 1001),
            result::ignored);
  auto meta = store.get(42, 1001);
  ASSERT_TRUE(meta);
  EXPECT_EQ(meta->poller_id, 2u);
  EXPECT_EQ(meta->machine_id, "machine-2");
}

/* a deleted host, or a disconnected agent, is no longer refreshed by engine */
TEST(otlp_host_metadata_store, not_refreshed_record_expires) {
  host_metadata_store store(std::chrono::seconds(900));
  store.update(make_info(1, 100), 1000);
  EXPECT_TRUE(store.get(42, 1900));
  EXPECT_FALSE(store.get(42, 1901));
  EXPECT_EQ(store.size(), 0u);
}

/* expiration uses broker clock: an event with an older engine clock is
 * accepted once the stored record has expired */
TEST(otlp_host_metadata_store, expired_record_is_replaced) {
  host_metadata_store store(std::chrono::seconds(900));
  store.update(make_info(2, 5000), 1000);
  EXPECT_EQ(store.update(make_info(1, 100), 2000), result::updated);
  EXPECT_EQ(store.get(42, 2000)->poller_id, 1u);
}
