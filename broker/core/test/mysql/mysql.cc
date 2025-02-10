/**
 * Copyright 2011 - 2022-2023 Centreon (https://www.centreon.com/)
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

#include <absl/strings/str_split.h>
#include <gtest/gtest.h>

#include <cmath>
#include <future>

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_check.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/host_group_member.hh"
#include "com/centreon/broker/neb/host_parent.hh"
#include "com/centreon/broker/neb/host_status.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/instance_status.hh"
#include "com/centreon/broker/neb/log_entry.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_check.hh"
#include "com/centreon/broker/neb/service_group.hh"
#include "com/centreon/broker/neb/service_group_member.hh"
#include "com/centreon/broker/sql/mysql_multi_insert.hh"
#include "com/centreon/broker/sql/query_preparator.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using log_v2 = com::centreon::common::log_v2::log_v2;

class DatabaseStorageTest : public ::testing::Test {
 public:
  void SetUp() override {
    try {
      config::applier::init(0, "test_broker", 0);
    } catch (std::exception const& e) {
      (void)e;
    }
  }
  void TearDown() override { config::applier::deinit(); }
};

// When there is no database
// Then the mysql creation throws an exception
TEST_F(DatabaseStorageTest, NoDatabase) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 9876, "root",
                         "centreon", "centreon_storage");
  std::unique_ptr<mysql> ms;
  ASSERT_THROW(ms.reset(new mysql(db_cfg, log_v2::instance().get(log_v2::SQL))),
               msg_fmt);
}

// When there is a database
// And when the connection is well done
// Then no exception is thrown and the mysql object is well built.
TEST_F(DatabaseStorageTest, ConnectionOk) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage");
  std::unique_ptr<mysql> ms;
  ASSERT_NO_THROW(ms = std::make_unique<mysql>(
                      db_cfg, log_v2::instance().get(log_v2::SQL)));
}

//// Given a mysql object
//// When an insert is done in database
//// Then nothing is inserted before the commit.
//// When the commit is done
//// Then the insert is available in the database.
// TEST_F(DatabaseStorageTest, SendDataBin) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   std::ostringstream oss;
//   int now(time(nullptr));
//   oss << "INSERT INTO data_bin (id_metric, ctime, status, value) VALUES "
//       << "(1, " << now << ", '0', 2.5)";
//   int thread_id(ms->run_query(oss.str(), "PROBLEME", true));
//   oss.str("");
//   oss << "SELECT id_metric, status FROM data_bin WHERE ctime=" << now;
//   std::promise<mysql_result> promise;
//   std::future<database::mysql_result> future = promise.get_future();
//
//   ms->run_query_and_get_result(oss.str(), std::move(promise), thread_id);
//
//   // The query is done from the same thread/connection
//   mysql_result res(future.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_FALSE(ms->fetch_row(res));
//   ASSERT_NO_THROW(ms->commit(thread_id));
//
//   std::promise<mysql_result> promise2;
//   std::future<database::mysql_result> future2 = promise2.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise2), thread_id);
//   mysql_result res1(future2.get());
//   ASSERT_TRUE(ms->fetch_row(res1));
// }
//
//// Given a mysql object
//// When a prepare statement is done
//// Then we can bind values to it and execute the statement.
//// Then a commit makes data available in the database.
// TEST_F(DatabaseStorageTest, PrepareQuery) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::ostringstream oss;
//   oss << "INSERT INTO "
//       << "metrics"
//       << "  (index_id, metric_name, unit_name, warn, warn_low,"
//          "   warn_threshold_mode, crit, crit_low, "
//          "   crit_threshold_mode, min, max, current_value,"
//          "   data_source_type)"
//          " VALUES (?, ?, ?, ?, "
//          "         ?, ?, ?, "
//          "         ?, ?, ?, ?, "
//          "         ?, ?)";
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   std::ostringstream nss;
//   nss << "metric_name - " << time(nullptr);
//   mysql_stmt stmt(ms->prepare_query(oss.str()));
//   stmt.bind_value_as_i32(0, 19);
//   stmt.bind_value_as_str(1, nss.str());
//   stmt.bind_value_as_str(2, "test/s");
//   stmt.bind_value_as_f32(3, NAN);
//   stmt.bind_value_as_f32(4, INFINITY);
//   stmt.bind_value_as_tiny(5, true);
//   stmt.bind_value_as_f32(6, 10.0);
//   stmt.bind_value_as_f32(7, 20.0);
//   stmt.bind_value_as_tiny(8, false);
//   stmt.bind_value_as_f32(9, 0.0);
//   stmt.bind_value_as_f32(10, 50.0);
//   stmt.bind_value_as_f32(11, 18.0);
//   stmt.bind_value_as_str(12, "2");
//   // We force the thread 0
//   ms->run_statement(stmt, "", false, 0);
//   oss.str("");
//   oss << "SELECT metric_name FROM metrics WHERE metric_name='" << nss.str()
//       << "'";
//   std::promise<mysql_result> promise;
//   std::future<database::mysql_result> future = promise.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise));
//   mysql_result res(future.get());
//   ASSERT_FALSE(ms->fetch_row(res));
//   ASSERT_NO_THROW(ms->commit());
//   std::promise<mysql_result> promise2;
//   std::future<database::mysql_result> future2 = promise2.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise2));
//   res = future2.get();
//   ASSERT_TRUE(ms->fetch_row(res));
// }
//
//// Given a mysql object
//// When a prepare statement is done
//// Then we can bind values to it and execute the statement.
//// Then a commit makes data available in the database.
// TEST_F(DatabaseStorageTest, PrepareQueryBadQuery) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::ostringstream oss;
//   oss << "INSERT INTO "
//       << "metrics"
//       << "  (index_id, metric_name, unit_name, warn, warn_low,"
//          "   warn_threshold_mode, crit, crit_low, "
//          "   crit_threshold_mode, min, max, current_value,"
//          "   data_source_type)"
//          " VALUES (?, ?, ?, ?, "
//          "         ?, ?, ?, "
//          "         ?, ?, ?, ?, "
//          "         ?, ?";
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   std::ostringstream nss;
//   nss << "metric_name - " << time(nullptr);
//   mysql_stmt stmt(ms->prepare_query(oss.str()));
//   stmt.bind_value_as_i32(0, 19);
//   stmt.bind_value_as_str(1, nss.str());
//   stmt.bind_value_as_str(2, "test/s");
//   stmt.bind_value_as_f32(3, NAN);
//   stmt.bind_value_as_f32(4, INFINITY);
//   stmt.bind_value_as_tiny(5, true);
//   stmt.bind_value_as_f32(6, 10.0);
//   stmt.bind_value_as_f32(7, 20.0);
//   stmt.bind_value_as_tiny(8, false);
//   stmt.bind_value_as_f32(9, 0.0);
//   stmt.bind_value_as_f32(10, 50.0);
//   stmt.bind_value_as_f32(11, 18.0);
//   stmt.bind_value_as_str(12, "2");
//   // The commit forces threads to empty their tasks stack
//   ms->commit();
//   // We are sure, the error is set.
//   ASSERT_THROW(ms->run_statement(stmt, "", false, 0), std::exception);
// }
//
// TEST_F(DatabaseStorageTest, SelectSync) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::ostringstream oss;
//   oss << "SELECT metric_id, index_id, metric_name FROM metrics LIMIT 10";
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   std::promise<mysql_result> promise;
//   std::future<database::mysql_result> future = promise.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise));
//   mysql_result res(future.get());
//   int count(0);
//   while (ms->fetch_row(res)) {
//     int v(res.value_as_i32(0));
//     std::string s(res.value_as_str(2));
//     ASSERT_GT(v, 0);
//     ASSERT_FALSE(s.empty());
//     std::cout << "metric name " << v << " content: " << s << std::endl;
//     ++count;
//   }
//   ASSERT_TRUE(count > 0 && count <= 10);
// }
//
// TEST_F(DatabaseStorageTest, QuerySyncWithError) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   std::promise<mysql_result> promise;
//   std::future<database::mysql_result> future = promise.get_future();
//   ms->run_query_and_get_result("SELECT foo FROM bar LIMIT 1",
//                                std::move(promise));
//   ASSERT_THROW(future.get(), exceptions::msg);
// }
//
// TEST_F(DatabaseStorageTest, QueryWithError) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   // The following insert fails
//   ms->run_query("INSERT INTO FOO (toto) VALUES (0)", "", true, 1);
//   ms->commit();
//
//   // The following is the same one, executed by the same thread but since the
//   // previous error, an exception should arrive.
//   ASSERT_THROW(ms->run_query("INSERT INTO FOO (toto) VALUES (0)", "", true,
//   1),
//                std::exception);
// }
//
// TEST_F(DatabaseStorageTest, PrepareQuerySync) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::ostringstream oss;
//   oss << "INSERT INTO metrics"
//       << "  (index_id, metric_name, unit_name, warn, warn_low,"
//          "   warn_threshold_mode, crit, crit_low, "
//          "   crit_threshold_mode, min, max, current_value,"
//          "   data_source_type)"
//          " VALUES (?, ?, ?, ?, "
//          "         ?, ?, ?, "
//          "         ?, ?, ?, ?, "
//          "         ?, ?)";
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   std::ostringstream nss;
//   nss << "metric_name - " << time(nullptr) << "bis2";
//   mysql_stmt stmt(ms->prepare_query(oss.str()));
//   stmt.bind_value_as_i32(0, 19);
//   stmt.bind_value_as_str(1, nss.str());
//   stmt.bind_value_as_str(2, "test/s");
//   stmt.bind_value_as_f32(3, 20.0);
//   stmt.bind_value_as_f32(4, 40.0);
//   stmt.bind_value_as_tiny(5, 1);
//   stmt.bind_value_as_f32(6, 10.0);
//   stmt.bind_value_as_f32(7, 20.0);
//   stmt.bind_value_as_tiny(8, 1);
//   stmt.bind_value_as_f32(9, 0.0);
//   stmt.bind_value_as_f32(10, 50.0);
//   stmt.bind_value_as_f32(11, 18.0);
//   stmt.bind_value_as_str(12, "2");
//   // We force the thread 0
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_statement_and_get_int(stmt, std::move(promise),
//                                 mysql_task::LAST_INSERT_ID, 0);
//   int id(future.get());
//   ASSERT_TRUE(id > 0);
//   std::cout << "id = " << id << std::endl;
//   oss.str("");
//   oss << "SELECT metric_id FROM metrics WHERE metric_name='" << nss.str()
//       << "'";
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_FALSE(ms->fetch_row(res));
//   ASSERT_NO_THROW(ms->commit());
//   std::promise<mysql_result> promise_r2;
//   std::future<database::mysql_result> future_r2 = promise_r2.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise_r2));
//   res = future_r2.get();
//   ASSERT_TRUE(ms->fetch_row(res));
//   std::cout << "id1 = " << res.value_as_i32(0) << std::endl;
//   ASSERT_TRUE(res.value_as_i32(0) == id);
// }
//
//// Given a mysql object
//// When a prepare statement is done
//// Then we can bind values to it and execute the statement.
//// Then a commit makes data available in the database.
// TEST_F(DatabaseStorageTest, RepeatPrepareQuery) {
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::ostringstream oss;
//   oss << "UPDATE metrics"
//          " SET unit_name=?, warn=?, warn_low=?, warn_threshold_mode=?,"
//          " crit=?, crit_low=?, crit_threshold_mode=?,"
//          " min=?, max=?, current_value=? "
//          "WHERE metric_id=?";
//
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   mysql_stmt stmt(ms->prepare_query(oss.str()));
//   for (int i(1); i < 4000; ++i) {
//     stmt.bind_value_as_str(0, "test/s");
//     stmt.bind_value_as_f32(1, NAN);
//     stmt.bind_value_as_f32(2, NAN);
//     stmt.bind_value_as_tiny(3, 0);
//     stmt.bind_value_as_f32(4, NAN);
//     stmt.bind_value_as_f32(5, NAN);
//     stmt.bind_value_as_tiny(6, 0);
//     stmt.bind_value_as_f32(7, 10.0);
//     stmt.bind_value_as_f32(8, 20.0);
//     stmt.bind_value_as_f32(9, 18.0);
//     stmt.bind_value_as_i32(10, i);
//
//     ms->run_statement(stmt);
//   }
//   ms->commit();
// }
//
//// Instance (15) statement
// TEST_F(DatabaseStorageTest, InstanceStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("instance_id");
//   query_preparator qp(neb::instance::static_type(), unique);
//   mysql_stmt inst_insupdate(qp.prepare_insert_or_update(*ms));
//   mysql_stmt inst_delete(qp.prepare_delete(*ms));
//
//   neb::instance inst;
//   inst.poller_id = 1;
//   inst.name = "Central";
//   inst.program_start = time(nullptr) - 100;
//   inst.program_end = time(nullptr) - 1;
//   inst.version = "1.8.1";
//
//   inst_insupdate << inst;
//   ms->run_statement(inst_insupdate, "", false, 0);
//
//   // Deletion
//   inst_delete << inst;
//   ms->run_statement(inst_delete, "", false, 0);
//
//   // Insert
//   inst_insupdate << inst;
//   ms->run_statement(inst_insupdate, "", false, 0);
//
//   // Update
//   inst.program_end = time(nullptr);
//   inst_insupdate << inst;
//   ms->run_statement(inst_insupdate, "", false, 0);
//
//   // Second instance
//   inst.poller_id = 2;
//   inst.name = "Central2";
//   inst_insupdate << inst;
//   ms->run_statement(inst_insupdate, "", false, 0);
//
//   ms->commit();
//
//   std::stringstream oss;
//   oss << "SELECT instance_id FROM instances ORDER BY instance_id";
//   std::promise<mysql_result> promise;
//   std::future<mysql_result> future = promise.get_future();
//   ms->run_query_and_get_result(oss.str(), std::move(promise));
//   mysql_result res(future.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_i32(0) == 1);
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_i32(0) == 2);
// }
//
//// Host (12) statement
// TEST_F(DatabaseStorageTest, HostStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   query_preparator qp(neb::host::static_type(), unique);
//   mysql_stmt host_insupdate(qp.prepare_insert_or_update(*ms));
//
//   neb::host h;
//   h.address = "10.0.2.15";
//   h.alias = "central";
//   h.flap_detection_on_down = true;
//   h.flap_detection_on_unreachable = true;
//   h.flap_detection_on_up = true;
//   h.host_name = "central_9";
//   h.notify_on_down = true;
//   h.notify_on_unreachable = true;
//   h.poller_id = 1;
//   h.stalk_on_down = false;
//   h.stalk_on_unreachable = false;
//   h.stalk_on_up = false;
//   h.statusmap_image = "";
//   h.timezone = "Europe/Paris";
//
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_query_and_get_int("DELETE FROM hosts", std::move(promise),
//                             mysql_task::int_type::AFFECTED_ROWS);
//   // We wait for the insertion.
//   future.get();
//
//   // Insert
//   for (int i(0); i < 50; ++i) {
//     h.host_id = 24 + i;
//     host_insupdate << h;
//     ms->run_statement(host_insupdate, "", false, i % 5);
//   }
//
//   // Update
//   for (int i(0); i < 50; ++i) {
//     h.host_id = 24 + i;
//     h.stalk_on_up = true;
//     host_insupdate << h;
//     ms->run_statement(host_insupdate, "", false, i % 5);
//   }
//
//   ms->commit();
//
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(
//       "SELECT stalk_on_up FROM hosts WHERE "
//       "host_id=24",
//       std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_bool(0));
//
//   h.host_id = 1;
//   h.address = "10.0.2.16";
//   h.alias = "central1";
//   h.flap_detection_on_down = true;
//   h.flap_detection_on_unreachable = true;
//   h.flap_detection_on_up = true;
//   h.host_name = "central_1";
//   h.notify_on_down = true;
//   h.notify_on_unreachable = true;
//   h.poller_id = 1;
//   h.stalk_on_down = false;
//   h.stalk_on_unreachable = false;
//   h.stalk_on_up = false;
//   h.statusmap_image = "";
//   h.timezone = "Europe/Paris";
//
//   host_insupdate << h;
//   ms->run_statement(host_insupdate, "", false, 0);
//   ms->commit();
//   std::promise<mysql_result> promise_r2;
//   std::future<mysql_result> future_r2 = promise_r2.get_future();
//   ms->run_query_and_get_result("SELECT host_id FROM hosts",
//                                std::move(promise_r2));
//   res = future_r2.get();
//   for (int i = 0; i < 2; ++i) {
//     ASSERT_TRUE(ms->fetch_row(res));
//     int v(res.value_as_i32(0));
//     ASSERT_TRUE(v == 1 || v == 24);
//   }
// }
//
TEST_F(DatabaseStorageTest, CustomVarStatement) {
  config::applier::modules modules(log_v2::instance().get(log_v2::SQL));
  modules.load_file("./broker/neb/10-neb.so");
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  std::unique_ptr<mysql> ms(
      new mysql(db_cfg, log_v2::instance().get(log_v2::SQL)));
  query_preparator::event_unique unique;
  unique.insert("host_id");
  unique.insert("name");
  unique.insert("service_id");
  query_preparator qp(neb::custom_variable::static_type(), unique);
  mysql_stmt cv_insert_or_update(qp.prepare_insert_or_update(*ms));
  mysql_stmt cv_delete(qp.prepare_delete(*ms));

  neb::custom_variable cv;
  cv.service_id = 498;
  cv.update_time = time(nullptr);
  cv.modified = false;
  cv.host_id = 31;
  cv.name = "PROCESSNAME";
  cv.value = "centengine";
  cv.default_value = "centengine";

  cv_insert_or_update << cv;
  ms->run_statement(cv_insert_or_update, mysql_error::empty, 0);

  // Deletion
  cv_delete << cv;
  ms->run_statement(cv_delete, mysql_error::empty, 0);

  // Insert
  cv_insert_or_update << cv;
  ms->run_statement(cv_insert_or_update, mysql_error::empty, 0);

  // Update
  cv.update_time = time(nullptr) + 1;
  cv_insert_or_update << cv;
  ms->run_statement(cv_insert_or_update, mysql_error::empty, 0);

  ms->commit();

  std::string query(
      "SELECT host_id FROM customvariables WHERE "
      "host_id=31 AND service_id=498"
      " AND name='PROCESSNAME'");
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query, std::move(promise));
  mysql_result res(future.get());
  ASSERT_TRUE(ms->fetch_row(res));
  uint64_t r = res.value_as_u64(0);
  ASSERT_TRUE(r > 0);
  ASSERT_FALSE(ms->fetch_row(res));

  promise = {};
  future = promise.get_future();
  mysql_stmt stmt(ms->prepare_query(query));
  ms->run_statement_and_get_result(stmt, std::move(promise), -1, 200);
  res = future.get();
  ASSERT_TRUE(ms->fetch_row(res));
  r = res.value_as_u64(0);
  ASSERT_TRUE(r > 0);
  ASSERT_FALSE(ms->fetch_row(res));
}

// TEST_F(DatabaseStorageTest, ModuleStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator qp(neb::module::static_type());
//   mysql_stmt module_insert(qp.prepare_insert(*ms));
//
//   neb::module m;
//   m.should_be_loaded = true;
//   m.filename = "/usr/lib64/nagios/cbmod.so";
//   m.loaded = true;
//   m.poller_id = 1;
//   m.args = "/etc/centreon-broker/central-module.xml";
//
//   // Deletion
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_query_and_get_int("DELETE FROM modules", std::move(promise),
//                             mysql_task::AFFECTED_ROWS);
//   // We wait for the deletion to be done
//   future.get();
//
//   // Insert
//   module_insert << m;
//   ms->run_statement(module_insert, "", false);
//   ms->commit();
//
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result("SELECT module_id FROM modules LIMIT 1",
//                                std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
// }
//
//// log_entry (17) statement queries
// TEST_F(DatabaseStorageTest, LogStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator qp(neb::log_entry::static_type());
//   mysql_stmt log_insert(qp.prepare_insert(*ms));
//
//   neb::log_entry le;
//   le.poller_name = "Central";
//   le.msg_type = 5;
//   le.output = "Event loop start at bar date";
//   le.notification_contact = "";
//   le.notification_cmd = "";
//   le.status = 0;
//   le.host_name = "";
//   le.c_time = time(nullptr);
//
//   // Deletion
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_query_and_get_int("DELETE FROM logs", std::move(promise),
//                             mysql_task::int_type::AFFECTED_ROWS);
//   // We wait for the deletion
//   future.get();
//
//   // Insert
//   log_insert << le;
//   ms->run_statement(log_insert, "", false);
//   ms->commit();
//
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(
//       "SELECT log_id FROM logs "
//       "WHERE output = \"Event loop start at bar date\"",
//       std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
// }
//
//// Instance status (16) statement
// TEST_F(DatabaseStorageTest, InstanceStatusStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("instance_id");
//   query_preparator qp(neb::instance_status::static_type(), unique);
//   mysql_stmt inst_status_update(qp.prepare_update(*ms));
//
//   neb::instance_status is;
//   is.active_host_checks_enabled = true;
//   is.active_service_checks_enabled = true;
//   is.check_hosts_freshness = false;
//   is.check_services_freshness = true;
//   is.global_host_event_handler = "";
//   is.global_service_event_handler = "";
//   is.last_alive = time(nullptr) - 5;
//   is.obsess_over_hosts = false;
//   is.obsess_over_services = false;
//   is.passive_host_checks_enabled = true;
//   is.passive_service_checks_enabled = true;
//   is.poller_id = 1;
//
//   // Insert
//   inst_status_update << is;
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_statement_and_get_int(inst_status_update, std::move(promise),
//                                 mysql_task::int_type::AFFECTED_ROWS);
//   ASSERT_TRUE(future.get() == 1);
//   ms->commit();
//
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(
//       "SELECT active_host_checks FROM instances "
//       "WHERE instance_id=1",
//       std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_bool(0));
// }
//
//// Host check (8) statement
// TEST_F(DatabaseStorageTest, HostCheckStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   query_preparator qp(neb::host_check::static_type(), unique);
//   mysql_stmt host_check_update(qp.prepare_update(*ms));
//
//   neb::host_check hc;
//   hc.command_line =
//       "/usr/lib/nagios/plugins/check_icmp -H 10.0.2.15 -w 3000.0,80% -c "
//       "5000.0,100% -p 1";
//   hc.host_id = 24;
//
//   // Update
//   host_check_update << hc;
//   ms->run_statement(host_check_update, "", false);
//   ms->commit();
//
//   std::promise<mysql_result> promise;
//   std::future<mysql_result> future = promise.get_future();
//   ms->run_query_and_get_result("SELECT host_id FROM hosts WHERE host_id=24",
//                                std::move(promise));
//   mysql_result res(future.get());
//   ASSERT_TRUE(ms->fetch_row(res));
// }
//
//// Host status (14) statement
// TEST_F(DatabaseStorageTest, HostStatusStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   query_preparator qp(neb::host_status::static_type(), unique);
//   mysql_stmt host_status_update(qp.prepare_update(*ms));
//
//   neb::host_status hs;
//   hs.active_checks_enabled = true;
//   hs.check_command = "base_host_alive";
//   hs.check_interval = 5;
//   hs.check_period = "24x7";
//   hs.check_type = 0;
//   hs.current_check_attempt = 1;
//   hs.current_state = 0;
//   hs.downtime_depth = 0;
//   hs.enabled = true;
//   hs.execution_time = 0.159834;
//   hs.has_been_checked = true;
//   hs.host_id = 24;
//   hs.last_check = time(nullptr) - 3;
//   hs.last_hard_state = 0;
//   hs.last_update = time(nullptr) - 300;
//   hs.latency = 0.001;
//   hs.max_check_attempts = 3;
//   hs.next_check = time(nullptr) + 50;
//   hs.obsess_over = true;
//   hs.output = "OK - 10.0.2.15: rta 0,020ms, lost 0%\n";
//   hs.perf_data =
//       "rta=0,020ms;3000,000;5000,000;0; pl=0%;80;100;; rtmax=0,020ms;;;; "
//       "rtmin=0,020ms;;;;";
//   hs.retry_interval = 1;
//   hs.should_be_scheduled = true;
//   hs.state_type = 1;
//
//   // Update
//   host_status_update << hs;
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_statement_and_get_int(host_status_update, std::move(promise),
//                                 mysql_task::int_type::AFFECTED_ROWS);
//
//   ASSERT_TRUE(future.get() == 1);
//
//   ms->commit();
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(
//       "SELECT execution_time FROM hosts WHERE host_id=24",
//       std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_f64(0) == 0.159834);
// }
//
//// Service (23) statement
// TEST_F(DatabaseStorageTest, ServiceStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   unique.insert("service_id");
//   query_preparator qp(neb::service::static_type(), unique);
//   mysql_stmt service_insupdate(qp.prepare_insert_or_update(*ms));
//
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_query_and_get_int("DELETE FROM services", std::move(promise),
//                             mysql_task::int_type::AFFECTED_ROWS);
//   future.get();
//
//   neb::service s;
//   s.host_id = 24;
//   s.service_id = 318;
//   s.default_active_checks_enabled = true;
//   s.default_event_handler_enabled = true;
//   s.default_flap_detection_enabled = true;
//   s.default_notifications_enabled = true;
//   s.default_passive_checks_enabled = true;
//   s.display_name = "test-dbr";
//   s.icon_image = "";
//   s.icon_image_alt = "";
//   s.notification_interval = 30;
//   s.notification_period = "";
//   s.notify_on_downtime = true;
//   s.notify_on_flapping = true;
//   s.notify_on_recovery = true;
//   s.retain_nonstatus_information = true;
//   s.retain_status_information = true;
//
//   // Update
//   service_insupdate << s;
//   ms->run_statement(service_insupdate, "", false);
//
//   ms->commit();
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(
//       "SELECT notification_interval FROM services WHERE host_id=24 AND "
//       "service_id=318",
//       std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_i32(0) == 30);
// }
//
//// Service Check (19) statement
// TEST_F(DatabaseStorageTest, ServiceCheckStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   unique.insert("service_id");
//   query_preparator qp(neb::service_check::static_type(), unique);
//   mysql_stmt service_check_update(qp.prepare_update(*ms));
//
//   neb::service_check sc;
//   sc.service_id = 318;
//   sc.host_id = 24;
//   sc.command_line = "/usr/bin/bash /home/admin/test.sh";
//
//   // Update
//   service_check_update << sc;
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_statement_and_get_int(service_check_update, std::move(promise),
//                                 mysql_task::int_type::AFFECTED_ROWS);
//
//   ASSERT_TRUE(future.get() == 1);
//
//   ms->commit();
//   std::promise<mysql_result> promise_r;
//   std::future<mysql_result> future_r = promise_r.get_future();
//   ms->run_query_and_get_result(
//       "SELECT command_line FROM services WHERE host_id=24 AND
//       service_id=318", std::move(promise_r));
//   mysql_result res(future_r.get());
//   ASSERT_TRUE(ms->fetch_row(res));
//   ASSERT_TRUE(res.value_as_str(0) == "/usr/bin/bash /home/admin/test.sh");
// }
//
//// Service Status (24) statement
// TEST_F(DatabaseStorageTest, ServiceStatusStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   unique.insert("service_id");
//   query_preparator qp(neb::service_status::static_type(), unique);
//   mysql_stmt service_status_update(qp.prepare_update(*ms));
//
//   neb::service_status ss;
//   ss.last_time_critical = time(nullptr) - 1000;
//   ss.last_time_ok = time(nullptr) - 50;
//   ss.last_time_unknown = time(nullptr) - 1500;
//   ss.last_time_warning = time(nullptr) - 500;
//   ss.service_id = 318;
//   ss.host_id = 24;
//
//   // Update
//   service_status_update << ss;
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_statement_and_get_int(service_status_update, std::move(promise),
//                                 mysql_task::int_type::AFFECTED_ROWS);
//
//   ASSERT_TRUE(future.get() == 1);
//
//   ms->commit();
// }
//
// TEST_F(DatabaseStorageTest, CustomvariableStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//
//   query_preparator::event_unique unique;
//   unique.insert("host_id");
//   unique.insert("name");
//   unique.insert("service_id");
//   query_preparator qp(neb::custom_variable::static_type(), unique);
//   mysql_stmt custom_variable_insupdate(qp.prepare_insert_or_update(*ms));
//
//   neb::custom_variable cv;
//   cv.default_value = "empty";
//   cv.modified = false;
//   cv.var_type = 1;
//   cv.update_time = 0;
//
//   std::cout << "SEND CUSTOM VARIABLES" << std::endl;
//   for (int j = 1; j <= 20; j++) {
//     for (int i = 1; i <= 30; i++) {
//       cv.host_id = j;
//       std::ostringstream oss;
//       oss << "cv_" << i << "_" << j;
//       cv.name = oss.str();
//       oss.str("");
//       oss << "v" << i << "_" << j;
//       cv.value = oss.str();
//
//       custom_variable_insupdate << cv;
//       ms->run_statement(custom_variable_insupdate, "ERROR CV STATEMENT",
//       true);
//     }
//   }
//   std::cout << "SEND CUSTOM VARIABLES => DONE" << std::endl;
//   ms->commit();
//   std::cout << "COMMIT => DONE" << std::endl;
//   std::string query(
//       "SELECT count(*) FROM customvariables WHERE service_id = 0");
//   std::promise<mysql_result> promise;
//   std::future<mysql_result> future = promise.get_future();
//   ms->run_query_and_get_result(query, std::move(promise));
//   mysql_result res(future.get());
//
//   ASSERT_TRUE(ms->fetch_row(res));
//   std::cout << "***** count = " << res.value_as_i32(0) << std::endl;
//   ASSERT_TRUE(res.value_as_i32(0) == 600);
// }
//
// TEST_F(DatabaseStorageTest, DowntimeStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//
//   std::string query(
//       "INSERT INTO downtimes"
//       " (actual_end_time, "
//       "actual_start_time, "
//       "author, type, deletion_time, duration, end_time, entry_time, "
//       "fixed, host_id, instance_id, internal_id, service_id, "
//       "start_time, triggered_by, cancelled, started, comment_data) "
//       "VALUES(:actual_end_time,:actual_start_time,:author,:type,:deletion_"
//       "time,:duration,:end_time,:entry_time,:fixed,:host_id,:instance_id,:"
//       "internal_id,:service_id,:start_time,:triggered_by,:cancelled,:"
//       "started,:comment_data) ON DUPLICATE KEY UPDATE "
//       "actual_end_time=GREATEST(COALESCE(actual_end_time, -1), "
//       ":actual_end_time),"
//       "actual_start_time=COALESCE(actual_start_time, :actual_start_time),"
//       "author=:author, cancelled=:cancelled, comment_data=:comment_data,"
//       "deletion_time=:deletion_time, duration=:duration, end_time=:end_time,"
//       "fixed=:fixed, host_id=:host_id, service_id=:service_id,"
//       "start_time=:start_time, started=:started,"
//       "triggered_by=:triggered_by, type=:type");
//   mysql_stmt downtime_insupdate(mysql_stmt(query, true));
//   ms->prepare_statement(downtime_insupdate);
//
//   time_t now(time(nullptr));
//
//   neb::downtime d;
//   d.actual_end_time = now;
//   d.actual_start_time = now - 30;
//   d.comment = "downtime test";
//   d.downtime_type = 1;
//   d.duration = 30;
//   d.end_time = now;
//   d.entry_time = now - 30;
//   d.fixed = true;
//   d.host_id = 24;
//   d.poller_id = 1;
//   d.service_id = 318;
//   d.start_time = now - 30;
//   d.was_started = true;
//
//   downtime_insupdate << d;
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   ms->run_statement_and_get_int(downtime_insupdate, std::move(promise),
//                                 mysql_task::int_type::AFFECTED_ROWS);
//
//   std::cout << "downtime_insupdate: " << downtime_insupdate.get_query()
//             << std::endl;
//
//   ASSERT_TRUE(future.get() == 1);
//   ms->commit();
// }
//
// TEST_F(DatabaseStorageTest, HostGroupMemberStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//
//   ms->run_query("DELETE FROM hostgroups");
//   ms->run_query("DELETE FROM hosts_hostgroups");
//   ms->commit();
//
//   query_preparator::event_unique unique;
//   unique.insert("hostgroup_id");
//   unique.insert("host_id");
//   query_preparator qp(neb::host_group_member::static_type(), unique);
//   mysql_stmt host_group_member_insert(qp.prepare_insert(*ms));
//
//   query_preparator::event_unique unique1;
//   unique1.insert("hostgroup_id");
//   query_preparator qp1(neb::host_group::static_type(), unique1);
//   mysql_stmt host_group_insupdate(qp1.prepare_insert_or_update(*ms));
//
//   neb::host_group_member hgm;
//   hgm.enabled = true;
//   hgm.group_id = 8;
//   hgm.group_name = "Test host group";
//   hgm.host_id = 24;
//   hgm.poller_id = 1;
//
//   host_group_member_insert << hgm;
//   std::cout << host_group_member_insert.get_query() << std::endl;
//
//   std::promise<mysql_result> promise;
//   std::future<mysql_result> future = promise.get_future();
//
//   int thread_id(ms->run_statement_and_get_result(host_group_member_insert,
//                                                  std::move(promise)));
//   try {
//     future.get();
//   } catch (std::exception const& e) {
//     neb::host_group hg;
//     hg.id = 8;
//     hg.name = "Test hostgroup";
//     hg.enabled = true;
//     hg.poller_id = 1;
//
//     host_group_insupdate << hg;
//
//     std::cout << host_group_insupdate.get_query() << std::endl;
//
//     ms->run_statement(host_group_insupdate,
//                       "Error: Unable to create host group", true, thread_id);
//
//     host_group_member_insert << hgm;
//     ms->run_statement(host_group_member_insert, "Error: host group not
//     defined",
//                       true, thread_id);
//   }
//   ms->commit();
// }
//
// TEST_F(DatabaseStorageTest, HostParentStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//
//   query_preparator qp(neb::host_parent::static_type());
//   mysql_stmt host_parent_insert(qp.prepare_insert(*ms, true));
//   query_preparator::event_unique unique;
//   unique.insert("child_id");
//   unique.insert("parent_id");
//   query_preparator qp_del(neb::host_parent::static_type(), unique);
//   mysql_stmt host_parent_delete = qp_del.prepare_delete(*ms);
//
//   neb::host_parent hp;
//   hp.enabled = true;
//   hp.host_id = 24;
//   hp.parent_id = 1;
//
//   // Insert.
//   host_parent_insert << hp;
//   std::promise<int> promise;
//   std::future<int> future = promise.get_future();
//   int thread_id(
//       ms->run_statement_and_get_int(host_parent_insert, std::move(promise),
//                                     mysql_task::int_type::AFFECTED_ROWS));
//
//   ASSERT_TRUE(future.get() == 1);
//
//   std::promise<int> promise2;
//   std::future<int> future2 = promise2.get_future();
//   // Second insert attempted just for the check
//   ms->run_statement_and_get_int(host_parent_insert, std::move(promise2),
//                                 mysql_task::int_type::AFFECTED_ROWS,
//                                 thread_id);
//
//   ASSERT_TRUE(future2.get() == 0);
//
//   // Disable parenting.
//   hp.enabled = false;
//
//   host_parent_delete << hp;
//   std::promise<int> promise3;
//   std::future<int> future3 = promise3.get_future();
//   ms->run_statement_and_get_int(host_parent_delete, std::move(promise3),
//                                 mysql_task::int_type::AFFECTED_ROWS,
//                                 thread_id);
//
//   ASSERT_TRUE(future3.get() == 1);
//   ms->commit();
// }
//
// TEST_F(DatabaseStorageTest, ServiceGroupMemberStatement) {
//   modules::loader l;
//   l.load_file("./lib/10-neb.so");
//   database_config db_cfg("MySQL", "127.0.0.1", 3306, "centreon", "centreon",
//                          "centreon_storage", 5, true, 5);
//   std::unique_ptr<mysql> ms(new mysql(db_cfg));
//
//   ms->run_query("DELETE FROM servicegroups");
//   ms->run_query("DELETE FROM services_servicegroups");
//   ms->commit();
//
//   query_preparator::event_unique unique;
//   unique.insert("servicegroup_id");
//   unique.insert("host_id");
//   unique.insert("service_id");
//   query_preparator qp(neb::service_group_member::static_type(), unique);
//   mysql_stmt service_group_member_insert(qp.prepare_insert(*ms));
//
//   query_preparator::event_unique unique1;
//   unique1.insert("servicegroup_id");
//   query_preparator qp1(neb::service_group::static_type(), unique1);
//   mysql_stmt service_group_insupdate(qp1.prepare_insert_or_update(*ms));
//
//   neb::service_group_member sgm;
//   sgm.enabled = false;
//   sgm.group_id = 8;
//   sgm.group_name = "Test service group";
//   sgm.host_id = 24;
//   sgm.service_id = 78;
//   sgm.poller_id = 1;
//
//   service_group_member_insert << sgm;
//
//   std::promise<mysql_result> promise;
//   std::future<mysql_result> future = promise.get_future();
//
//   int thread_id(ms->run_statement_and_get_result(service_group_member_insert,
//                                                  std::move(promise)));
//   ASSERT_THROW(future.get(), std::exception);
//   neb::service_group sg;
//   sg.id = 8;
//   sg.name = "Test servicegroup";
//   sg.enabled = true;
//   sg.poller_id = 1;
//
//   service_group_insupdate << sg;
//
//   ms->run_statement(service_group_insupdate,
//                     "Error: Unable to create service group", true,
//                     thread_id);
//
//   std::promise<mysql_result> promise2;
//   std::future<mysql_result> future2 = promise2.get_future();
//   service_group_member_insert << sgm;
//   ms->run_statement_and_get_result(service_group_member_insert,
//                                    std::move(promise2), thread_id);
//   ASSERT_NO_THROW(future2.get());
//   ms->commit();
// }
//
////// Given a mysql object
////// When a prepare statement is done
////// Then we can bind several rows of values to it and execute the statement.
////// Then a commit makes data available in the database.
//// TEST_F(DatabaseStorageTest, PrepareBulkQuery) {
////  database_config db_cfg(
////    "MySQL",
////    "127.0.0.1",
////    3306,
////    "centreon",
////    "centreon",
////    "centreon_storage",
////    5,
////    true,
////    5);
////  time_t now(time(NULL));
////  std::ostringstream oss;
////  oss << "INSERT INTO " << "metrics"
////      << "  (index_id, metric_name, unit_name, warn, warn_low,"
////         "   warn_threshold_mode, crit, crit_low, "
////         "   crit_threshold_mode, min, max, current_value,"
////         "   data_source_type)"
////         " VALUES (?, ?, ?, ?, "
////         "         ?, ?, ?, "
////         "         ?, ?, ?, ?, "
////         "         ?, ?)";
////
////  std::unique_ptr<mysql> ms(new mysql(db_cfg));
////  std::ostringstream nss;
////  nss << "metric_name - " << time(NULL);
////  mysql_stmt stmt(ms->prepare_query(oss.str()));
////
////  // Rows are just put on the same row one after the other. The important
////  thing
////  // is to know the length of a row in bytes.
////  stmt.set_array_size(2);
////  for (int i(0); i < 2; ++i) {
////    stmt.bind_value_as_i32(0 + i * 13, 19);
////    stmt.bind_value_as_str(1 + i * 13, nss.str());
////    stmt.bind_value_as_str(2 + i * 13, "test/s");
////    stmt.bind_value_as_f32(3 + i * 13, NAN);
////    stmt.bind_value_as_f32(4 + i * 13, INFINITY);
////    stmt.bind_value_as_tiny(5 + i * 13, true);
////    stmt.bind_value_as_f32(6 + i * 13, 10.0);
////    stmt.bind_value_as_f32(7 + i * 13, 20.0);
////    stmt.bind_value_as_tiny(8 + i * 13, false);
////    stmt.bind_value_as_f32(9 + i * 13, 0.0);
////    stmt.bind_value_as_f32(10 + i * 13, 50.0);
////    stmt.bind_value_as_f32(11 + i * 13, 1 + 2 * i);
////    stmt.bind_value_as_str(12 + i * 13, "2");
////  }
////  // We force the thread 0
////  ms->run_statement(stmt, NULL, "", false, 0);
////  oss.str("");
////  oss << "SELECT metric_name FROM metrics WHERE metric_name='" << nss.str()
///<< /  "'"; std::promise<mysql_result> promise; ms->run_query(oss.str(),
///&promise); /  mysql_result res(promise.get_future().get()); /
/// ASSERT_FALSE(ms->fetch_row(res)); /  ASSERT_NO_THROW(ms->commit()); /
/// promise = std::promise<mysql_result>(); /  ms->run_query(oss.str(),
///&promise); /  res = promise.get_future().get(); /
/// ASSERT_TRUE(ms->fetch_row(res));
////}
//
TEST_F(DatabaseStorageTest, ChooseConnectionByName) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms =
      std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL));
  int thread_foo(ms->choose_connection_by_name("foo"));
  int thread_bar(ms->choose_connection_by_name("bar"));
  int thread_boo(ms->choose_connection_by_name("boo"));
  int thread_foo1(ms->choose_connection_by_name("foo"));
  int thread_bar1(ms->choose_connection_by_name("bar"));
  int thread_boo1(ms->choose_connection_by_name("boo"));
  ASSERT_EQ(thread_foo, 0);
  ASSERT_EQ(thread_bar, 1);
  ASSERT_EQ(thread_boo, 2);
  ASSERT_EQ(thread_foo, thread_foo1);
  ASSERT_EQ(thread_bar, thread_bar1);
  ASSERT_EQ(thread_boo, thread_boo1);
}

// Given a mysql object
// When a prepare statement is done
// Then we can bind values to it and execute the statement.
// Then a commit makes data available in the database.
TEST_F(DatabaseStorageTest, RepeatStatements) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, unit_name CHAR(30), value DOUBLE, warn FLOAT, crit FLOAT, "
      "hidden enum('0', '1') DEFAULT '0', metric VARCHAR(30) CHARACTER SET "
      "utf8mb4 DEFAULT NULL) DEFAULT CHARSET=utf8mb4"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query(
      "INSERT INTO ut_test (unit_name, value, warn, crit, metric) VALUES "
      "(?,?,?,?,?)");
  mysql_stmt stmt(ms->prepare_query(query));

  constexpr int TOTAL = 200000;

  for (int i = 0; i < TOTAL; i++) {
    stmt.bind_value_as_str(0, fmt::format("unit_{}", i % 100));
    stmt.bind_value_as_f64(1, ((double)i) / 500);
    stmt.bind_value_as_f32(2, ((float)i) / 500 + 12.0f);
    stmt.bind_value_as_f32(3, ((float)i) / 500 + 25.0f);
    stmt.bind_value_as_str(4, fmt::format("metric_{}", i));
    ms->run_statement(stmt);
  }
  ms->commit();
  std::string query3{"SELECT count(*) from ut_test"};
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query3, std::move(promise));
  mysql_result res(future.get());

  ASSERT_TRUE(ms->fetch_row(res));
  std::cout << "***** count = " << res.value_as_i32(0) << std::endl;
  ASSERT_EQ(res.value_as_i32(0), TOTAL);

  std::string query4(
      "SELECT id, unit_name, value, warn, crit, hidden, metric FROM ut_test "
      "WHERE id < 20");
  promise = {};
  future = promise.get_future();
  ms->run_query_and_get_result(query4, std::move(promise));
  res = future.get();
  uint32_t count_query = 0;
  while (ms->fetch_row(res)) {
    count_query++;
    ASSERT_TRUE(res.value_as_u64(0) < 20);
    ASSERT_TRUE(res.value_as_str(1).substr(0, 5) == "unit_");
    ASSERT_TRUE(res.value_as_f64(3) >= 12);
    ASSERT_TRUE(res.value_as_f32(4) >= 25);
    ASSERT_TRUE(!res.value_as_bool(5));
    ASSERT_TRUE(res.value_as_str(6).substr(0, 7) == "metric_");
  }
  ASSERT_TRUE(count_query);

  /* Same query with a prepared statement */
  promise = {};
  future = promise.get_future();
  mysql_stmt select_stmt(ms->prepare_query(query4));
  ms->run_statement_and_get_result(select_stmt, std::move(promise), -1, 200);
  res = future.get();
  uint32_t count_statement = 0;
  while (ms->fetch_row(res)) {
    count_statement++;
    ASSERT_TRUE(res.value_as_i64(0) < 20);
    ASSERT_TRUE(res.value_as_str(1).substr(0, 5) == "unit_");
    ASSERT_TRUE(res.value_as_f64(3) >= 12);
    ASSERT_TRUE(res.value_as_f32(4) >= 25);
    ASSERT_TRUE(!res.value_as_bool(5));
    ASSERT_TRUE(res.value_as_str(6).substr(0, 7) == "metric_");
  }
  ASSERT_TRUE(count_query == count_statement);
}

TEST_F(DatabaseStorageTest, CheckBulkStatement) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string version = ms->get_server_version();
  std::vector<std::string_view> arr =
      absl::StrSplit(version, absl::ByAnyChar(".-"));
  ASSERT_TRUE(arr.size() >= 4u);
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  EXPECT_TRUE(absl::SimpleAtoi(arr[0], &major));
  EXPECT_TRUE(absl::SimpleAtoi(arr[1], &minor));
  EXPECT_TRUE(absl::SimpleAtoi(arr[2], &patch));
  std::string_view server_name(arr[3]);
  if (ms->support_bulk_statement()) {
    std::string query1{"DROP TABLE IF EXISTS ut_test"};
    std::string query2{
        "CREATE TABLE ut_test (id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT "
        "PRIMARY KEY, unit_name CHAR(30), value DOUBLE, warn FLOAT, crit "
        "FLOAT, hidden enum('0', '1') DEFAULT '0', metric VARCHAR(30) "
        "CHARACTER SET utf8mb4 DEFAULT NULL) DEFAULT CHARSET=utf8mb4"};
    ms->run_query(query1);
    ms->commit();
    ms->run_query(query2);
    ms->commit();

    std::string query(
        "INSERT INTO ut_test (unit_name, value, warn, crit, metric) VALUES "
        "(?,?,?,?,?)");
    mysql_bulk_stmt stmt(query, log_v2::instance().get(log_v2::SQL));
    ms->prepare_statement(stmt);

    constexpr int TOTAL = 200000;

    int step = 0;
    auto bb = stmt.create_bind();
    ASSERT_TRUE(bb->empty());
    bb->reserve(20000);
    for (int j = 0; j < TOTAL; j++) {
      bb->set_value_as_str(0, fmt::format("unit_{}", step));
      bb->set_value_as_f64(1, ((double)step) / 500);
      bb->set_value_as_f32(2, ((float)step) / 500 + 12.0f);
      bb->set_value_as_f32(3, ((float)step) / 500 + 25.0f);
      bb->set_value_as_str(4, fmt::format("metric_{}", step));
      bb->next_row();
      step++;
      if (step == 20000) {
        step = 0;
        stmt.set_bind(std::move(bb));
        ms->run_statement(stmt);
        bb = stmt.create_bind();
      }
    }
    ms->commit();
    std::string query3{"SELECT count(*) from ut_test"};
    std::promise<mysql_result> promise;
    std::future<mysql_result> future = promise.get_future();
    ms->run_query_and_get_result(query3, std::move(promise));
    mysql_result res(future.get());

    ASSERT_TRUE(ms->fetch_row(res));
    std::cout << "***** count = " << res.value_as_i32(0) << std::endl;
    ASSERT_EQ(res.value_as_i32(0), TOTAL);
  }
}

TEST_F(DatabaseStorageTest, UpdateBulkStatement) {
  constexpr int TOTAL = 20;

  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  if (ms->support_bulk_statement()) {
    std::string query{
        "UPDATE ut_test SET value=?, warn=?, crit=?, metric=?, hidden=? WHERE "
        "unit_name=?"};
    mysql_bulk_stmt s(query, log_v2::instance().get(log_v2::SQL));
    ms->prepare_statement(s);
    auto b = s.create_bind();

    for (int j = 0; j < TOTAL; j++) {
      b->set_value_as_str(5, fmt::format("unit_{}", j));
      b->set_value_as_f64(0, j);
      b->set_value_as_f32(1, 1000);
      b->set_value_as_f32(2, 2000);
      b->set_value_as_str(3, fmt::format("metric_{}", j));
      b->set_value_as_str(4, fmt::format("{}", j % 2));
      b->next_row();
    }
    s.set_bind(std::move(b));
    ms->run_statement(s);
    ms->commit();
    std::string query1{"SELECT count(*) from ut_test WHERE crit=2000"};
    std::promise<mysql_result> promise;
    std::future<mysql_result> future = promise.get_future();
    ms->run_query_and_get_result(query1, std::move(promise));
    mysql_result res(future.get());

    ASSERT_TRUE(ms->fetch_row(res));
    std::cout << "***** count = " << res.value_as_i32(0) << std::endl;
    ASSERT_TRUE(res.value_as_i32(0) == TOTAL * 10);
  } else
    std::cout << "Test not executed." << std::endl;

  std::string query(
      "SELECT DISTINCT id,value,warn,metric,hidden FROM ut_test WHERE id < ? "
      "LIMIT ?");
  mysql_stmt select_stmt(ms->prepare_query(query));
  select_stmt.bind_value_as_i32(0, TOTAL);
  select_stmt.bind_value_as_i32(1, TOTAL);
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_statement_and_get_result(select_stmt, std::move(promise), -1, 200);
  mysql_result res(future.get());

  bool inside = false;
  while (ms->fetch_row(res)) {
    inside = true;
    ASSERT_EQ(res.value_as_f64(1), res.value_as_i32(0) - 1);
    ASSERT_EQ(res.value_as_f32(2), 1000);
    ASSERT_EQ(res.value_as_str(3),
              fmt::format("metric_{}", res.value_as_i32(0) - 1));
    ASSERT_EQ(res.value_as_tiny(4), (res.value_as_i32(0) + 1) % 2);
    ASSERT_EQ(res.value_as_bool(4), res.value_as_i32(0) % 2 ? false : true);
  }
  ASSERT_TRUE(inside);
}

// Given a mysql object
// When a prepare statement is done
// Then we can bind values to it and execute the statement.
// Then a commit makes data available in the database.
TEST_F(DatabaseStorageTest, LastInsertId) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  time_t now = time(nullptr);
  std::string query(
      fmt::format("INSERT INTO metrics"
                  " (index_id, metric_name, unit_name, warn, warn_low,"
                  " warn_threshold_mode, crit, crit_low,"
                  " crit_threshold_mode, min, max, current_value,"
                  " data_source_type)"
                  " VALUES (19, 'metric_name - {}bis', 'test/s', 20.0, 40.0, "
                  "1, 10.0, 20.0, 1, 0.0, 50.0, 18.0, '2')",
                  now));

  auto ms =
      std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL));
  // We force the thread 0
  std::promise<int> promise;
  std::future<int> future = promise.get_future();
  ms->run_query_and_get_int(query, std::move(promise),
                            mysql_task::int_type::LAST_INSERT_ID);
  int id = future.get();

  // Commit is needed to make the select later. But it is not needed to get
  // the id. Moreover, if we commit before getting the last id, the result
  // will be null.
  ms->commit();
  ASSERT_TRUE(id > 0);
  std::cout << "id = " << id << std::endl;
  query = fmt::format(
      "SELECT metric_id FROM metrics WHERE metric_name = 'metric_name - "
      "{}bis'",
      now);

  std::promise<mysql_result> promise_r;
  std::future<database::mysql_result> future_r = promise_r.get_future();
  ms->run_query_and_get_result(query, std::move(promise_r));
  mysql_result res(future_r.get());
  ASSERT_TRUE(ms->fetch_row(res));
  ASSERT_TRUE(res.value_as_i32(0) == id);
}

TEST_F(DatabaseStorageTest, BulkStatementWithNullStr) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  if (ms->support_bulk_statement()) {
    std::string query1{"DROP TABLE IF EXISTS ut_test"};
    std::string query2{
        "CREATE TABLE ut_test (id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT "
        "PRIMARY KEY, unit_name CHAR(30), value DOUBLE, warn FLOAT, crit "
        "FLOAT, hidden enum('0', '1') DEFAULT '0', metric VARCHAR(30) "
        "CHARACTER SET utf8mb4 DEFAULT NULL) DEFAULT CHARSET=utf8mb4"};
    ms->run_query(query1);
    ms->commit();
    ms->run_query(query2);
    ms->commit();

    std::string query(
        "INSERT INTO ut_test (unit_name, value, warn, crit, metric) VALUES "
        "(?,?,?,?,?)");
    mysql_bulk_stmt stmt(query, log_v2::instance().get(log_v2::SQL));
    ms->prepare_statement(stmt);

    constexpr int TOTAL = 200000;

    int step = 0;
    auto bb = stmt.create_bind();
    bb->reserve(20000);
    for (int j = 0; j < TOTAL; j++) {
      if (step % 2)
        bb->set_value_as_str(0, fmt::format("unit_{}", step));
      else
        bb->set_null_str(0);
      if (step % 2 == 0)
        bb->set_value_as_f64(1, ((double)step) / 500);
      else
        bb->set_null_f64(1);
      bb->set_value_as_f32(2, ((float)step) / 500 + 12.0f);
      bb->set_value_as_f32(3, ((float)step) / 500 + 25.0f);
      bb->set_value_as_str(4, fmt::format("metric_{}", step));
      bb->next_row();
      step++;
      if (step == 20000) {
        step = 0;
        stmt.set_bind(std::move(bb));
        ms->run_statement(stmt);
        bb = stmt.create_bind();
      }
    }
    ms->commit();
    std::string query3{
        "SELECT count(*) from ut_test WHERE unit_name is NULL OR value is "
        "NULL"};
    std::promise<mysql_result> promise;
    std::future<mysql_result> future = promise.get_future();
    ms->run_query_and_get_result(query3, std::move(promise));
    mysql_result res(future.get());

    ASSERT_TRUE(ms->fetch_row(res));
    std::cout << "***** count = " << res.value_as_i32(0) << std::endl;
    ASSERT_EQ(res.value_as_i32(0), TOTAL);
  }
}

TEST_F(DatabaseStorageTest, RepeatStatementsWithNull) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, unit_name CHAR(30), value DOUBLE, warn FLOAT, crit FLOAT, "
      "hidden enum('0', '1') DEFAULT '0', metric VARCHAR(30) CHARACTER SET "
      "utf8mb4 DEFAULT NULL) DEFAULT CHARSET=utf8mb4"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query(
      "INSERT INTO ut_test (unit_name, value, warn, crit, metric) VALUES "
      "(?,?,?,?,?)");
  mysql_stmt stmt(ms->prepare_query(query));

  constexpr int TOTAL = 20000;

  for (int i = 0; i < TOTAL; i++) {
    if (i % 2 == 0)
      stmt.bind_value_as_str(0, fmt::format("unit_{}", i % 100));
    else
      stmt.bind_null_str(0);
    if (i % 2 == 1)
      stmt.bind_value_as_f64(1, ((double)i) / 500);
    else
      stmt.bind_null_f64(1);
    stmt.bind_value_as_f32(2, ((float)i) / 500 + 12.0f);
    stmt.bind_value_as_f32(3, ((float)i) / 500 + 25.0f);
    stmt.bind_value_as_str(4, fmt::format("metric_{}", i));
    ms->run_statement(stmt);
  }
  ms->commit();
  std::string query3{
      "SELECT count(*) from ut_test WHERE unit_name IS NULL OR value IS NULL"};
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query3, std::move(promise));
  mysql_result res(future.get());

  ASSERT_TRUE(ms->fetch_row(res));
  std::cout << "***** count = " << res.value_as_i32(0) << std::endl;
  ASSERT_EQ(res.value_as_i32(0), TOTAL);
}

TEST_F(DatabaseStorageTest, RepeatStatementsWithBigStrings) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e enum('a', "
      "'b', "
      "'c') DEFAULT 'a')"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query("INSERT INTO ut_test (name, value, t, e) VALUES (?,?,?,?)");
  mysql_stmt stmt(ms->prepare_query(query));

  constexpr int TOTAL = 200;

  for (int i = 0; i < TOTAL; i++) {
    stmt.bind_value_as_str(0, fmt::format("{:a>{}}", i, 500));
    stmt.bind_value_as_f64(1, static_cast<double>(i));
    stmt.bind_value_as_tiny(2, i % 100);
    stmt.bind_value_as_tiny(3, (i % 3) + 1);
    ms->run_statement(stmt);
  }
  ms->commit();

  std::string query4("SELECT id, name, value, t, e FROM ut_test");
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query4, std::move(promise));
  mysql_result res = future.get();
  size_t count = 0;
  while (ms->fetch_row(res)) {
    count++;
    int64_t id = res.value_as_i64(0);
    ASSERT_TRUE(id >= 1 && id <= TOTAL);

    double value = res.value_as_f64(2);
    int32_t ivalue = static_cast<int32_t>(value);
    ASSERT_TRUE(value >= 0 && value <= TOTAL - 1);

    std::string name(res.value_as_str(1));
    std::string exp_name(fmt::format("{:a>{}}", ivalue, 500));

    ASSERT_EQ(name, exp_name);

    ASSERT_EQ(res.value_as_tiny(3), ivalue % 100);

    char exp_char = 'a' + (ivalue % 3);
    char e = res.value_as_str(4)[0];
    ASSERT_EQ(e, exp_char);
  }
  ASSERT_EQ(count, TOTAL);

  mysql_stmt select_stmt(ms->prepare_query(query4));
  for (size_t length : {200, 1000}) {
    std::cout << "Case with length = " << length << std::endl;
    promise = {};
    future = promise.get_future();
    ms->run_statement_and_get_result(select_stmt, std::move(promise), -1,
                                     length);
    res = future.get();
    count = 0;

    if (length == 200)
      testing::internal::CaptureStdout();

    while (ms->fetch_row(res)) {
      count++;
      int32_t id = res.value_as_u32(0);
      ASSERT_TRUE(id >= 1 && id <= TOTAL);

      double value = res.value_as_f64(2);
      int32_t ivalue = static_cast<int32_t>(value);
      ASSERT_TRUE(value >= 0 && value <= TOTAL - 1);

      std::string name(res.value_as_str(1));
      /* A string "aaaaa.....aaaaaNNN" where a is repeated 500 times and
       * NNN is ivalue */
      std::string exp_name(fmt::format("{:a>{}}", ivalue, 500));

      ASSERT_EQ(name, exp_name);

      ASSERT_EQ(res.value_as_tiny(3), ivalue % 100);

      char exp_char = 'a' + (ivalue % 3);
      char e = res.value_as_str(4)[0];
      ASSERT_EQ(e, exp_char);
    }
    if (length == 200) {
      std::string output(testing::internal::GetCapturedStdout());
      std::cout << output << std::endl;
      ASSERT_TRUE(output.find("columns in the current row are too long") !=
                  std::string::npos);
    } else
      ASSERT_EQ(count, TOTAL);
  }
}

TEST_F(DatabaseStorageTest, RepeatStatementsWithNullValues) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e enum('a', "
      "'b', 'c') DEFAULT 'a', b TINYINT)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query("INSERT INTO ut_test (name,b) VALUES (?,?)");
  mysql_stmt stmt(ms->prepare_query(query));

  constexpr int TOTAL = 200;

  for (int i = 0; i < TOTAL; i++) {
    stmt.bind_value_as_str(0, fmt::format("foo{}", i));
    stmt.bind_value_as_tiny(1, i % 2);
    ms->run_statement(stmt);
  }
  ms->commit();

  std::string query4("SELECT id, value, b FROM ut_test LIMIT 5");
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query4, std::move(promise));
  mysql_result res = future.get();
  bool inside = false;
  while (ms->fetch_row(res)) {
    inside = true;
    int64_t id = res.value_as_i64(0);
    ASSERT_TRUE(id >= 1 && id <= TOTAL);

    bool value = res.value_is_null(0);
    ASSERT_FALSE(value);

    value = res.value_is_null(1);
    ASSERT_TRUE(value);

    double val = res.value_as_f64(1);
    ASSERT_EQ(val, 0);

    char b = res.value_as_tiny(2);
    ASSERT_TRUE(b == 0 || b == 1);

    bool bb = res.value_as_bool(2);
    ASSERT_TRUE((b && bb) || (!b && !bb));
  }
  ASSERT_TRUE(inside);

  mysql_stmt select_stmt(ms->prepare_query(query4));
  promise = {};
  future = promise.get_future();
  ms->run_statement_and_get_result(select_stmt, std::move(promise), -1, 50);
  res = future.get();
  inside = false;

  while (ms->fetch_row(res)) {
    inside = true;
    int32_t id = res.value_as_i64(0);
    ASSERT_TRUE(id >= 1 && id <= TOTAL);

    bool value = res.value_is_null(0);
    ASSERT_FALSE(value);

    value = res.value_is_null(1);
    ASSERT_TRUE(value);

    double val = res.value_as_f64(1);
    ASSERT_EQ(val, 0);
  }
  ASSERT_TRUE(inside);
}

TEST_F(DatabaseStorageTest, BulkStatementsWithNullValues) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e enum('a', "
      "'b', 'c') DEFAULT 'a', b TINYINT, i INT, u INT UNSIGNED)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query("INSERT INTO ut_test (name,b, i, u) VALUES (?,?, ?, ?)");
  mysql_bulk_stmt stmt(query, log_v2::instance().get(log_v2::SQL));
  ms->prepare_statement(stmt);

  auto bb = stmt.create_bind();
  bb->reserve(200);
  constexpr int TOTAL = 200;

  for (int i = 0; i < TOTAL; i++) {
    ASSERT_EQ(bb->current_row(), i);
    if (i % 2) {
      bb->set_value_as_str(0, fmt::format("foo{}", i));
      bb->set_value_as_tiny(1, (i / 2) % 2);
      bb->set_value_as_i32(2, i + 2);
      bb->set_value_as_u32(3, i);
    } else {
      bb->set_null_str(0);
      bb->set_null_tiny(1);
      bb->set_null_i32(2);
      bb->set_null_u32(3);
    }
    bb->next_row();
  }
  stmt.set_bind(std::move(bb));
  ms->run_statement(stmt);
  ms->commit();

  std::string query4("SELECT id, value, b, i, u FROM ut_test LIMIT 5");
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query4, std::move(promise));
  mysql_result res = future.get();
  bool inside1 = false;
  bool inside2 = false;
  while (ms->fetch_row(res)) {
    if (res.value_is_null(2)) {
      inside1 = true;
      ASSERT_TRUE(!res.value_is_null(0));
      ASSERT_TRUE(res.value_is_null(1));
      ASSERT_TRUE(res.value_is_null(3));
      ASSERT_TRUE(res.value_is_null(4));
    } else {
      inside2 = true;
      int64_t id = res.value_as_i64(0);
      ASSERT_TRUE(id >= 1 && id <= TOTAL);

      bool value = res.value_is_null(0);
      ASSERT_FALSE(value);

      value = res.value_is_null(1);
      ASSERT_TRUE(value);

      double val = res.value_as_f64(1);
      ASSERT_EQ(val, 0);

      char b = res.value_as_tiny(2);
      ASSERT_TRUE(b == 0 || b == 1);

      bool bb = res.value_as_bool(2);
      ASSERT_TRUE((b && bb) || (!b && !bb));

      int32_t i = res.value_as_i32(3);
      ASSERT_TRUE(i < TOTAL + 2 && i >= 2);

      int32_t u = res.value_as_u32(4);
      ASSERT_EQ(u + 2, i);
    }
  }
  ASSERT_TRUE(inside1 && inside2);

  mysql_stmt select_stmt(ms->prepare_query(query4));
  promise = {};
  future = promise.get_future();
  ms->run_statement_and_get_result(select_stmt, std::move(promise), -1, 50);
  res = future.get();
  bool inside = false;

  while (ms->fetch_row(res)) {
    inside = true;
    int32_t id = res.value_as_i64(0);
    ASSERT_TRUE(id >= 1 && id <= TOTAL);

    bool value = res.value_is_null(0);
    ASSERT_FALSE(value);

    value = res.value_is_null(1);
    ASSERT_TRUE(value);

    double val = res.value_as_f64(1);
    ASSERT_EQ(val, 0);
  }
  ASSERT_TRUE(inside);
}

TEST_F(DatabaseStorageTest, RepeatStatementsWithBooleanValues) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e enum('a', "
      "'b', 'c') DEFAULT 'a', b TINYINT)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query("INSERT INTO ut_test (name,b, t) VALUES (?,?,?)");
  mysql_stmt stmt(ms->prepare_query(query));

  constexpr int TOTAL = 200;

  for (int i = 0; i < TOTAL; i++) {
    stmt.bind_value_as_str(0, fmt::format("foo{}", i));
    stmt.bind_value_as_bool(1, (i % 2) == 0);
    stmt.bind_value_as_bool(2, (i % 2) == 1);
    ms->run_statement(stmt);
  }
  ms->commit();

  std::string query4("SELECT name, b, t FROM ut_test LIMIT 5");
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query4, std::move(promise));
  mysql_result res = future.get();
  bool inside = false;
  while (ms->fetch_row(res)) {
    inside = true;
    ASSERT_TRUE(res.value_as_bool(1) != res.value_as_bool(2));
  }
  ASSERT_TRUE(inside);
}

TEST_F(DatabaseStorageTest, BulkStatementsWithBooleanValues) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e enum('a', "
      "'b', 'c') DEFAULT 'a', b TINYINT, i INT, u INT UNSIGNED)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  std::string query("INSERT INTO ut_test (name,b, t) VALUES (?,?, ?)");
  mysql_bulk_stmt stmt(query, log_v2::instance().get(log_v2::SQL));
  ms->prepare_statement(stmt);

  auto bb = stmt.create_bind();
  bb->reserve(200);
  constexpr int TOTAL = 200;

  for (int i = 0; i < TOTAL; i++) {
    ASSERT_EQ(bb->current_row(), i);
    bb->set_value_as_str(0, fmt::format("foo{}", i));
    bb->set_value_as_bool(1, (i % 2) == 0);
    bb->set_value_as_bool(2, (i % 2) == 1);
    bb->next_row();
  }
  stmt.set_bind(std::move(bb));
  ms->run_statement(stmt);
  ms->commit();

  std::string query4("SELECT id, value, b, t FROM ut_test LIMIT 5");
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  ms->run_query_and_get_result(query4, std::move(promise));
  mysql_result res = future.get();
  bool inside1 = false;
  while (ms->fetch_row(res)) {
    inside1 = true;
    ASSERT_TRUE(res.value_as_i32(2) <= 1);
    ASSERT_TRUE(res.value_as_i32(3) <= 1);
    ASSERT_NE(res.value_as_bool(2), res.value_as_bool(3));
  }
  ASSERT_TRUE(inside1);
}

struct row {
  using pointer = std::shared_ptr<row>;
  uint64_t id;
  std::string name;
  double value;
  char t;
  std::string e;
  int i;
  unsigned u;
};

static std::string row_filler(const row& data) {
  return fmt::format("('{}',{},{},'{}',{},{})", data.name, data.value,
                     int(data.t), data.e, data.i, data.u);
}

static std::string row_filler2(const row& data) {
  return fmt::format("({},'{}',{},{},'{}',{},{})", data.id, data.name,
                     data.value, int(data.t), data.e, data.i, data.u);
}

TEST_F(DatabaseStorageTest, MySqlMultiInsert) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e "
      "enum('a', "
      "'b', 'c') DEFAULT 'a', i INT, u INT UNSIGNED)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  //------------------------ insert test

  database::mysql_multi_insert inserter(
      "INSERT INTO ut_test (name, value, t, e, i, u) VALUES", "");

  unsigned data_index;

  for (data_index = 0; data_index < 1000000; ++data_index) {
    row to_insert = {.name = fmt::format("name_{}", data_index),
                     .value = double(data_index) / 10,
                     .t = char(data_index),
                     .e = std::string(1, 'a' + data_index % 3),
                     .i = int(data_index),
                     .u = data_index + 1};
    inserter.push(row_filler(to_insert));
  }

  std::chrono::system_clock::time_point start_insert =
      std::chrono::system_clock::now();
  unsigned nb_request = inserter.execute_queries(*ms);
  ms->commit();
  std::chrono::system_clock::time_point end_insert =
      std::chrono::system_clock::now();
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::SQL),
                     " insert {} rows in {} requests duration: {} seconds",
                     data_index, nb_request,
                     std::chrono::duration_cast<std::chrono::seconds>(
                         end_insert - start_insert)
                         .count());

  std::promise<mysql_result> select_prom;
  std::future<mysql_result> select_fut = select_prom.get_future();
  ms->run_query_and_get_result(
      "SELECT id, name, value, t,e,i,u FROM ut_test ORDER BY u",
      std::move(select_prom));
  mysql_result select_res = select_fut.get();

  ASSERT_EQ(select_res.get_rows_count(), 1000000);
  for (data_index = 0; data_index < 1000000; ++data_index) {
    ms->fetch_row(select_res);
    uint64_t select_au = select_res.value_as_u64(0);
    std::string name = select_res.value_as_str(1);
    ASSERT_EQ(name, fmt::format("name_{}", select_au - 1));
    double value = select_res.value_as_f64(2);
    ASSERT_EQ(value, double(select_au - 1) / 10);
    std::string e = select_res.value_as_str(4);
    ASSERT_EQ('a' + (select_au - 1) % 3, e[0]);
    int i = select_res.value_as_i64(5);
    ASSERT_EQ(select_au, i + 1);
    unsigned u = select_res.value_as_u64(6);
    ASSERT_EQ(select_au, u);
  }

  //-------------- insert or update test
  database::mysql_multi_insert inserter2(
      "INSERT INTO ut_test (id, name, value, t, e, i, u) VALUES",
      "ON DUPLICATE KEY UPDATE u = VALUES(u)");

  for (data_index = 0; data_index < 1000000; ++data_index) {
    row to_insert = {.id = data_index + 10,
                     .name = fmt::format("name_{}", data_index + 10),
                     .value = double(data_index + 10) / 10,
                     .t = char(data_index + 10),
                     .e = std::string(1, 'a' + (data_index + 10) % 3),
                     .i = int(data_index + 10),
                     .u = data_index + 10 - 10};
    inserter2.push(row_filler2(to_insert));
  }

  start_insert = std::chrono::system_clock::now();
  nb_request = inserter2.execute_queries(*ms);
  ms->commit();
  end_insert = std::chrono::system_clock::now();
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::SQL),
                     " insert {} rows in {} requests duration: {} seconds",
                     data_index, nb_request,
                     std::chrono::duration_cast<std::chrono::seconds>(
                         end_insert - start_insert)
                         .count());

  std::promise<mysql_result> select_prom2;
  std::future<mysql_result> select_fut2 = select_prom2.get_future();
  ms->run_query_and_get_result("SELECT id, name, value, t,e,i,u FROM ut_test",
                               std::move(select_prom2));
  select_res = select_fut2.get();

  ASSERT_EQ(select_res.get_rows_count(), 1000009);
  for (data_index = 0; data_index < 1000000; ++data_index) {
    ms->fetch_row(select_res);
    uint64_t select_au = select_res.value_as_u64(0);
    std::string name = select_res.value_as_str(1);
    ASSERT_EQ(name, fmt::format("name_{}", select_au - 1));
    double value = select_res.value_as_f64(2);
    ASSERT_EQ(value, double(select_au - 1) / 10);
    std::string e = select_res.value_as_str(4);
    ASSERT_EQ('a' + (select_au - 1) % 3, e[0]);
    int i = select_res.value_as_i64(5);
    ASSERT_EQ(select_au, i + 1);
    unsigned u = select_res.value_as_u64(6);
    if (select_au < 10) {
      ASSERT_EQ(select_au, u);
    } else {
      ASSERT_EQ(select_au, u + 10);
    }
  }
}

static unsigned event_binder_index = 0;

struct bulk_event_binder {
  void operator()(database::mysql_bulk_bind& binder) const {
    binder.set_value_as_str(0, fmt::format("toto{}", event_binder_index));
    binder.set_value_as_f64(1, 12.34 + event_binder_index);
    binder.set_value_as_tiny(2, (45 + event_binder_index) % 100);
    binder.set_value_as_str(3, "b");
    binder.set_value_as_i32(4, 678 + event_binder_index);
    binder.set_value_as_u32(5, 789 + event_binder_index);
    binder.next_row();
    ++event_binder_index;
  }
};

struct multi_event_binder {
  std::string operator()() const {
    std::string ret =
        fmt::format("('toto{}',{},{},'b',{},{})", event_binder_index,
                    12.34 + event_binder_index, (45 + event_binder_index) % 100,
                    678 + event_binder_index, 789 + event_binder_index);
    ++event_binder_index;
    return ret;
  }
};

TEST_F(DatabaseStorageTest, bulk_or_multi_bbdo_event_bulk) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e "
      "enum('a', "
      "'b', 'c') DEFAULT 'a', i INT, u INT UNSIGNED)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  auto inserter = std::make_unique<database::bulk_or_multi>(
      *ms, "INSERT INTO ut_test (name, value, t, e, i, u) VALUES (?,?,?,?,?,?)",
      100000, log_v2::instance().get(log_v2::SQL));

  auto begin = std::chrono::system_clock::now();
  event_binder_index = 0;

  bulk_event_binder binder;
  for (unsigned data_index = 0; data_index < 100000; ++data_index) {
    inserter->add_bulk_row(binder);
  }

  inserter->execute(*ms);
  ms->commit();

  log_v2::instance()
      .get(log_v2::SQL)
      ->info("100000 rows inserted in {} ms",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now() - begin)
                 .count());

  std::promise<mysql_result> select_prom;
  std::future<mysql_result> select_fut = select_prom.get_future();
  ms->run_query_and_get_result(
      "SELECT id, name, value, t,e,i,u FROM ut_test ORDER BY value",
      std::move(select_prom));
  mysql_result select_res = select_fut.get();

  ASSERT_EQ(select_res.get_rows_count(), 100000);
  for (unsigned data_index = 0; data_index < 100000; ++data_index) {
    ms->fetch_row(select_res);
    ASSERT_EQ(select_res.value_as_str(1), fmt::format("toto{}", data_index));
    ASSERT_EQ(select_res.value_as_f64(2), 12.34 + data_index);
    ASSERT_EQ(select_res.value_as_str(4), "b");
    ASSERT_EQ(select_res.value_as_i32(5), 678 + data_index);
    ASSERT_EQ(select_res.value_as_i32(6), 789 + data_index);
  }
}

TEST_F(DatabaseStorageTest, bulk_or_multi_bbdo_event_multi) {
  database_config db_cfg("MySQL", "127.0.0.1", MYSQL_SOCKET, 3306, "root",
                         "centreon", "centreon_storage", 5, true, 5);
  auto ms{std::make_unique<mysql>(db_cfg, log_v2::instance().get(log_v2::SQL))};
  std::string query1{"DROP TABLE IF EXISTS ut_test"};
  std::string query2{
      "CREATE TABLE ut_test (id BIGINT NOT NULL AUTO_INCREMENT "
      "PRIMARY KEY, name VARCHAR(1000), value DOUBLE, t TINYINT, e "
      "enum('a', "
      "'b', 'c') DEFAULT 'a', i INT, u INT UNSIGNED)"};
  ms->run_query(query1);
  ms->commit();
  ms->run_query(query2);
  ms->commit();

  auto inserter = std::make_unique<database::bulk_or_multi>(
      "INSERT INTO ut_test (name, value, t, e, i, u) VALUES", "");

  auto begin = std::chrono::system_clock::now();
  event_binder_index = 0;
  multi_event_binder filler;
  for (unsigned data_index = 0; data_index < 100000; ++data_index) {
    inserter->add_multi_row(filler);
  }

  inserter->execute(*ms);
  ms->commit();

  log_v2::instance()
      .get(log_v2::SQL)
      ->info("100000 rows inserted in {} ms",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now() - begin)
                 .count());

  std::promise<mysql_result> select_prom;
  std::future<mysql_result> select_fut = select_prom.get_future();
  ms->run_query_and_get_result(
      "SELECT id, name, value, t,e,i,u FROM ut_test ORDER by value",
      std::move(select_prom));
  mysql_result select_res = select_fut.get();

  ASSERT_EQ(select_res.get_rows_count(), 100000);
  for (unsigned data_index = 0; data_index < 100000; ++data_index) {
    ms->fetch_row(select_res);
    ASSERT_EQ(select_res.value_as_str(1), fmt::format("toto{}", data_index));
    ASSERT_EQ(select_res.value_as_f64(2), 12.34 + data_index);
    ASSERT_EQ(select_res.value_as_str(4), "b");
    ASSERT_EQ(select_res.value_as_i32(5), 678 + data_index);
    ASSERT_EQ(select_res.value_as_i32(6), 789 + data_index);
  }
}
