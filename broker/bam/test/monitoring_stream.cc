/**
 * Copyright 2014, 2022-2024 Centreon
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

#include "com/centreon/broker/bam/monitoring_stream.hh"

#include <gtest/gtest.h>

#include "bbdo/bam/ba_status.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "common/log_v2/log_v2.hh"

using log_v2 = com::centreon::common::log_v2::log_v2;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

class BamMonitoringStream : public testing::Test {
  void SetUp() override {
    config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
  }
  void TearDown() override { config::applier::deinit(); }
};

TEST_F(BamMonitoringStream, WriteKpi) {
  database_config cfg("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                      "centreon");
  database_config storage("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                          "centreon_storage");

  std::shared_ptr<persistent_cache> cache;
  std::unique_ptr<monitoring_stream> ms;

  ASSERT_NO_THROW(ms.reset(new monitoring_stream(
      "", cfg, storage, cache, log_v2::instance().get(log_v2::BAM))));

  std::shared_ptr<pb_kpi_status> st{std::make_shared<pb_kpi_status>()};
  st->mut_obj().set_kpi_id(1);

  ms->write(std::static_pointer_cast<io::data>(st));
}

TEST_F(BamMonitoringStream, WriteBA) {
  database_config cfg("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                      "centreon");
  database_config storage("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                          "centreon_storage");
  ;
  std::shared_ptr<persistent_cache> cache;
  std::unique_ptr<monitoring_stream> ms;

  ASSERT_NO_THROW(ms.reset(new monitoring_stream(
      "", cfg, storage, cache, log_v2::instance().get(log_v2::BAM))));

  std::shared_ptr<ba_status> st{std::make_shared<ba_status>(ba_status())};

  ms->write(std::static_pointer_cast<io::data>(st));
}

TEST_F(BamMonitoringStream, WorkWithNoPendigMysqlRequest) {
  database_config cfg("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                      "centreon", 0);
  database_config storage("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                          "centreon_storage", 0);
  ;
  std::shared_ptr<persistent_cache> cache;
  std::unique_ptr<monitoring_stream> ms;

  ASSERT_NO_THROW(ms.reset(new monitoring_stream(
      "", cfg, storage, cache, log_v2::instance().get(log_v2::BAM))));

  std::shared_ptr<ba_status> st{std::make_shared<ba_status>(ba_status())};

  ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(st)), 1);

  std::shared_ptr<neb::acknowledgement> dt{
      std::make_shared<neb::acknowledgement>()};

  ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(dt)), 1);
}

TEST_F(BamMonitoringStream, WorkWithPendigMysqlRequest) {
  database_config cfg("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                      "centreon", 5);
  database_config storage("MySQL", "127.0.0.1", "", 3306, "root", "centreon",
                          "centreon_storage", 5);
  ;
  std::shared_ptr<persistent_cache> cache;
  std::unique_ptr<monitoring_stream> ms;

  ASSERT_NO_THROW(ms.reset(new monitoring_stream(
      "", cfg, storage, cache, log_v2::instance().get(log_v2::BAM))));

  std::shared_ptr<ba_status> st{std::make_shared<ba_status>(ba_status())};

  for (unsigned ii = 0; ii < 4; ++ii) {
    ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(st)), 0);
  }
  ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(st)), 5);

  std::shared_ptr<neb::acknowledgement> dt{
      std::make_shared<neb::acknowledgement>()};

  ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(st)), 0);
  for (unsigned ii = 0; ii < 48; ++ii) {
    std::cout << ii << std::endl;
    ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(dt)), 0);
  }
  ASSERT_EQ(ms->write(std::static_pointer_cast<io::data>(dt)), 50);
}
