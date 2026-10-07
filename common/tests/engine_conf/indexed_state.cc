/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/indexed_state.hh"
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <atomic>
#include <sstream>
#include <thread>
#include "common/engine_conf/state.pb.h"

using namespace com::centreon::engine;
using google::protobuf::util::MessageDifferencer;

namespace {
constexpr uint32_t hosts_count = 50;
constexpr uint32_t services_per_host = 20;
constexpr uint32_t poller_id = 7;

/**
 * @brief Build a State with objects in several indexed containers and some
 * fields that stay in the State itself.
 */
std::unique_ptr<configuration::State> make_state() {
  auto state = std::make_unique<configuration::State>();
  state->set_poller_id(poller_id);
  state->set_interval_length(60);
  state->add_cfg_file("/etc/centreon-engine/hosts.cfg");
  auto* tp = state->add_timeperiods();
  tp->set_timeperiod_name("24x7");
  auto* cmd = state->add_commands();
  cmd->set_command_name("check_ping");
  cmd->set_command_line("/usr/lib/nagios/plugins/check_ping");
  auto* contact = state->add_contacts();
  contact->set_contact_name("admin");
  for (uint32_t h = 1; h <= hosts_count; h++) {
    auto* host = state->add_hosts();
    host->set_host_id(h);
    host->set_host_name("host_" + std::to_string(h));
    for (uint32_t s = 1; s <= services_per_host; s++) {
      auto* svc = state->add_services();
      svc->set_host_id(h);
      svc->set_service_id((h - 1) * services_per_host + s);
      svc->set_service_description("service_" + std::to_string(s));
    }
  }
  return state;
}

/**
 * @brief Compare two States whatever the order of the entries of their
 * repeated fields: the indexes are hash maps.
 *
 * @return The differences, empty if the States are the same.
 */
std::string state_differences(const configuration::State& a,
                              const configuration::State& b) {
  std::string retval;
  MessageDifferencer differencer;
  differencer.set_repeated_field_comparison(MessageDifferencer::AS_SET);
  differencer.ReportDifferencesToString(&retval);
  /* The report also lists the entries found at another position, which are
   * not differences for a comparison as sets. */
  if (differencer.Compare(a, b))
    retval.clear();
  return retval;
}
}  // namespace

/* What is written must parse back to the State the indexed_state was built
 * from, indexed objects included. */
TEST(IndexedState, SerializeGivesBackTheWholeState) {
  auto state = make_state();
  configuration::State expected(*state);
  configuration::indexed_state indexed(std::move(state));

  std::ostringstream oss;
  indexed.serialize_to_ostream(&oss);

  configuration::State parsed;
  ASSERT_TRUE(parsed.ParseFromString(oss.str()));
  ASSERT_EQ(parsed.hosts_size(), hosts_count);
  ASSERT_EQ(parsed.services_size(), hosts_count * services_per_host);
  ASSERT_EQ(state_differences(parsed, expected), "");
}

/* Serializing must not touch the indexed_state: same objects, same addresses,
 * and the State still has its own fields. */
TEST(IndexedState, SerializeLeavesTheIndexesUntouched) {
  configuration::indexed_state indexed(make_state());
  const configuration::Host* host_1 = indexed.hosts().at(1).get();

  std::ostringstream oss;
  indexed.serialize_to_ostream(&oss);

  ASSERT_EQ(indexed.state().poller_id(), poller_id);
  ASSERT_EQ(indexed.state().hosts_size(), 0);
  ASSERT_EQ(indexed.hosts().size(), hosts_count);
  ASSERT_EQ(indexed.services().size(), hosts_count * services_per_host);
  ASSERT_EQ(indexed.hosts().at(1).get(), host_1);
}

/* Engine writes state.prot while other threads read the configuration (the
 * broker log sink reads it on every log line). The serialization used to
 * empty the indexed_state for the time of the write, so a reader saw a null
 * State: this test crashed or counted errors with it. */
TEST(IndexedState, SerializeKeepsTheStateReadableByOtherThreads) {
  configuration::indexed_state indexed(make_state());
  std::atomic_bool done{false};
  std::atomic_uint32_t errors{0};
  std::atomic_uint32_t reads{0};

  std::thread reader([&] {
    while (!done) {
      if (indexed.state().poller_id() != poller_id ||
          indexed.hosts().size() != hosts_count)
        ++errors;
      ++reads;
    }
  });

  for (int i = 0; i < 200; i++) {
    std::ostringstream oss;
    indexed.serialize_to_ostream(&oss);
  }
  done = true;
  reader.join();

  ASSERT_GT(reads.load(), 0u);
  ASSERT_EQ(errors.load(), 0u);
}
