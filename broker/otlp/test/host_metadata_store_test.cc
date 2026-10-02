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
  EXPECT_EQ(meta->os_type, "linux");
  EXPECT_EQ(meta->os_name, "AlmaLinux");
  EXPECT_EQ(meta->os_version, "9.4");
  EXPECT_EQ(meta->arch, "amd64");
  EXPECT_EQ(meta->machine_id, "machine-1");
  EXPECT_EQ(meta->ips, std::vector<std::string>{"10.0.0.1"});
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
  EXPECT_TRUE(meta->ips.empty());
  EXPECT_TRUE(meta->os_name.empty());
  EXPECT_EQ(store.size(), 1u);
}

/* no ordering between events: the last received one wins, whatever its
 * poller */
TEST(otlp_host_metadata_store, last_event_wins) {
  host_metadata_store store;
  store.set(make_info(2, "machine-2"));
  store.set(make_info(1, "machine-1"));
  EXPECT_EQ(store.get(42)->machine_id, "machine-1");
}
