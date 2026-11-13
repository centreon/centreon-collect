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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <chrono>

#include <grpcpp/create_channel.h>
#include <fstream>
#include <thread>

#include "com/centreon/engine/host.hh"

#include "com/centreon/engine/enginerpc.hh"

#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/configuration/applier/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "com/centreon/engine/version.hh"
#include "common/engine_conf/hostgroup_helper.hh"
#include "engine.grpc.pb.h"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class EngineRpc : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();

    // Do not unload this in the tear down function, it is done by the
    // other unload function... :-(

    pb_config.set_execute_service_checks(true);

    configuration::error_cnt err;
    /* contact */
    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);

    /* hosts */
    configuration::Host hst_child;
    configuration::host_helper hst_child_hlp(&hst_child);
    configuration::applier::host hst_aply2;
    hst_child.set_host_name("child_host");
    hst_child.set_address("127.0.0.1");
    hst_child_hlp.hook("parents", "test_host");
    hst_child.set_host_id(42);
    hst_child_hlp.set_default_values();
    hst_aply2.add_object(hst_child);

    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst = new_pb_configuration_host("test_host", "admin");
    configuration::applier::host hst_aply;
    hst.set_host_id(12);
    hst_hlp.set_default_values();
    hst_aply.add_object(hst);

    hst_aply.resolve_object(hst, err);
    hst_aply2.resolve_object(hst_child, err);

    ASSERT_EQ(engine::host::hosts.size(), 2u);

    host_map::iterator child = engine::host::hosts.find("child_host");
    host_map::iterator parent = engine::host::hosts.find("test_host");

    ASSERT_EQ(child->second->parent_hosts.size(), 1u);
    ASSERT_EQ(parent->second->child_hosts.size(), 1u);

    /* hostgroup */
    configuration::Hostgroup hg;
    configuration::hostgroup_helper hg_hlp(&hg);
    configuration::applier::hostgroup hg_aply;
    hg.set_hostgroup_name("test_hg");
    hg_hlp.hook("members", "test_host");
    hg_aply.add_object(hg);
    hg_aply.expand_objects(pb_config);
    hg_aply.resolve_object(hg, err);

    /* service */
    configuration::Service svc{
        new_pb_configuration_service("test_host", "test_svc", "admin")};
    configuration::Command cmd;
    configuration::command_helper cmd_hlp(&cmd);
    cmd.set_command_name("cmd");
    cmd.set_command_line("/bin/sh -c 'echo \"test_cmd\"'");
    svc.set_check_command("cmd");
    configuration::applier::command cmd_aply;

    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);
    cmd_aply.add_object(cmd);

    svc_aply.resolve_object(svc, err);

    configuration::Anomalydetection ad{new_pb_configuration_anomalydetection(
        "test_host", "test_ad", "admin",
        12,  // service_id of the anomalydetection
        13,  // service_id of the dependent service
        "/tmp/thresholds_status_change.json")};
    configuration::applier::anomalydetection ad_aply;
    ad_aply.add_object(ad);

    ad_aply.resolve_object(ad, err);

    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_down);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));

    service_map const& sm{engine::service::services};
    for (auto& p : sm) {
      std::shared_ptr<engine::service> svc = p.second;
      if (svc->service_id() == 12)
        _ad = std::static_pointer_cast<engine::anomalydetection>(svc);
      else
        _svc = svc;
    }
    _svc->set_current_state(engine::service::state_critical);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));

    contact_map const& cm{engine::contact::contacts};
    _contact = cm.begin()->second;

    /* servicegroup */
    configuration::Servicegroup sg;
    configuration::servicegroup_helper sg_hlp(&sg);
    sg.set_servicegroup_name("test_sg");
    configuration::applier::servicegroup sg_aply;
    sg_hlp.hook("members", "test_host,test_svc");

    sg_aply.add_object(sg);
    sg_aply.expand_objects(pb_config);
    sg_aply.resolve_object(sg, err);
  }

  void TearDown() override {
    _host.reset();
    _svc.reset();
    _ad.reset();
    deinit_config_state();
  }

  std::list<std::string> execute(const std::string& command) {
    std::list<std::string> retval;
    char path[1024];
    std::ostringstream oss;
    oss << "tests/rpc_client_engine " << command;

    FILE* fp = popen(oss.str().c_str(), "r");
    while (fgets(path, sizeof(path), fp) != nullptr) {
      size_t count = strlen(path);
      if (count > 0)
        --count;
      retval.push_back(std::string(path, count));
    }
    pclose(fp);
    return retval;
  }

  void CreateFile(std::string const& filename, std::string const& content) {
    std::ofstream oss(filename);
    oss << content;
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::contact> _contact;
  std::shared_ptr<engine::service> _svc;
  std::shared_ptr<engine::anomalydetection> _ad;
};

/* calls command manager in another thread (function is used for unit tests)
 */
static void call_command_manager(std::unique_ptr<std::thread>& th,
                                 std::condition_variable* condvar,
                                 std::mutex* mutex,
                                 bool* continuerunning) {
  auto fn = [continuerunning, mutex, condvar]() {
    std::unique_lock<std::mutex> lock(*mutex);
    for (;;) {
      command_manager::instance().execute();
      if (condvar->wait_for(
              lock, std::chrono::milliseconds(50),
              [continuerunning]() -> bool { return *continuerunning; })) {
        break;
      }
    }
  };

  th.reset(new std::thread(fn));
}

TEST_F(EngineRpc, StartStop) {
  enginerpc erpc("0.0.0.0", 40001);
  ASSERT_NO_THROW(erpc.shutdown());
}

TEST_F(EngineRpc, GetVersion) {
  std::ostringstream oss;
  oss << "GetVersion: major: " << CENTREON_ENGINE_VERSION_MAJOR;
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("GetVersion");
  ASSERT_EQ(output.front(), oss.str());
  if (output.size() == 2u) {
    oss.str("");
    oss << "minor: " << CENTREON_ENGINE_VERSION_MINOR;
    ASSERT_EQ(output.back(), oss.str());
  } else {
    oss.str("");
    oss << "patch: " << CENTREON_ENGINE_VERSION_PATCH;
    ASSERT_EQ(output.back(), oss.str());
  }
  erpc.shutdown();
}

TEST_F(EngineRpc, GetHost) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  std::vector<std::string> vectests = {
      "GetHost",
      fmt::format("Host name: {}", _host->name()),
      fmt::format("Host alias: {}", _host->get_alias()),
      fmt::format("Host id: {}", _host->host_id()),
      "Host address: 127.0.0.1",
      "Host state: 1",
      "Host period: test_period"};
  _host->set_current_state(engine::host::state_down);
  _host->set_check_period("test_period");
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(fmt::format("GetHost byhostid {}", _host->host_id()));
  auto output2 = execute(fmt::format("GetHost byhostname {}", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> result_ids(output.size());
  std::copy(output.begin(), output.end(), result_ids.begin());

  ASSERT_EQ(vectests, result_ids);

  std::vector<std::string> result_names(output2.size());
  std::copy(output2.begin(), output2.end(), result_names.begin());

  ASSERT_EQ(vectests, result_names);
  erpc.shutdown();
}

TEST_F(EngineRpc, GetWrongHost) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  std::vector<std::string> vectests = {"GetHostByHostName rpc engine failed",
                                       "GetHost",
                                       "Host name: ",
                                       "Host alias: ",
                                       "Host id: 0",
                                       "Host address: ",
                                       "Host state: 0",
                                       "Host period: "};
  _host->set_current_state(engine::host::state_down);
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetHost byhostname wrong_host");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> result_names(output.size());
  std::copy(output.begin(), output.end(), result_names.begin());

  ASSERT_EQ(vectests, result_names);
  erpc.shutdown();
}

TEST_F(EngineRpc, GetService) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  std::vector<std::string> vectests = {"GetService",
                                       "Host id: 12",
                                       "Service id: 13",
                                       "Host name: test_host",
                                       "Serv desc: test_svc",
                                       "Service state: 2",
                                       "Service period: test_period"};
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_check_period("test_period");
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetService bynames test_host test_svc");
  auto output2 = execute("GetService byids 12 13");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> result_names(output.size());
  std::copy(output.begin(), output.end(), result_names.begin());

  ASSERT_EQ(vectests, result_names);

  std::vector<std::string> result_ids(output2.size());
  std::copy(output2.begin(), output2.end(), result_ids.begin());

  ASSERT_EQ(vectests, result_ids);
  erpc.shutdown();
}

TEST_F(EngineRpc, GetWrongService) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  std::vector<std::string> vectests = {"GetService rpc engine failed",
                                       "GetService",
                                       "Host id: 0",
                                       "Service id: 0",
                                       "Host name: ",
                                       "Serv desc: ",
                                       "Service state: 0",
                                       "Service period: "};

  _svc->set_current_state(engine::service::state_critical);
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetService bynames wrong_host wrong_svc");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> result_names(output.size());
  std::copy(output.begin(), output.end(), result_names.begin());

  ASSERT_EQ(vectests, result_names);
  erpc.shutdown();
}

TEST_F(EngineRpc, GetContact) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  std::vector<std::string> vectests = {"GetContact", "admin", "admin",
                                       "admin@centreon.com"};
  _contact->set_email("admin@centreon.com");

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetContact admin");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> result_names(output.size());
  std::copy(output.begin(), output.end(), result_names.begin());

  ASSERT_EQ(vectests, result_names);
  erpc.shutdown();
}

TEST_F(EngineRpc, GetWrongContact) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  std::vector<std::string> vectests = {"GetContact rpc engine failed",
                                       "GetContact", "", "", ""};

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetContact wrong_contactadmin");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> result_names(output.size());
  std::copy(output.begin(), output.end(), result_names.begin());

  ASSERT_EQ(vectests, result_names);
  erpc.shutdown();
}

TEST_F(EngineRpc, GetHostsCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetHostsCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "2");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetContactsCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetContactsCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "1");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetServicesCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetServicesCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "2");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetServiceGroupsCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetServiceGroupsCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "1");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetContactGroupsCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetContactGroupsCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "0");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetHostGroupsCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetHostGroupsCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "1");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetServiceDependenciesCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetServiceDependenciesCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "0");
  erpc.shutdown();
}

TEST_F(EngineRpc, GetHostDependenciesCount) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("GetHostDependenciesCount");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.back(), "0");
  erpc.shutdown();
}

TEST_F(EngineRpc, AddHostComment) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(comment::comments.size(), 0u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output =
      execute("AddHostComment test_host test-admin mycomment 1 10000");
  ASSERT_EQ(comment::comments.size(), 1u);

  output = execute("DeleteComment 1");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, AddServiceComment) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(comment::comments.size(), 0u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(
      "AddServiceComment test_host test_svc test-admin mycomment 1 10000");
  ASSERT_EQ(comment::comments.size(), 1u);

  output = execute("DeleteComment 1");
  ASSERT_EQ(comment::comments.size(), 0u);
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, DeleteComment) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(comment::comments.size(), 0u);
  // create comment
  std::ostringstream oss;
  oss << "my comment ";
  auto cmt = std::make_shared<comment>(
      comment::host, comment::user, _host->host_id(), 0, 10000, "test-admin",
      oss.str(), true, comment::external, false, 0);
  comment::comments.insert({cmt->get_comment_id(), cmt});

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("DeleteComment 1");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, DeleteWrongComment) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  std::vector<std::string> vectests{
      "DeleteComment failed.",
      "DeleteComment 0",
  };
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("DeleteComment 999");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  std::vector<std::string> results(output.size());
  std::copy(output.begin(), output.end(), results.begin());

  ASSERT_EQ(vectests, results);
  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, DeleteAllHostComments) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  // first test
  ASSERT_EQ(comment::comments.size(), 0u);
  // create some comments
  for (int i = 0; i < 10; ++i) {
    std::string cmt_str{fmt::format("my host comment {}", i)};
    auto cmt = std::make_shared<comment>(
        comment::host, comment::user, _host->host_id(), 0, 10000, "test-admin",
        cmt_str, true, comment::external, false, 0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
  }
  ASSERT_EQ(comment::comments.size(), 10u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(
      fmt::format("DeleteAllHostComments byhostid {}", _host->host_id()));

  ASSERT_EQ(comment::comments.size(), 0u);
  // second test
  for (int i = 0; i < 10; ++i) {
    std::string cmt_str{fmt::format("my host comment {}", i)};
    auto cmt = std::make_shared<comment>(
        comment::host, comment::user, _host->host_id(), 0, 10000, "test-admin",
        cmt_str, true, comment::external, false, 0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
  }
  ASSERT_EQ(comment::comments.size(), 10u);
  output = execute(
      fmt::format("DeleteAllHostComments byhostname {}", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, DeleteAllServiceComments) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  auto svc = _svc;
  auto hit = engine::host::hosts_by_id.find(svc->host_id());
  auto hst = hit->second;

  // first test
  ASSERT_EQ(comment::comments.size(), 0u);
  // create some comments
  for (int i = 0; i < 10; ++i) {
    std::string cmt_str{fmt::format("my service comment {} on service ({}, {})",
                                    i, svc->host_id(), svc->service_id())};
    auto cmt = std::make_shared<comment>(
        comment::service, comment::user, svc->host_id(), svc->service_id(),
        10000, "test-admin", cmt_str, true, comment::external, false, 0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
  }
  ASSERT_EQ(comment::comments.size(), 10u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(fmt::format("DeleteAllServiceComments byids {} {}",
                                    svc->host_id(), svc->service_id()));

  ASSERT_EQ(comment::comments.size(), 0u);
  // second test
  for (int i = 0; i < 10; ++i) {
    std::string cmt_str{fmt::format("my service comment {}", i)};
    auto cmt = std::make_shared<comment>(
        comment::service, comment::user, svc->host_id(), svc->service_id(),
        10000, "test-admin", cmt_str, true, comment::external, false, 0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
  }
  ASSERT_EQ(comment::comments.size(), 10u);
  output = execute(fmt::format("DeleteAllServiceComments bynames {} {}",
                               hst->name(), svc->description()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, RemoveHostAcknowledgement) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;
  oss << "my comment ";
  // first test
  _host->set_acknowledgement(AckType::NORMAL);
  // create comment
  auto cmt = std::make_shared<comment>(
      comment::host, comment::acknowledgment, _host->host_id(), 0, 10000,
      "test-admin", oss.str(), false, comment::external, false, 0);
  comment::comments.insert({cmt->get_comment_id(), cmt});

  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(
      fmt::format("RemoveHostAcknowledgement byhostid {}", _host->host_id()));

  ASSERT_EQ(_host->problem_has_been_acknowledged(), false);
  ASSERT_EQ(comment::comments.size(), 0u);
  // second test
  _host->set_acknowledgement(AckType::NORMAL);
  cmt = std::make_shared<comment>(
      comment::host, comment::acknowledgment, _host->host_id(), 0, 10000,
      "test-admin", oss.str(), false, comment::external, false, 0);
  comment::comments.insert({cmt->get_comment_id(), cmt});

  output = execute(
      fmt::format("RemoveHostAcknowledgement byhostname {}", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(_host->problem_has_been_acknowledged(), false);
  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, RemoveServiceAcknowledgement) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  auto svc = _svc;
  auto hit = engine::host::hosts_by_id.find(svc->host_id());
  auto hst = hit->second;
  std::string ack_str{"my comment"};
  _svc->set_acknowledgement(AckType::NORMAL);
  auto cmt = std::make_shared<comment>(
      comment::service, comment::acknowledgment, hst->host_id(),
      svc->service_id(), 10000, "test-admin", ack_str, false, comment::external,
      false, 0);
  comment::comments.insert({cmt->get_comment_id(), cmt});

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output =
      execute("RemoveServiceAcknowledgement bynames test_host test_svc");

  ASSERT_EQ(comment::comments.size(), 0u);
  ASSERT_EQ(svc->problem_has_been_acknowledged(), false);

  svc->set_acknowledgement(AckType::NORMAL);
  cmt = std::make_shared<comment>(comment::service, comment::acknowledgment,
                                  hst->host_id(), svc->service_id(), 10000,
                                  "test-admin", ack_str, false,
                                  comment::external, false, 0);
  comment::comments.insert({cmt->get_comment_id(), cmt});

  output = execute("RemoveServiceAcknowledgement byids 12 13");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(comment::comments.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, AcknowledgementHostProblem) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(_host->problem_has_been_acknowledged(), false);
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(fmt::format(
      "AcknowledgementHostProblem {} admin test 1 0 0", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(_host->problem_has_been_acknowledged(), true);
  erpc.shutdown();
}

TEST_F(EngineRpc, AcknowledgementServiceProblem) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(_svc->problem_has_been_acknowledged(), false);
  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(
      "AcknowledgementServiceProblem test_host test_svc admin test 1 0 0");
  ;
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(_svc->problem_has_been_acknowledged(), true);
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleHostDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleHostDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  // we fake a wrong test with an undefined parameter
  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleHostDowntime 0", output.back());
  oss.str("");

  // we make the right test
  oss << "ScheduleHostDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
  ASSERT_EQ("ScheduleHostDowntime 1", output.back());

  // deleting the current downtime
  uint64_t id = downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .begin()
                    ->second->get_downtime_id();
  oss.str("");
  oss << "DeleteDowntime " << id;
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleWrongHostDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleHostDowntime test_host " << now + 1 << " " << now
      << " 0 0 10000 admin host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  // we fake a wrong test with an
  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleHostDowntime 0", output.back());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleServiceDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleServiceDowntime test_host test_svc " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleServiceDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleServiceDowntime test_host test_svc " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(2u, downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .size());  // one for service and one for ano
  ASSERT_EQ("ScheduleServiceDowntime 1", output.back());

  oss.str("");
  uint64_t id = downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .begin()
                    ->second->get_downtime_id();
  oss << "DeleteDowntime " << id;
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleWrongServiceDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleServiceDowntime test_host test_svc " << now + 1 << " " << now
      << " 0 0 10000 admin host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleServiceDowntime 0", output.back());
  oss.str("");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleHostServicesDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  std::ostringstream oss2;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleHostServicesDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleHostServicesDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleHostServicesDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(3u, downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .size());  // one for service and one for ano
  ASSERT_EQ("ScheduleHostServicesDowntime 1", output.back());

  oss2 << "DeleteServiceDowntimeFull test_host undef undef undef"
          " undef undef undef undef undef";

  output = execute(oss2.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();
  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleHostGroupHostsDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleHostGroupHostsDowntime test_hg " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleHostGroupHostsDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleHostGroupHostsDowntime test_hg " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
  uint64_t id = downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .begin()
                    ->second->get_downtime_id();
  oss.str("");
  oss << "DeleteDowntime " << id;
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleHostGroupServicesDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  set_time(20000);
  time_t now = time(nullptr);

  oss << "ScheduleHostGroupServicesDowntime test_hg " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleHostGroupServicesDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleHostGroupServicesDowntime test_hg " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(3u, downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .size());  // one for service and one for ano
  ASSERT_EQ("ScheduleHostGroupServicesDowntime 1", output.back());

  oss.str("");
  oss << "DeleteServiceDowntimeFull test_host undef undef undef"
         " undef undef undef undef undef";
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleServiceGroupHostsDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  std::ostringstream oss2;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  set_time(20000);
  time_t now = time(nullptr);
  oss << "ScheduleServiceGroupHostsDowntime test_sg " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleServiceGroupHostsDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleServiceGroupHostsDowntime test_sg " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
  ASSERT_EQ("ScheduleServiceGroupHostsDowntime 1", output.back());

  // deleting current downtime
  uint64_t id = downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .begin()
                    ->second->get_downtime_id();
  oss.str("");
  oss << "DeleteDowntime " << id;
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleServiceGroupServicesDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  std::ostringstream oss2;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  set_time(20000);
  time_t now = time(nullptr);
  oss << "ScheduleServiceGroupServicesDowntime test_sg " << now << " "
      << now + 1 << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleServiceGroupServicesDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleServiceGroupServicesDowntime test_sg " << now << " "
      << now + 1 << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(2u, downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .size());  // one for service and one for ano
  ASSERT_EQ("ScheduleServiceGroupServicesDowntime 1", output.back());

  uint64_t id = downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .begin()
                    ->second->get_downtime_id();
  oss2 << "DeleteDowntime " << id;
  output = execute(oss2.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleAndPropagateHostDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);
  oss << "ScheduleAndPropagateHostDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 undef host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ("ScheduleAndPropagateHostDowntime 0", output.back());
  oss.str("");

  oss << "ScheduleAndPropagateHostDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(2u, downtime_manager::instance().get_scheduled_downtimes().size());
  ASSERT_EQ("ScheduleAndPropagateHostDowntime 1", output.back());

  oss.str("");
  oss << "DeleteDowntimeByHostName test_host undef undef undef";
  output = execute(oss.str());

  oss.str("");
  oss << "DeleteDowntimeByHostName child_host undef undef undef";
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, ScheduleAndPropagateTriggeredHostDowntime) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  std::ostringstream oss;
  std::ostringstream oss2;
  bool continuerunning = false;

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());

  set_time(20000);
  time_t now = time(nullptr);
  oss << "ScheduleHostDowntime test_host " << now << " " << now + 1
      << " 0 0 10000 admin host " << now;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(oss.str());
  ASSERT_EQ(1u, downtime_manager::instance().get_scheduled_downtimes().size());
  uint64_t id = downtime_manager::instance()
                    .get_scheduled_downtimes()
                    .begin()
                    ->second->get_downtime_id();
  oss.str("");
  oss << "ScheduleAndPropagateTriggeredHostDowntime test_host " << now << " "
      << now + 1 << " 0 " << id << " 10000 admin host " << now;
  output = execute(oss.str());
  ASSERT_EQ(3u, downtime_manager::instance().get_scheduled_downtimes().size());

  oss.str("");
  oss << "DeleteDowntime " << id;
  output = execute(oss.str());
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(0u, downtime_manager::instance().get_scheduled_downtimes().size());
  erpc.shutdown();
}

TEST_F(EngineRpc, DelayHostNotification) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  auto hst = engine::host::hosts_by_id.find(12)->second;

  ASSERT_EQ(hst->get_next_notification(), 0);

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("DelayHostNotification byhostid 12 20");
  ASSERT_EQ(hst->get_next_notification(), 20);

  output = execute(
      fmt::format("DelayHostNotification byhostname {} 10", hst->name()));
  ASSERT_EQ(hst->get_next_notification(), 10);
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, DelayServiceNotification) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(_host->get_next_notification(), 0);

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("DelayServiceNotification byids 12 13 20");
  ASSERT_EQ(_svc->get_next_notification(), 20);

  output = execute("DelayServiceNotification bynames test_host test_svc 10");
  ASSERT_EQ(_svc->get_next_notification(), 10);
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeHostObjectIntVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output =
      execute(fmt::format("ChangeHostObjectIntVar {} 0 1 1.0", _host->name()));
  ASSERT_EQ(_host->check_interval(), 1u);
  output =
      execute(fmt::format("ChangeHostObjectIntVar {} 1 1 2.0", _host->name()));
  ASSERT_EQ(_host->retry_interval(), 2u);
  output =
      execute(fmt::format("ChangeHostObjectIntVar {} 2 1 1.0", _host->name()));
  ASSERT_EQ(_host->max_check_attempts(), 1);
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();
  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeServiceObjectIntVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(
      "ChangeServiceObjectIntVar"
      " test_host test_svc 0 1 1.0");
  ASSERT_EQ(_svc->check_interval(), 1u);
  output = execute(
      "ChangeServiceObjectIntVar"
      " test_host test_svc 1 1 2.0");
  ASSERT_EQ(_svc->retry_interval(), 2u);
  output = execute(
      "ChangeServiceObjectIntVar"
      " test_host test_svc 2 1 1.0");
  ASSERT_EQ(_svc->max_check_attempts(), 1);
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeContactObjectIntVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(
      "ChangeContactObjectIntVar"
      " admin 0 1 1.0");
  ASSERT_EQ(_contact->get_modified_attributes(), 1u);
  output = execute(
      "ChangeContactObjectIntVar"
      " admin 1 2 1.0");
  ASSERT_EQ(_contact->get_modified_host_attributes(), 2u);
  output = execute(
      "ChangeContactObjectIntVar"
      " admin 2 3 1.0");
  ASSERT_EQ(_contact->get_modified_service_attributes(), 3u);
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeHostObjectCharVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("ChangeHostObjectCharVar null 0 cmd");
  ASSERT_EQ(output.back(), "ChangeHostObjectCharVar 1");
  output =
      execute(fmt::format("ChangeHostObjectCharVar {} 1 cmd", _host->name()));
  ASSERT_EQ(_host->event_handler(), "cmd");
  output =
      execute(fmt::format("ChangeHostObjectCharVar {} 2 cmd", _host->name()));
  ASSERT_EQ(_host->check_command(), "cmd");
  output =
      execute(fmt::format("ChangeHostObjectCharVar {} 3 24x7", _host->name()));
  ASSERT_EQ(_host->check_period(), "24x7");
  output =
      execute(fmt::format("ChangeHostObjectCharVar {} 4 24x7", _host->name()));
  ASSERT_EQ(_host->notification_period(), "24x7");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeServiceObjectCharVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  auto svc = _svc;
  auto hit = engine::host::hosts_by_id.find(svc->host_id());
  auto hst = hit->second;

  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute("ChangeServiceObjectCharVar null null 0 cmd");
  ASSERT_EQ(output.back(), "ChangeServiceObjectCharVar 1");
  output = execute(fmt::format("ChangeServiceObjectCharVar {} {} 1 cmd",
                               hst->name(), svc->description()));
  ASSERT_EQ(_svc->event_handler(), "cmd");
  output = execute(fmt::format("ChangeServiceObjectCharVar {} {} 2 cmd",
                               hst->name(), svc->description()));
  ASSERT_EQ(_svc->check_command(), "cmd");
  output = execute(fmt::format("ChangeServiceObjectCharVar {} {} 3 24x7",
                               hst->name(), svc->description()));
  ASSERT_EQ(_svc->check_period(), "24x7");
  output = execute(fmt::format("ChangeServiceObjectCharVar {} {} 4 24x7",
                               hst->name(), svc->description()));
  ASSERT_EQ(_svc->notification_period(), "24x7");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeContactObjectCharVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(engine::timeperiod::timeperiods.size(), 1u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);

  auto output = execute(
      "ChangeContactObjectCharVar"
      " admin 0 24x7");
  ASSERT_EQ(_contact->get_host_notification_period(), "24x7");
  output = execute(
      "ChangeContactObjectCharVar"
      " admin 1 24x7");
  ASSERT_EQ(_contact->get_service_notification_period(), "24x7");

  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeHostObjectCustomVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  ASSERT_EQ(_host->custom_variables.size(), 0u);
  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(fmt::format(
      "ChangeHostObjectCustomVar {} test_var test_val", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(_host->custom_variables.size(), 1u);
  ASSERT_EQ(_host->custom_variables["TEST_VAR"].value(), "test_val");
  _host->custom_variables.clear();
  ASSERT_EQ(_host->custom_variables.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeServiceObjectCustomVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  _svc->custom_variables.clear();
  ASSERT_EQ(_svc->custom_variables.size(), 0u);
  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(
      "ChangeServiceObjectCustomVar"
      " test_host test_svc test_var test_val");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(_svc->custom_variables.size(), 1u);
  ASSERT_EQ(_svc->custom_variables["TEST_VAR"].value(), "test_val");
  _svc->custom_variables.clear();
  ASSERT_EQ(_svc->custom_variables.size(), 0u);
  erpc.shutdown();
}

TEST_F(EngineRpc, ChangeContactObjectCustomVar) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;
  ASSERT_EQ(_contact->get_custom_variables().size(), 0u);

  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(
      "ChangeContactObjectCustomVar"
      " admin test_var test_val");
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();
  ASSERT_EQ(_contact->get_custom_variables().size(), 1u);
  ASSERT_EQ(_contact->get_custom_variables()["TEST_VAR"].value(), "test_val");

  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessServiceCheckResult) {
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("ProcessServiceCheckResult test_host test_svc 0");
  ASSERT_EQ(output.size(), 1u);
  ASSERT_EQ(output.front(), "ProcessServiceCheckResult: 0");
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessServiceCheckResultBadHost) {
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("ProcessServiceCheckResult \"\" test_svc 0");
  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(), "ProcessServiceCheckResult failed.");
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessServiceCheckResultBadService) {
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("ProcessServiceCheckResult test_host \"\" 0");
  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(), "ProcessServiceCheckResult failed.");
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessHostCheckResult) {
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("ProcessHostCheckResult test_host 0");
  ASSERT_EQ(output.size(), 1u);
  ASSERT_EQ(output.front(), "ProcessHostCheckResult: 0");
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessHostCheckResultBadHost) {
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("ProcessHostCheckResult '' 0");
  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(), "ProcessHostCheckResult failed.");
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessExternalCommand) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  _host->set_checks_enabled(true);
  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output =
      execute(fmt::format("ProcessExternalCommand '[1700000000] "
                          "DISABLE_HOST_CHECK;{}'",
                          _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.size(), 1u);
  ASSERT_EQ(output.front(), "ProcessExternalCommand: 0");
  ASSERT_FALSE(_host->active_checks_enabled());
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessExternalCommandEmpty) {
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("ProcessExternalCommand ''");
  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(),
            "ProcessExternalCommand failed: 3 command must not be empty");
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessExternalCommandSeveralLines) {
  enginerpc erpc("0.0.0.0", 40001);
  _host->set_checks_enabled(true);
  auto output = execute(fmt::format(
      "ProcessExternalCommand '[1700000000] DISABLE_HOST_CHECK;{0}\n"
      "[1700000000] DISABLE_HOST_CHECK;{0}'",
      _host->name()));
  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(),
            "ProcessExternalCommand failed: 3 command must be a single line "
            "without control characters");
  command_manager::instance().execute();
  ASSERT_TRUE(_host->active_checks_enabled());
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessExternalCommandMalformed) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  _host->set_checks_enabled(true);
  call_command_manager(th, &condvar, &mutex, &continuerunning);
  // No "[<timestamp>] " prefix.
  auto output = execute(fmt::format(
      "ProcessExternalCommand 'DISABLE_HOST_CHECK;{}'", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(),
            "ProcessExternalCommand failed: 3 command is malformed or "
            "unknown, expected format: \"[<timestamp>] "
            "<COMMAND_NAME>;<arguments>\"");
  ASSERT_TRUE(_host->active_checks_enabled());
  erpc.shutdown();
}

TEST_F(EngineRpc, ProcessExternalCommandUnknown) {
  enginerpc erpc("0.0.0.0", 40001);
  std::unique_ptr<std::thread> th;
  std::condition_variable condvar;
  std::mutex mutex;
  bool continuerunning = false;

  call_command_manager(th, &condvar, &mutex, &continuerunning);
  auto output = execute(fmt::format(
      "ProcessExternalCommand '[1700000000] NOT_A_COMMAND;{}'", _host->name()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    continuerunning = true;
  }
  condvar.notify_one();
  th->join();

  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(),
            "ProcessExternalCommand failed: 3 command is malformed or "
            "unknown, expected format: \"[<timestamp>] "
            "<COMMAND_NAME>;<arguments>\"");
  erpc.shutdown();
}

TEST_F(EngineRpc, NewThresholdsFile) {
  CreateFile(
      "/tmp/thresholds_file.json",
      "[{\n \"host_id\": \"12\",\n \"service_id\": \"12\",\n \"metric_name\": "
      "\"metric\",\n \"predict\": [{\n \"timestamp\": 50000,\n \"upper\": "
      "84,\n \"lower\": 74,\n \"fit\": 79\n }, {\n \"timestamp\": 100000,\n "
      "\"upper\": 10,\n \"lower\": 5,\n \"fit\": 51.5\n }, {\n \"timestamp\": "
      "150000,\n \"upper\": 100,\n \"lower\": 93,\n \"fit\": 96.5\n }, {\n "
      "\"timestamp\": 200000,\n \"upper\": 100,\n \"lower\": 97,\n \"fit\": "
      "98.5\n }, {\n \"timestamp\": 250000,\n \"upper\": 100,\n \"lower\": "
      "21,\n \"fit\": 60.5\n }\n]}]");
  enginerpc erpc("0.0.0.0", 40001);
  auto output = execute("NewThresholdsFile /tmp/thresholds_file.json");
  ASSERT_EQ(output.size(), 1u);
  ASSERT_EQ(output.front(), "NewThresholdsFile: 0");
  command_manager::instance().execute();
  ASSERT_EQ(_ad->get_thresholds_file(), "/tmp/thresholds_file.json");
}

TEST(enginerpc_test, tcp_address_gets_the_port) {
  ASSERT_FALSE(enginerpc::is_unix_socket("localhost"));
  ASSERT_EQ(enginerpc::listening_address("localhost", 50155),
            "localhost:50155");
  ASSERT_EQ(enginerpc::listening_address("0.0.0.0", 40001), "0.0.0.0:40001");
}

TEST(enginerpc_test, unix_socket_address_is_used_as_is) {
  ASSERT_TRUE(enginerpc::is_unix_socket("unix:/run/engine.sock"));
  ASSERT_EQ(enginerpc::listening_address("unix:/run/engine.sock", 50155),
            "unix:/run/engine.sock");
}

TEST(enginerpc_test, host_name_starting_with_unix_is_tcp) {
  ASSERT_FALSE(enginerpc::is_unix_socket("unix"));
  ASSERT_EQ(enginerpc::listening_address("unixhost", 50155), "unixhost:50155");
}

/* GetVersion through the Unix domain socket of the server. */
static grpc::Status get_version_on(const std::string& address) {
  auto stub = com::centreon::engine::Engine::NewStub(
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(10));
  const ::google::protobuf::Empty e;
  com::centreon::engine::Version version;
  return stub->GetVersion(&context, e, &version);
}

TEST_F(EngineRpc, UnixSocketAnswersWithOwnerAndGroupMode) {
  const std::string path =
      fmt::format("/tmp/ut_engine_rpc_mode_{}.sock", getpid());
  const std::string address = "unix:" + path;
  unlink(path.c_str());
  mode_t mask = umask(022);
  {
    enginerpc erpc(address, 40001);
    ASSERT_TRUE(erpc.listening());
    struct stat st;
    ASSERT_EQ(stat(path.c_str(), &st), 0);
    ASSERT_TRUE(S_ISSOCK(st.st_mode));
    ASSERT_EQ(st.st_mode & 07777, 0660u);
    grpc::Status status = get_version_on(address);
    ASSERT_TRUE(status.ok()) << status.error_message();
    erpc.shutdown();
  }
  // The umask of the process is restored once the server is started.
  ASSERT_EQ(umask(mask), 022u);
  unlink(path.c_str());
}

TEST_F(EngineRpc, UnixSocketReplacesStaleSocket) {
  const std::string path =
      fmt::format("/tmp/ut_engine_rpc_stale_{}.sock", getpid());
  const std::string address = "unix:" + path;
  unlink(path.c_str());
  // A socket file nobody listens on, as a killed Engine leaves it.
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_GE(fd, 0);
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
  ASSERT_EQ(bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)),
            0);
  close(fd);
  struct stat st;
  ASSERT_EQ(stat(path.c_str(), &st), 0);

  enginerpc erpc(address, 40001);
  ASSERT_TRUE(erpc.listening());
  grpc::Status status = get_version_on(address);
  ASSERT_TRUE(status.ok()) << status.error_message();
  erpc.shutdown();
  unlink(path.c_str());
}

TEST_F(EngineRpc, UnixSocketDoesNotReplaceRegularFile) {
  const std::string path =
      fmt::format("/tmp/ut_engine_rpc_file_{}.sock", getpid());
  CreateFile(path, "not a socket");

  enginerpc erpc("unix:" + path, 40001);
  ASSERT_FALSE(erpc.listening());
  struct stat st;
  ASSERT_EQ(stat(path.c_str(), &st), 0);
  ASSERT_TRUE(S_ISREG(st.st_mode));
  erpc.shutdown();
  unlink(path.c_str());
}
