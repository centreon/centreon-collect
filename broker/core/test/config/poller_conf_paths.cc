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

#include "broker/core/config/applier/poller_conf.hh"

using com::centreon::broker::config::applier::poller_conf_paths;

/**
 * Scenario: the files of a poller are named in one place.
 * Given poller 12 in the directory /d
 * Then its acknowledged configuration is /d/12.prot
 * And the one prepared for it is /d/new-12.prot
 * And the difference to send it is /d/new-diff-12.prot
 * And the acknowledged difference waiting for the database is /d/diff-12.prot
 * And the one exported while another is in flight is /d/next-12.prot.
 */
TEST(PollerConfPaths, NamesTheFiveFiles) {
  const std::filesystem::path dir{"/d"};
  const poller_conf_paths paths{dir, 12};
  EXPECT_EQ(paths.stored_path(), std::filesystem::path{"/d/12.prot"});
  EXPECT_EQ(paths.new_path(), std::filesystem::path{"/d/new-12.prot"});
  EXPECT_EQ(paths.new_diff_path(),
            std::filesystem::path{"/d/new-diff-12.prot"});
  EXPECT_EQ(paths.diff_path(), std::filesystem::path{"/d/diff-12.prot"});
  EXPECT_EQ(paths.next_path(), std::filesystem::path{"/d/next-12.prot"});
}

/**
 * Scenario: a file name tells which poller it belongs to, and what it is.
 * Given the five files of poller 12
 * Then each one is recognized by its own parser only
 * And the parsers give back 12.
 */
TEST(PollerConfPaths, EachFileHasItsOwnParser) {
  const std::filesystem::path dir{"/d"};
  const poller_conf_paths paths{dir, 12};
  EXPECT_EQ(poller_conf_paths::stored_id(paths.stored_path()), 12u);
  EXPECT_EQ(poller_conf_paths::stored_id(paths.new_path()), 0u);
  EXPECT_EQ(poller_conf_paths::stored_id(paths.new_diff_path()), 0u);
  EXPECT_EQ(poller_conf_paths::stored_id(paths.diff_path()), 0u);
  EXPECT_EQ(poller_conf_paths::stored_id(paths.next_path()), 0u);

  EXPECT_EQ(poller_conf_paths::new_id(paths.stored_path()), 0u);
  EXPECT_EQ(poller_conf_paths::new_id(paths.new_path()), 12u);
  EXPECT_EQ(poller_conf_paths::new_id(paths.new_diff_path()), 0u);
  EXPECT_EQ(poller_conf_paths::new_id(paths.diff_path()), 0u);

  EXPECT_EQ(poller_conf_paths::new_diff_id(paths.stored_path()), 0u);
  EXPECT_EQ(poller_conf_paths::new_diff_id(paths.new_path()), 0u);
  EXPECT_EQ(poller_conf_paths::new_diff_id(paths.new_diff_path()), 12u);
  EXPECT_EQ(poller_conf_paths::new_diff_id(paths.diff_path()), 0u);

  EXPECT_EQ(poller_conf_paths::diff_id(paths.stored_path()), 0u);
  EXPECT_EQ(poller_conf_paths::diff_id(paths.new_path()), 0u);
  EXPECT_EQ(poller_conf_paths::diff_id(paths.new_diff_path()), 0u);
  EXPECT_EQ(poller_conf_paths::diff_id(paths.diff_path()), 12u);
  EXPECT_EQ(poller_conf_paths::diff_id(paths.next_path()), 0u);

  EXPECT_EQ(poller_conf_paths::next_id(paths.stored_path()), 0u);
  EXPECT_EQ(poller_conf_paths::next_id(paths.new_path()), 0u);
  EXPECT_EQ(poller_conf_paths::next_id(paths.new_diff_path()), 0u);
  EXPECT_EQ(poller_conf_paths::next_id(paths.diff_path()), 0u);
  EXPECT_EQ(poller_conf_paths::next_id(paths.next_path()), 12u);
}

/**
 * Scenario: the other files of the directory belong to no poller.
 * Given files that are not poller configurations
 * Then no parser gives them a poller ID.
 */
TEST(PollerConfPaths, OtherFilesBelongToNoPoller) {
  for (const char* name :
       {"/d/topology.cache", "/d/12.lck", "/d/abc.prot", "/d/new-abc.prot",
        "/d/diff-.prot", "/d/new-diff-.prot", "/d/next-.prot", "/d/old-12.prot",
        "/d/12.prot.tmp"}) {
    EXPECT_EQ(poller_conf_paths::stored_id(name), 0u) << name;
    EXPECT_EQ(poller_conf_paths::new_id(name), 0u) << name;
    EXPECT_EQ(poller_conf_paths::new_diff_id(name), 0u) << name;
    EXPECT_EQ(poller_conf_paths::diff_id(name), 0u) << name;
    EXPECT_EQ(poller_conf_paths::next_id(name), 0u) << name;
  }
}
