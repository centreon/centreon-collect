/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/container/flat_set.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/victoria_metrics/stream.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::common;
using namespace com::centreon::broker::victoria_metrics;
using namespace nlohmann;

extern std::shared_ptr<asio::io_context> g_io_context;

class victoria_stream_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    config::applier::state::load(com::centreon::common::BROKER);
    file::disk_accessor::load(1000);
  }
  static void TearDownTestSuite() {}
};

TEST_F(victoria_stream_test, Authorization) {
  http::http_config dummy;
  std::vector<http_tsdb::column> dummy2;
  auto cfg = std::make_shared<http_tsdb::http_tsdb_config>(
      dummy, "/write", "Aladdin", "open sesame", 1, dummy2, dummy2);

  std::shared_ptr<stream> s =
      stream::load(g_io_context, cfg, "my_account", [cfg]() {
        return http::http_connection::load(
            com::centreon::common::pool::io_context_ptr(),
            log_v2::log_v2::instance().get(log_v2::log_v2::VICTORIA_METRICS),
            cfg);
      });
  ASSERT_EQ(s->get_authorization(), "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
}
