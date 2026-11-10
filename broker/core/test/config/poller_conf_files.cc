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

using com::centreon::broker::config::applier::poller_conf_files;

/**
 * Scenario: the files of a poller are named in one place.
 * Given poller 12 in the directory /d
 * Then its acknowledged configuration is /d/12.prot
 * And the one prepared for it is /d/new-12.prot
 * And the difference between them is /d/diff-12.prot.
 */
TEST(PollerConfFiles, NamesTheThreeFiles) {
  const std::filesystem::path dir{"/d"};
  const poller_conf_files files{dir, 12};
  EXPECT_EQ(files.stored_path(), std::filesystem::path{"/d/12.prot"});
  EXPECT_EQ(files.new_path(), std::filesystem::path{"/d/new-12.prot"});
  EXPECT_EQ(files.diff_path(), std::filesystem::path{"/d/diff-12.prot"});
}

/**
 * Scenario: a file name tells which poller it belongs to, and what it is.
 * Given the three files of poller 12
 * Then each one is recognized by its own parser only
 * And the parsers give back 12.
 */
TEST(PollerConfFiles, EachFileHasItsOwnParser) {
  const std::filesystem::path dir{"/d"};
  const poller_conf_files files{dir, 12};
  EXPECT_EQ(poller_conf_files::stored_id(files.stored_path()), 12u);
  EXPECT_EQ(poller_conf_files::stored_id(files.new_path()), 0u);
  EXPECT_EQ(poller_conf_files::stored_id(files.diff_path()), 0u);

  EXPECT_EQ(poller_conf_files::new_id(files.stored_path()), 0u);
  EXPECT_EQ(poller_conf_files::new_id(files.new_path()), 12u);
  EXPECT_EQ(poller_conf_files::new_id(files.diff_path()), 0u);

  EXPECT_EQ(poller_conf_files::diff_id(files.stored_path()), 0u);
  EXPECT_EQ(poller_conf_files::diff_id(files.new_path()), 0u);
  EXPECT_EQ(poller_conf_files::diff_id(files.diff_path()), 12u);
}

/**
 * Scenario: the other files of the directory belong to no poller.
 * Given files that are not poller configurations
 * Then no parser gives them a poller ID.
 */
TEST(PollerConfFiles, OtherFilesBelongToNoPoller) {
  for (const char* name :
       {"/d/topology.cache", "/d/12.lck", "/d/abc.prot", "/d/new-abc.prot",
        "/d/diff-.prot", "/d/old-12.prot", "/d/12.prot.tmp"}) {
    EXPECT_EQ(poller_conf_files::stored_id(name), 0u) << name;
    EXPECT_EQ(poller_conf_files::new_id(name), 0u) << name;
    EXPECT_EQ(poller_conf_files::diff_id(name), 0u) << name;
  }
}
