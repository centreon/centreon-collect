/*
** Copyright 2016 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/downtimes/downtime_finder.hh"
#include <gtest/gtest.h>

#include "com/centreon/clib.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

class DowntimeFinderFindMatchingAllTest : public ::testing::Test {
 public:
  void SetUp() override {
    init_config_state();
    downtime_manager::instance().clear_scheduled_downtimes();
    downtime_manager::instance().initialize_downtime_data();
    new_downtime(0, "test_host", "", 234567891, 134567892, 1, 0, 84,
                 "other_author", "test_comment");
    new_downtime(1, "first_host", "test_service", 123456789, 134567892, 1, 0,
                 42, "test_author", "other_comment");
    new_downtime(2, "other_host", "other_service", 123456789, 345678921, 0, 1,
                 42, "", "test_comment");
    new_downtime(3, "test_host", "test_service", 234567891, 345678921, 0, 1, 84,
                 "test_author", "");
    new_downtime(4, "other_host", "test_service", 123456789, 134567892, 1, 1,
                 42, "test_author", "test_comment");
    new_downtime(5, "out_host", "out_service", 7265943625, 7297479625, 1, 2,
                 31626500, "out_author", "out_comment");
    _dtf = std::make_unique<downtime_finder>(
        downtime_manager::instance().get_scheduled_downtimes());
  }

  void TearDown() override {
    _dtf.reset();
    downtime_manager::instance().clear_scheduled_downtimes();
    downtime_manager::instance().initialize_downtime_data();
    deinit_config_state();
  }

  void new_downtime(uint64_t downtime_id,
                    std::string const& host_name,
                    std::string const& service_description,
                    time_t start,
                    time_t end,
                    int fixed,
                    unsigned long triggered_by,
                    int32_t duration,
                    std::string const& author,
                    std::string const& comment) {
    downtime_manager::instance().schedule_downtime(
        downtime::service_downtime, host_name, service_description, start,
        author.c_str(), comment.c_str(), start, end, fixed, triggered_by,
        duration, &downtime_id);
  }

 protected:
  std::unique_ptr<downtime_finder> _dtf;
  downtime* dtl;
  downtime_finder::criteria_set criterias;
  downtime_finder::result_set result;
  downtime_finder::result_set expected;
};

// Given a downtime_finder object with a NULL downtime list
// When find_matching_all() is called
// Then an empty result_set is returned
TEST_F(DowntimeFinderFindMatchingAllTest, NullDowntimeList) {
  std::multimap<time_t, std::shared_ptr<downtime>> map;
  downtime_finder local_dtf(map);
  criterias.push_back(downtime_finder::criteria("host", "test_host"));
  result = local_dtf.find_matching_all(criterias);
  ASSERT_TRUE(result.empty());
}

// Given a downtime_finder object with the test downtime list
// And a downtime of the test list has a null host_name
// When find_matching_all() is called with criteria ("host", "anyhost")
// Then an empty result_set is returned
TEST_F(DowntimeFinderFindMatchingAllTest, NullHostNotFound) {
  criterias.push_back(downtime_finder::criteria("host", "anyhost"));
  result = _dtf->find_matching_all(criterias);
  ASSERT_TRUE(result.empty());
}

// Given a downtime finder object with the test downtime list
// And a downtime of the test list has a null service_description
// When find_matching_all() is called with criteria ("service", "anyservice")
// Then an empty result_set is returned
TEST_F(DowntimeFinderFindMatchingAllTest, NullServiceNotFound) {
  criterias.push_back(downtime_finder::criteria("service", "anyservice"));
  result = _dtf->find_matching_all(criterias);
  ASSERT_TRUE(result.empty());
}

// Given a downtime finder object with the test downtime list
// And a downtime the test list has a null service_description
// When find_matching_all() is called with the criteria ("service", "")
// Then the result_set contains the downtime
TEST_F(DowntimeFinderFindMatchingAllTest, NullServiceFound) {
  criterias.push_back(downtime_finder::criteria("service", ""));
  result = _dtf->find_matching_all(criterias);
  ASSERT_TRUE(result.empty());
}

// Given a downtime_finder object with the test downtime list
// And a downtime of the test list has a null author
// When find_matching_all() is called with the criteria ("author", "anyauthor")
// Then an empty result_set is returned
TEST_F(DowntimeFinderFindMatchingAllTest, NullAuthorNotFound) {
  criterias.push_back(downtime_finder::criteria("author", "anyauthor"));
  result = _dtf->find_matching_all(criterias);
  ASSERT_TRUE(result.empty());
}

// Given a downtime_finder object with the test downtime list
// And a downtime of the test list has a null author
// When find_matching_all() is called with the criteria ("author", "")
// Then the result_set contains the downtime
TEST_F(DowntimeFinderFindMatchingAllTest, NullAuthorFound) {
  criterias.push_back(downtime_finder::criteria("author", ""));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// And a downtime of the test list has a null comment
// When find_matching_all() is called with the criteria ("comment",
// "anycomment") Then an empty result_set is returned
TEST_F(DowntimeFinderFindMatchingAllTest, NullCommentNotFound) {
  criterias.push_back(downtime_finder::criteria("comment", "anycomment"));
  result = _dtf->find_matching_all(criterias);
  ASSERT_TRUE(result.empty());
}

// Given a downtime_finder object with the test downtime list
// And a downtime of the test list has a null comment
// When find_matching_all() is called with the criteria ("comment", "")
// Then the result_set contains the downtime
TEST_F(DowntimeFinderFindMatchingAllTest, NullCommentFound) {
  criterias.push_back(downtime_finder::criteria("comment", ""));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(3);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("host", "test_host")
// Then all downtimes of host /test_host/ are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleHosts) {
  criterias.push_back(downtime_finder::criteria("host", "test_host"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(3);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("service",
// "test_service") Then all downtimes of service /test_service/ are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleServices) {
  criterias.push_back(downtime_finder::criteria("service", "test_service"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(1);
  expected.push_back(4);
  expected.push_back(3);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("start", "123456789")
// Then all downtimes with 123456789 as start time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleStart) {
  criterias.push_back(downtime_finder::criteria("start", "123456789"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(1);
  expected.push_back(2);
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("end", "134567892")
// Then all downtimes with 134567892 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleEnd) {
  criterias.push_back(downtime_finder::criteria("end", "134567892"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(1);
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("fixed", "0")
// Then all downtimes that are not fixed are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleFixed) {
  criterias.push_back(downtime_finder::criteria("fixed", "0"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(3);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("triggered_by", "0")
// Then all downtimes that are not triggered by other downtimes are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleTriggeredBy) {
  criterias.push_back(downtime_finder::criteria("triggered_by", "0"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(1);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("duration", "42")
// Then all downtimes with a duration of 42 seconds are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleDuration) {
  criterias.push_back(downtime_finder::criteria("duration", "42"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(1);
  expected.push_back(2);
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("author",
// "test_author") Then all downtimes from author /test_author/ are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleAuthor) {
  criterias.push_back(downtime_finder::criteria("author", "test_author"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(1);
  expected.push_back(4);
  expected.push_back(3);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("comment",
// "test_comment") Then all downtimes with comment "test_comment" are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleComment) {
  criterias.push_back(downtime_finder::criteria("comment", "test_comment"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When findMatchinAll() is called with criterias ("author", "test_author"),
// ("duration", "42") and ("comment", "test_comment") Then all downtimes
// matching the criterias are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleCriterias) {
  criterias.push_back(downtime_finder::criteria("author", "test_author"));
  criterias.push_back(downtime_finder::criteria("duration", "42"));
  criterias.push_back(downtime_finder::criteria("comment", "test_comment"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("end", "4102441200")
// Then all downtimes with 4102441200 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, OutOfRangeEnd) {
  criterias.push_back(downtime_finder::criteria("end", "4102441200"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("start", "4102441200")
// Then all downtimes with 4102441200 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, OutOfRangeStart) {
  criterias.push_back(downtime_finder::criteria("start", "4102441200"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("duration",
// "4102441200") Then all downtimes with 31622400 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, OutOfRangeDuration) {
  criterias.push_back(downtime_finder::criteria("duration", "31622400"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}
