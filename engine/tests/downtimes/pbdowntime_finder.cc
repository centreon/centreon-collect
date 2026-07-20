/**
 * Copyright 2019-2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/downtimes/downtime_finder.hh"

#include "com/centreon/clib.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "helper.hh"
#include "test_engine.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

class DowntimeFinderFindMatchingAllTest : public TestEngine {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    configuration::error_cnt err;
    _state_hlp = init_config_state();
    configuration::Contact ctc{
        new_pb_configuration_contact("admin", false, "a")};
    configuration::applier::contact ctc_aply;
    ctc_aply.add_object(ctc);

    configuration::Host hst{new_pb_configuration_host("test_host", "admin", 1)};
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::Host hst1{
        new_pb_configuration_host("first_host", "admin", 12)};
    hst_aply.add_object(hst1);

    configuration::Host hst2{
        new_pb_configuration_host("other_host", "admin", 2)};
    hst_aply.add_object(hst2);

    hst_aply.resolve_object(hst, err);
    hst_aply.resolve_object(hst1, err);

    configuration::Service svc{
        new_pb_configuration_service("first_host", "test_service", "admin", 8)};
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    configuration::Service svc1{
        new_pb_configuration_service("first_host", "other_svc", "admin", 9)};
    svc_aply.add_object(svc1);

    configuration::Service svc2{
        new_pb_configuration_service("test_host", "new_svc", "admin", 10)};
    svc_aply.add_object(svc2);

    configuration::Service svc3{
        new_pb_configuration_service("test_host", "new_svc1", "admin", 11)};
    svc_aply.add_object(svc3);

    configuration::Service svc4{
        new_pb_configuration_service("test_host", "new_svc2", "admin", 12)};
    svc_aply.add_object(svc4);

    svc_aply.resolve_object(svc, err);
    svc_aply.resolve_object(svc1, err);
    svc_aply.resolve_object(svc2, err);
    svc_aply.resolve_object(svc3, err);
    svc_aply.resolve_object(svc4, err);

    downtime_manager::instance().clear_scheduled_downtimes();
    downtime_manager::instance().initialize_downtime_data();
    new_downtime(1, 1, 10, 234567891, 734567892, 1, 0, 84, "other_author",
                 "test_comment");
    // OK
    new_downtime(2, 12, 8, 123456789, 134567892, 1, 0, 42, "test_author",
                 "other_comment");
    // OK
    new_downtime(3, 12, 9, 123456789, 345678921, 0, 1, 42, "", "test_comment");
    new_downtime(4, 1, 10, 123456789, 345678921, 0, 1, 84, "test_author", "");
    // OK
    new_downtime(5, 1, 11, 123456789, 134567892, 1, 1, 42, "test_author",
                 "test_comment");
    // OK
    new_downtime(6, 1, 12, 7265943625, 7297479625, 1, 2, 31626500, "out_author",
                 "out_comment");
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
                    const uint64_t host_id,
                    const uint64_t service_id,
                    time_t start,
                    time_t end,
                    int fixed,
                    unsigned long triggered_by,
                    int32_t duration,
                    std::string const& author,
                    std::string const& comment) {
    downtime_manager::instance().schedule_downtime(
        downtime::service_downtime, host_id, service_id, start, author.c_str(),
        comment.c_str(), start, end, fixed, triggered_by, duration,
        &downtime_id);
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
// When find_matching_all() is called with the criteria ("author",
// "anyauthor")
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
  expected.push_back(3);
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
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("host", "test_host")
// Then all downtimes of host /test_host/ are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleHosts) {
  criterias.push_back(downtime_finder::criteria("host", "test_host"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(4);
  expected.push_back(5);
  expected.push_back(1);
  expected.push_back(6);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("service",
// "test_service") Then all downtimes of service /test_service/ are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleServices) {
  criterias.push_back(downtime_finder::criteria("service", "test_service"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("start", "123456789")
// Then all downtimes with 123456789 as start time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleStart) {
  criterias.push_back(downtime_finder::criteria("start", "123456789"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(3);
  expected.push_back(4);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("end", "134567892")
// Then all downtimes with 134567892 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleEnd) {
  criterias.push_back(downtime_finder::criteria("end", "134567892"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("fixed", "0")
// Then all downtimes that are not fixed are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleFixed) {
  criterias.push_back(downtime_finder::criteria("fixed", "0"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(3);
  expected.push_back(4);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("triggered_by", "0")
// Then all downtimes that are not triggered by other downtimes are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleTriggeredBy) {
  criterias.push_back(downtime_finder::criteria("triggered_by", "0"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(1);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("duration", "42")
// Then all downtimes with a duration of 42 seconds are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleDuration) {
  criterias.push_back(downtime_finder::criteria("duration", "42"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(3);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("author",
// "test_author") Then all downtimes from author /test_author/ are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleAuthor) {
  criterias.push_back(downtime_finder::criteria("author", "test_author"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(2);
  expected.push_back(4);
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("comment",
// "test_comment") Then all downtimes with comment "test_comment" are returned
TEST_F(DowntimeFinderFindMatchingAllTest, MultipleComment) {
  criterias.push_back(downtime_finder::criteria("comment", "test_comment"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(3);
  expected.push_back(5);
  expected.push_back(1);
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
  expected.push_back(5);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("end", "4102441200")
// Then all downtimes with 4102441200 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, OutOfRangeEnd) {
  criterias.push_back(downtime_finder::criteria("end", "4102441200"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(6);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("start",
// "4102441200")
// Then all downtimes with 4102441200 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, OutOfRangeStart) {
  criterias.push_back(downtime_finder::criteria("start", "4102441200"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(6);
  ASSERT_EQ(result, expected);
}

// Given a downtime_finder object with the test downtime list
// When find_matching_all() is called with the criteria ("duration",
// "4102441200") Then all downtimes with 31622400 as end time are returned
TEST_F(DowntimeFinderFindMatchingAllTest, OutOfRangeDuration) {
  criterias.push_back(downtime_finder::criteria("duration", "31622400"));
  result = _dtf->find_matching_all(criterias);
  expected.push_back(6);
  ASSERT_EQ(result, expected);
}
