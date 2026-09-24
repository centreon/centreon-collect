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

#include "agent_info_refresher.hh"

using namespace com::centreon::agent;

extern std::shared_ptr<asio::io_context> g_io_context;

namespace {
/* collected content and sent messages are shared with the io_context thread */
struct refresher_test_data {
  absl::Mutex protect;
  AgentInfo collected ABSL_GUARDED_BY(protect);
  std::vector<std::shared_ptr<MessageFromAgent>> sent ABSL_GUARDED_BY(protect);
};

agent_info_refresher::pointer create_refresher(
    const std::shared_ptr<refresher_test_data>& data) {
  return agent_info_refresher::load(
      g_io_context, spdlog::default_logger(), std::chrono::milliseconds(20),
      [data](AgentInfo* to_fill) {
        absl::MutexLock l(data->protect);
        *to_fill = data->collected;
      },
      [data](const std::shared_ptr<MessageFromAgent>& msg) {
        absl::MutexLock l(data->protect);
        data->sent.push_back(msg);
      });
}

AgentInfo make_info(std::initializer_list<std::string> ips) {
  AgentInfo ret;
  ret.set_host("host1");
  ret.set_os_type("linux");
  for (const std::string& ip : ips)
    ret.add_ips(ip);
  return ret;
}
}  // namespace

TEST(agent_info_refresher, nothing_sent_before_connection) {
  auto data = std::make_shared<refresher_test_data>();
  {
    absl::MutexLock l(data->protect);
    data->collected = make_info({"10.0.0.1"});
  }
  auto refresher = create_refresher(data);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  refresher->stop();
  absl::MutexLock l(data->protect);
  EXPECT_TRUE(data->sent.empty());
}

TEST(agent_info_refresher, nothing_sent_if_unchanged) {
  auto data = std::make_shared<refresher_test_data>();
  AgentInfo info = make_info({"10.0.0.1"});
  {
    absl::MutexLock l(data->protect);
    data->collected = info;
  }
  auto refresher = create_refresher(data);
  refresher->set_last_sent(info);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  refresher->stop();
  absl::MutexLock l(data->protect);
  EXPECT_TRUE(data->sent.empty());
}

TEST(agent_info_refresher, update_sent_once_on_change) {
  auto data = std::make_shared<refresher_test_data>();
  auto refresher = create_refresher(data);
  refresher->set_last_sent(make_info({"10.0.0.1"}));
  {
    absl::MutexLock l(data->protect);
    data->collected = make_info({"10.0.0.1", "10.0.0.2"});
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  refresher->stop();
  absl::MutexLock l(data->protect);
  ASSERT_EQ(data->sent.size(), 1u);
  ASSERT_TRUE(data->sent[0]->has_info_update());
  EXPECT_EQ(data->sent[0]->info_update().ips_size(), 2);
}

TEST(agent_info_refresher, nothing_sent_after_stop) {
  auto data = std::make_shared<refresher_test_data>();
  auto refresher = create_refresher(data);
  refresher->set_last_sent(make_info({"10.0.0.1"}));
  refresher->stop();
  {
    absl::MutexLock l(data->protect);
    data->collected = make_info({"10.0.0.2"});
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  absl::MutexLock l(data->protect);
  EXPECT_TRUE(data->sent.empty());
}
