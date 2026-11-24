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
#include "com/centreon/broker/sql/database_config.hh"

#include <gtest/gtest.h>

#include "com/centreon/broker/config/endpoint.hh"

using namespace com::centreon::broker;

namespace {
config::endpoint db_endpoint() {
  config::endpoint retval(config::endpoint::output);
  retval.name = "db";
  retval.params["db_type"] = "mysql";
  retval.params["db_host"] = "127.0.0.1";
  retval.params["db_name"] = "centreon_storage";
  return retval;
}
}  // namespace

/* The timeouts must stay above innodb_lock_wait_timeout (50s by default). */
TEST(DatabaseConfig, TimeoutsDefault) {
  database_config cfg(db_endpoint(), {});
  ASSERT_EQ(cfg.get_read_timeout(), 60u);
  ASSERT_EQ(cfg.get_write_timeout(), 60u);
}

TEST(DatabaseConfig, TimeoutsSet) {
  config::endpoint ep = db_endpoint();
  ep.params["db_read_timeout"] = "5";
  ep.params["db_write_timeout"] = "7";
  database_config cfg(ep, {});
  ASSERT_EQ(cfg.get_read_timeout(), 5u);
  ASSERT_EQ(cfg.get_write_timeout(), 7u);
}

TEST(DatabaseConfig, TimeoutsBadValue) {
  config::endpoint ep = db_endpoint();
  ep.params["db_read_timeout"] = "foo";
  ep.params["db_write_timeout"] = "-3";
  database_config cfg(ep, {});
  ASSERT_EQ(cfg.get_read_timeout(), 60u);
  ASSERT_EQ(cfg.get_write_timeout(), 60u);
}

/* A configuration change of a timeout must not be ignored. */
TEST(DatabaseConfig, TimeoutsCopiedAndCompared) {
  config::endpoint ep = db_endpoint();
  ep.params["db_read_timeout"] = "5";
  database_config cfg1(ep, {});
  database_config cfg2(cfg1);
  ASSERT_EQ(cfg2.get_read_timeout(), 5u);
  ASSERT_EQ(cfg1, cfg2);
  cfg2.set_write_timeout(30);
  ASSERT_NE(cfg1, cfg2);
}
