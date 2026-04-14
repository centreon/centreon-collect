/**
 * Copyright 2023 Centreon (https://www.centreon.com/)
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

#include "bbdo/neb.pb.h"

#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/log_v2/log_v2.hh"
#include "storage/metric_mapping.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

static std::string to_string(const com::centreon::broker::cache::string& src) {
  return std::string(src.c_str(), src.length());
}

using namespace com::centreon::broker;
using namespace com::centreon::broker::cache;

using log_v2 = com::centreon::common::log_v2::log_v2;

class global_cache_test : public testing::Test {
 public:
  static void SetUpTestSuite() {
    // log_v2::instance().get(log_v2::CORE)->set_level(spdlog::level::trace);
    srand(time(nullptr));
  }
};

TEST_F(global_cache_test, CanBeMoved) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto fill = [&](unsigned index_min, unsigned index_max) {
    for (unsigned ii = index_min; ii < index_max; ++ii) {
      auto instance = std::make_shared<neb::pb_instance>();
      instance->mut_obj().set_instance_id(ii);
      instance->mut_obj().set_name(fmt::format("instance_{}", ii));
      instance->mut_obj().set_running(true);
      obj->write(instance);

      auto host = std::make_shared<neb::pb_host>();
      host->mut_obj().set_host_id(ii);
      host->mut_obj().set_name(fmt::format("host_{}", ii));
      host->mut_obj().set_check_command(
          fmt::format("host_check_command {}", ii));
      host->mut_obj().set_output(
          fmt::format("host_check_command_output {}", ii));
      host->mut_obj().set_enabled(true);
      obj->write(host);

      auto host_custom_var = std::make_shared<neb::pb_custom_variable>();
      host_custom_var->mut_obj().set_host_id(ii);
      host_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
      host_custom_var->mut_obj().set_value("1");
      host_custom_var->mut_obj().set_enabled(true);
      obj->write(host_custom_var);

      auto service = std::make_shared<neb::pb_service>();
      service->mut_obj().set_host_id(ii);
      service->mut_obj().set_service_id(ii + 1);
      service->mut_obj().set_description(fmt::format("service_{}", ii + 1));
      service->mut_obj().set_check_command(
          fmt::format("service_check_command {}", ii + 1));
      service->mut_obj().set_output(
          fmt::format("service_check_command_output {}", ii + 1));
      service->mut_obj().set_enabled(true);
      obj->write(service);
      auto service_custom_var = std::make_shared<neb::pb_custom_variable>();
      service_custom_var->mut_obj().set_host_id(ii);
      service_custom_var->mut_obj().set_service_id(ii + 1);
      service_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
      service_custom_var->mut_obj().set_value("2");
      service_custom_var->mut_obj().set_enabled(true);
      obj->write(service_custom_var);

      auto index_mapping = std::make_shared<storage::pb_index_mapping>();
      index_mapping->mut_obj().set_index_id(ii + 2);
      index_mapping->mut_obj().set_host_id(ii);
      index_mapping->mut_obj().set_service_id(ii + 1);
      obj->write(index_mapping);

      auto metric_mapping = std::make_shared<storage::pb_metric_mapping>();
      metric_mapping->mut_obj().set_metric_id(ii);
      metric_mapping->mut_obj().set_index_id(ii + 1);
      obj->write(metric_mapping);
    }
  };

  fill(0, 1000);

  auto check_data = [&](unsigned index_min, unsigned index_max) {
    for (unsigned ii = index_min; ii < index_max; ++ii) {
      {
        global_cache::lock l;
        auto inst = obj->get_instance(ii, l);
        ASSERT_TRUE(inst);
        ASSERT_EQ(fmt::format("instance_{}", ii), to_string(inst->name()));
      }
      {
        global_cache::lock l;
        auto hst = obj->get_host(ii, l);
        ASSERT_TRUE(hst);
        ASSERT_EQ(fmt::format("host_{}", ii), to_string(hst->name()));
        ASSERT_EQ(fmt::format("host_check_command {}", ii),
                  to_string(hst->check_command()));
        ASSERT_EQ(fmt::format("host_check_command_output {}", ii),
                  to_string(hst->output()));
        ASSERT_EQ(obj->get_severity(ii, 0), 1);
      }
      {
        global_cache::lock l;
        auto serv = obj->get_service(ii, ii + 1, l);
        ASSERT_TRUE(serv);
        ASSERT_EQ(fmt::format("service_{}", ii + 1),
                  to_string(serv->description()));
        ASSERT_EQ(fmt::format("service_check_command {}", ii + 1),
                  to_string(serv->check_command()));
        ASSERT_EQ(fmt::format("service_check_command_output {}", ii + 1),
                  to_string(serv->output()));
      }
      ASSERT_EQ(obj->get_severity(ii, ii + 1), 2);
      auto host_serv_id = obj->get_host_serv_id(ii + 2);
      ASSERT_TRUE(host_serv_id);
      ASSERT_EQ(obj->get_index_id_from_metric_id(ii), ii + 1);
    }
  };

  check_data(0, 1000);
  const void* mapping_begin = obj->get_address();

  std::cout << "first mapping at " << mapping_begin << std::endl;

  global_cache::unload();
  obj.reset();

  obj = global_cache::load(g_io_context, "/tmp/cache_test");

  std::cout << "reload mapping at " << obj->get_address() << std::endl;

  check_data(0, 1000);
  obj.reset();

  global_cache::unload();

  char temp_path[] = "/tmp/cache_test_XXXXXX";
  mkstemp(temp_path);
  ::remove(temp_path);

  // use old map address to force global cache to use another one
  boost::interprocess::managed_mapped_file dummy1(
      interprocess::create_only, temp_path, 0x10000, mapping_begin);

  obj = global_cache::load(g_io_context, "/tmp/cache_test");

  check_data(0, 1000);

  fill(1000, 2000);
  check_data(0, 2000);

  mapping_begin = obj->get_address();
  std::cout << "second mapping at " << mapping_begin << std::endl;

  obj.reset();
  global_cache::unload();

  strcpy(temp_path, "/tmp/cache_test_XXXXXX");
  mkstemp(temp_path);
  ::remove(temp_path);

  std::cout << "dummy mapping at " << mapping_begin << std::endl;
  // use old map address to force global cache to use another one
  boost::interprocess::managed_mapped_file dummy2(
      interprocess::create_only, temp_path, 0x10000, mapping_begin);

  std::cout << "new mapping" << std::endl;
  obj = global_cache::load(g_io_context, "/tmp/cache_test");
  mapping_begin = obj->get_address();
  std::cout << "third mapping at " << mapping_begin << std::endl;
  check_data(0, 2000);

  // then we remove rt file and data from conf file must be used
  std::cout << "rebuild rt file from conf one" << std::endl;
  obj.reset();
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  obj = global_cache::load(g_io_context, "/tmp/cache_test");

  check_data(0, 2000);
}

TEST_F(global_cache_test, Group) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  unsigned ii;
  for (ii = 0; ii < 1000; ++ii) {
    auto host = std::make_shared<neb::pb_host>();
    host->mut_obj().set_host_id(ii);
    host->mut_obj().set_name(fmt::format("host_{}", ii));
    host->mut_obj().set_enabled(true);
    obj->write(host);

    auto service = std::make_shared<neb::pb_service>();
    service->mut_obj().set_host_id(ii);
    service->mut_obj().set_service_id(ii + 1);
    service->mut_obj().set_description(fmt::format("service_{}", ii + 1));
    service->mut_obj().set_enabled(true);
    obj->write(service);

    for (unsigned jj = 0; jj < 10; ++jj) {
      auto hst_group = std::make_shared<neb::pb_host_group>();
      hst_group->mut_obj().set_hostgroup_id(ii / 10 + jj);
      hst_group->mut_obj().set_enabled(true);
      obj->write(hst_group);
      auto hst_grp_member = std::make_shared<neb::pb_host_group_member>();
      hst_grp_member->mut_obj().set_hostgroup_id(ii / 10 + jj);
      hst_grp_member->mut_obj().set_host_id(ii);
      hst_grp_member->mut_obj().set_poller_id(ii & 1);
      hst_grp_member->mut_obj().set_enabled(true);
      obj->write(hst_grp_member);
    }
    for (unsigned jj = 0; jj < 10; ++jj) {
      auto serv_group = std::make_shared<neb::pb_service_group>();
      serv_group->mut_obj().set_servicegroup_id(ii / 10 + jj);
      serv_group->mut_obj().set_enabled(true);
      obj->write(serv_group);
      auto serv_grp_member = std::make_shared<neb::pb_service_group_member>();
      serv_grp_member->mut_obj().set_servicegroup_id(ii / 10 + 1000 + jj);
      serv_grp_member->mut_obj().set_host_id(ii);
      serv_grp_member->mut_obj().set_service_id(ii + 1);
      serv_grp_member->mut_obj().set_poller_id(ii & 1);
      serv_grp_member->mut_obj().set_enabled(true);
      obj->write(serv_grp_member);
    }
  }

  // check host groups
  for (ii = 0; ii < 1000; ++ii) {
    std::ostringstream host_grp;
    std::string check;
    absl::StrAppend(&check, ii / 10);
    for (unsigned jj = 1; jj < 10; ++jj) {
      check.push_back(',');
      absl::StrAppend(&check, ii / 10 + jj);
    }
    obj->append_host_group(ii, host_grp);
    ASSERT_EQ(host_grp.str(), check);
  }

  // check service group
  for (ii = 0; ii < 1000; ++ii) {
    std::ostringstream serv_grp;
    std::string check;
    absl::StrAppend(&check, ii / 10 + 1000);
    for (unsigned jj = 1; jj < 10; ++jj) {
      check.push_back(',');
      absl::StrAppend(&check, ii / 10 + 1000 + jj);
    }
    obj->append_service_group(ii, ii + 1, serv_grp);
    ASSERT_EQ(serv_grp.str(), check);
  }

  {
    auto serv_grp_member = std::make_shared<neb::pb_service_group_member>();
    serv_grp_member->mut_obj().set_servicegroup_id(1000);
    serv_grp_member->mut_obj().set_host_id(0);
    serv_grp_member->mut_obj().set_service_id(1);
    serv_grp_member->mut_obj().set_poller_id(0);
    serv_grp_member->mut_obj().set_enabled(false);
    obj->write(serv_grp_member);
    std::ostringstream serv_grp;
    obj->append_service_group(0, 1, serv_grp);
    ASSERT_EQ(serv_grp.str(), "1001,1002,1003,1004,1005,1006,1007,1008,1009");
  }

  // service group remove from poller 0 but not from poller 1
  {
    auto serv_group = std::make_shared<neb::pb_service_group>();
    serv_group->mut_obj().set_servicegroup_id(1005);
    serv_group->mut_obj().set_poller_id(0);
    serv_group->mut_obj().set_enabled(false);
    obj->write(serv_group);

    std::ostringstream serv_grp;
    obj->append_service_group(0, 1, serv_grp);
    ASSERT_EQ(serv_grp.str(), "1001,1002,1003,1004,1006,1007,1008,1009");
  }
  {
    std::ostringstream serv_grp;
    obj->append_service_group(1, 2, serv_grp);
    ASSERT_EQ(serv_grp.str(),
              "1000,1001,1002,1003,1004,1005,1006,1007,1008,1009");
  }

  // remove from all pollers
  {
    auto serv_group = std::make_shared<neb::pb_service_group>();
    serv_group->mut_obj().set_servicegroup_id(1005);
    serv_group->mut_obj().set_poller_id(1);
    serv_group->mut_obj().set_enabled(false);
    obj->write(serv_group);
    std::ostringstream serv_grp;
    obj->append_service_group(1, 2, serv_grp);
    ASSERT_EQ(serv_grp.str(), "1000,1001,1002,1003,1004,1006,1007,1008,1009");
  }

  {
    auto hst_grp_member = std::make_shared<neb::pb_host_group_member>();
    hst_grp_member->mut_obj().set_host_id(55);
    hst_grp_member->mut_obj().set_hostgroup_id(10);
    hst_grp_member->mut_obj().set_poller_id(1);
    hst_grp_member->mut_obj().set_enabled(false);
    obj->write(hst_grp_member);

    std::ostringstream host_grp;
    obj->append_host_group(55, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,11,12,13,14");
  }

  {
    auto hst_grp_member = std::make_shared<neb::pb_host_group_member>();
    hst_grp_member->mut_obj().set_host_id(55);
    hst_grp_member->mut_obj().set_hostgroup_id(13);
    hst_grp_member->mut_obj().set_poller_id(1);
    hst_grp_member->mut_obj().set_enabled(false);
    obj->write(hst_grp_member);

    std::ostringstream host_grp;
    obj->append_host_group(55, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,11,12,14");
  }
  {
    std::ostringstream host_grp;
    obj->append_host_group(54, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,10,11,12,13,14");
  }
  {
    auto hst_grp_member = std::make_shared<neb::pb_host_group_member>();
    hst_grp_member->mut_obj().set_host_id(54);
    hst_grp_member->mut_obj().set_hostgroup_id(13);
    hst_grp_member->mut_obj().set_poller_id(0);
    hst_grp_member->mut_obj().set_enabled(false);
    obj->write(hst_grp_member);

    std::ostringstream host_grp;
    obj->append_host_group(54, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,10,11,12,14");
  }
}

TEST_F(global_cache_test, Tag) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  unsigned ii;

  for (ii = 0; ii < 100; ++ii) {
    auto tg = std::make_shared<neb::pb_tag>();
    tg->mut_obj().set_id(ii);
    tg->mut_obj().set_name(fmt::format("tag_service_group_{}", ii));
    tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
    tg->mut_obj().set_type(TagType::SERVICEGROUP);
    obj->write(tg);
    tg = std::make_shared<neb::pb_tag>();
    tg->mut_obj().set_id(ii + 100);
    tg->mut_obj().set_name(fmt::format("tag_host_group_{}", ii + 100));
    tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
    tg->mut_obj().set_type(TagType::HOSTGROUP);
    obj->write(tg);
    tg = std::make_shared<neb::pb_tag>();
    tg->mut_obj().set_id(ii + 200);
    tg->mut_obj().set_name(fmt::format("tag_service_category_{}", ii + 200));
    tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
    tg->mut_obj().set_type(TagType::SERVICECATEGORY);
    obj->write(tg);
    tg = std::make_shared<neb::pb_tag>();
    tg->mut_obj().set_id(ii + 300);
    tg->mut_obj().set_name(fmt::format("tag_host_category_{}", ii + 300));
    tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
    tg->mut_obj().set_type(TagType::HOSTCATEGORY);
    obj->write(tg);
  }
  for (ii = 0; ii < 1000; ++ii) {
    auto host = std::make_shared<neb::pb_host>();

    host->mut_obj().set_host_id(ii);
    host->mut_obj().set_name(fmt::format("host_{}", ii));
    host->mut_obj().set_enabled(true);
    // host group
    for (unsigned tag_id = 100 + (ii % 10); tag_id < 110 + (ii % 10);
         ++tag_id) {
      auto tg = host->mut_obj().add_tags();
      tg->set_id(tag_id);
      tg->set_type(TagType::HOSTGROUP);
    }
    // host cat
    for (unsigned tag_id = 300 + (ii % 10); tag_id < 310 + (ii % 10);
         ++tag_id) {
      auto tg = host->mut_obj().add_tags();
      tg->set_id(tag_id);
      tg->set_type(TagType::HOSTCATEGORY);
    }

    obj->write(host);

    auto service = std::make_shared<neb::pb_service>();
    service->mut_obj().set_host_id(ii);
    service->mut_obj().set_service_id(ii + 1);
    service->mut_obj().set_description(fmt::format("service_{}", ii + 1));
    service->mut_obj().set_enabled(true);
    // service group
    for (unsigned tag_id = 10 + (ii % 10); tag_id < 20 + (ii % 10); ++tag_id) {
      auto tg = service->mut_obj().add_tags();
      tg->set_id(tag_id);
      tg->set_type(TagType::SERVICEGROUP);
    }
    // service cat
    for (unsigned tag_id = 200 + (ii % 10); tag_id < 210 + (ii % 10);
         ++tag_id) {
      auto tg = service->mut_obj().add_tags();
      tg->set_id(tag_id);
      tg->set_type(TagType::SERVICECATEGORY);
    }
    obj->write(service);
  }

  for (ii = 0; ii < 1000; ++ii) {
    std::ostringstream host_tag_group_id, host_tag_group_name,
        serv_tag_group_id, serv_tag_group_name;
    std::ostringstream host_tag_cat_id, host_tag_cat_name, serv_tag_cat_id,
        serv_tag_cat_name;

    // host group
    std::string check_id;
    std::string check_name;
    unsigned begin_id = 100 + (ii % 10);
    check_name = fmt::format("tag_host_group_{}", begin_id);
    absl::StrAppend(&check_id, begin_id);
    for (unsigned jj = 1; jj < 10; ++jj) {
      check_id.push_back(',');
      absl::StrAppend(&check_id, ++begin_id);
      check_name.push_back(',');
      check_name.append(fmt::format("tag_host_group_{}", begin_id));
    }
    obj->append_host_tag_id(ii, TagType::HOSTGROUP, host_tag_group_id);
    obj->append_host_tag_name(ii, TagType::HOSTGROUP, host_tag_group_name);
    ASSERT_EQ(host_tag_group_id.str(), check_id);
    ASSERT_EQ(host_tag_group_name.str(), check_name);

    // host category
    check_id.clear();
    begin_id = 300 + (ii % 10);
    check_name = fmt::format("tag_host_category_{}", begin_id);
    absl::StrAppend(&check_id, begin_id);
    for (unsigned jj = 1; jj < 10; ++jj) {
      check_id.push_back(',');
      absl::StrAppend(&check_id, ++begin_id);
      check_name.push_back(',');
      check_name.append(fmt::format("tag_host_category_{}", begin_id));
    }
    obj->append_host_tag_id(ii, TagType::HOSTCATEGORY, host_tag_cat_id);
    obj->append_host_tag_name(ii, TagType::HOSTCATEGORY, host_tag_cat_name);
    ASSERT_EQ(host_tag_cat_id.str(), check_id);
    ASSERT_EQ(host_tag_cat_name.str(), check_name);

    // service group
    check_id.clear();
    begin_id = 10 + (ii % 10);
    check_name = fmt::format("tag_service_group_{}", begin_id);
    absl::StrAppend(&check_id, begin_id);
    for (unsigned jj = 1; jj < 10; ++jj) {
      check_id.push_back(',');
      absl::StrAppend(&check_id, ++begin_id);
      check_name.push_back(',');
      check_name.append(fmt::format("tag_service_group_{}", begin_id));
    }
    obj->append_serv_tag_id(ii, ii + 1, TagType::SERVICEGROUP,
                            serv_tag_group_id);
    obj->append_serv_tag_name(ii, ii + 1, TagType::SERVICEGROUP,
                              serv_tag_group_name);
    ASSERT_EQ(serv_tag_group_id.str(), check_id);
    ASSERT_EQ(serv_tag_group_name.str(), check_name);

    // service teg
    check_id.clear();
    begin_id = 200 + (ii % 10);
    check_name = fmt::format("tag_service_category_{}", begin_id);
    absl::StrAppend(&check_id, begin_id);
    for (unsigned jj = 1; jj < 10; ++jj) {
      check_id.push_back(',');
      absl::StrAppend(&check_id, ++begin_id);
      check_name.push_back(',');
      check_name.append(fmt::format("tag_service_category_{}", begin_id));
    }
    obj->append_serv_tag_id(ii, ii + 1, TagType::SERVICECATEGORY,
                            serv_tag_cat_id);
    obj->append_serv_tag_name(ii, ii + 1, TagType::SERVICECATEGORY,
                              serv_tag_cat_name);
    ASSERT_EQ(serv_tag_cat_id.str(), check_id);
    ASSERT_EQ(serv_tag_cat_name.str(), check_name);
  }
}

TEST_F(global_cache_test, Huge) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "begin construct cache");
  // 10000 hosts with 30 services with 20 metrics
  unsigned serv_id = 1;
  unsigned index_id = 1;
  unsigned metric_id = 1;
  for (unsigned host_id = 1; host_id < 10000; ++host_id) {
    auto host = std::make_shared<neb::pb_host>();

    host->mut_obj().set_host_id(host_id);
    host->mut_obj().set_name(fmt::format("host_{}", host_id));
    host->mut_obj().set_check_command(
        fmt::format("host_check_command {}", host_id));
    host->mut_obj().set_output(
        fmt::format("host_check_command_output {}", host_id));
    host->mut_obj().set_enabled(true);
    obj->write(host);

    for (unsigned cpt_service = host_id; cpt_service < 30;
         ++cpt_service, ++serv_id, ++index_id) {
      auto service = std::make_shared<neb::pb_service>();
      service->mut_obj().set_host_id(host_id);
      service->mut_obj().set_service_id(serv_id);
      service->mut_obj().set_description(fmt::format("service_{}", serv_id));
      service->mut_obj().set_check_command(
          fmt::format("service_check_command {}", serv_id));
      service->mut_obj().set_output(
          fmt::format("service_check_command_output {}", serv_id));
      service->mut_obj().set_enabled(true);
      obj->write(service);
      auto mapp = std::make_shared<storage::pb_index_mapping>();
      mapp->mut_obj().set_index_id(index_id);
      mapp->mut_obj().set_host_id(host_id);
      mapp->mut_obj().set_service_id(serv_id);
      obj->write(mapp);
      for (unsigned cpt_metric = 0; cpt_metric < 20;
           ++cpt_metric, ++metric_id) {
        auto metric_index = std::make_shared<storage::pb_metric_mapping>();
        metric_index->mut_obj().set_index_id(index_id);
        metric_index->mut_obj().set_metric_id(metric_id);
        obj->write(metric_index);
      }
    }
  }
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "end construct cache");
  // we search all serv and all host for all metrics
  for (unsigned ii = 1; ii < metric_id; ++ii) {
    uint64_t index_id = obj->get_index_id_from_metric_id(ii);
    ASSERT_NE(index_id, 0);
    std::optional<host_serv_pair> hst_serv_id = obj->get_host_serv_id(index_id);
    ASSERT_TRUE(hst_serv_id);
    global_cache::lock l;
    auto hst = obj->get_host(hst_serv_id->first, l);
    ASSERT_TRUE(hst);
    auto srv = obj->get_service(hst_serv_id->first, hst_serv_id->second, l);
    ASSERT_TRUE(srv);
  }
  obj.reset();
  cache::global_cache::unload();
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "end of 10000 metric search");
}

// ---------------------------------------------------------------------------
// Write / null guard
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, WriteNullIsNoOp) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");
  // Must not crash or throw.
  ASSERT_NO_THROW(obj->write(nullptr));
  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Unknown-ID lookups return null / zero
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, UnknownLookupReturnsNull) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_host(9999, l), nullptr);
  }
  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_service(9999, 9999, l), nullptr);
  }
  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_instance(9999, l), nullptr);
  }
  EXPECT_FALSE(obj->get_host_serv_id(9999));
  EXPECT_EQ(obj->get_index_id_from_metric_id(9999), 0u);
  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_dimension_ba_event(9999, l), nullptr);
  }
  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_dimension_bv_event(9999, l), nullptr);
  }
  EXPECT_FALSE(obj->get_severity(9999, 9999).has_value());

  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Severity via CRITICALITY_LEVEL custom variable
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, SeverityFromCustomVar) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  // Host without custom var: no severity.
  auto host = std::make_shared<neb::pb_host>();
  host->mut_obj().set_host_id(1);
  host->mut_obj().set_name("h1");
  host->mut_obj().set_enabled(true);
  obj->write(host);
  EXPECT_EQ(obj->get_severity(1, 0), 0);

  // Add CRITICALITY_LEVEL=5 on the host.
  auto hcv = std::make_shared<neb::pb_custom_variable>();
  hcv->mut_obj().set_host_id(1);
  hcv->mut_obj().set_name("CRITICALITY_LEVEL");
  hcv->mut_obj().set_value("5");
  hcv->mut_obj().set_enabled(true);
  obj->write(hcv);
  EXPECT_EQ(obj->get_severity(1, 0), 5);

  // Service: severity from its own custom var overrides the host one.
  auto service = std::make_shared<neb::pb_service>();
  service->mut_obj().set_host_id(1);
  service->mut_obj().set_service_id(2);
  service->mut_obj().set_description("s2");
  service->mut_obj().set_enabled(true);
  obj->write(service);
  auto scv = std::make_shared<neb::pb_custom_variable>();
  scv->mut_obj().set_host_id(1);
  scv->mut_obj().set_service_id(2);
  scv->mut_obj().set_name("CRITICALITY_LEVEL");
  scv->mut_obj().set_value("3");
  scv->mut_obj().set_enabled(true);
  obj->write(scv);
  EXPECT_EQ(obj->get_severity(1, 2), 3);

  obj.reset();
  global_cache::unload();
}

TEST_F(global_cache_test, SeverityDisabledCustomVarIsIgnored) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto host = std::make_shared<neb::pb_host>();
  host->mut_obj().set_host_id(1);
  host->mut_obj().set_name("h1");
  host->mut_obj().set_enabled(true);
  obj->write(host);

  // Add CRITICALITY_LEVEL then disable it.
  auto hcv = std::make_shared<neb::pb_custom_variable>();
  hcv->mut_obj().set_host_id(1);
  hcv->mut_obj().set_name("CRITICALITY_LEVEL");
  hcv->mut_obj().set_value("7");
  hcv->mut_obj().set_enabled(true);
  obj->write(hcv);
  EXPECT_EQ(obj->get_severity(1, 0), 7);

  host->mut_obj().set_enabled(false);
  obj->write(host);
  EXPECT_FALSE(obj->get_severity(1, 0).has_value());

  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Instance lifecycle
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, InstanceLifecycle) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto inst = std::make_shared<neb::pb_instance>();
  inst->mut_obj().set_instance_id(42);
  inst->mut_obj().set_name("poller-42");
  inst->mut_obj().set_running(true);
  obj->write(inst);
  {
    global_cache::lock l;
    auto* stored = obj->get_instance(42, l);
    ASSERT_TRUE(stored);
    EXPECT_EQ(to_string(stored->name()), "poller-42");
  }
  // Stopping the instance removes it from the cache.
  inst->mut_obj().set_running(false);
  obj->write(inst);
  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_instance(42, l), nullptr);
  }
  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Host lifecycle (disabled host is removed)
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, HostDisabledIsRemoved) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto host = std::make_shared<neb::pb_host>();
  host->mut_obj().set_host_id(10);
  host->mut_obj().set_name("h10");
  host->mut_obj().set_enabled(true);
  obj->write(host);
  {
    global_cache::lock l;
    ASSERT_TRUE(obj->get_host(10, l));
  }
  host->mut_obj().set_enabled(false);
  obj->write(host);

  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_host(10, l), nullptr);
  }
  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Host status updates runtime fields
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, HostStatusUpdatesFields) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto host = std::make_shared<neb::pb_host>();
  host->mut_obj().set_host_id(1);
  host->mut_obj().set_name("h1");
  host->mut_obj().set_enabled(true);
  host->mut_obj().set_state(Host_State::Host_State_UP);
  obj->write(host);

  auto status = std::make_shared<neb::pb_host_status>();
  status->mut_obj().set_host_id(1);
  status->mut_obj().set_state(HostStatus_State::HostStatus_State_DOWN);
  status->mut_obj().set_output("CRITICAL: disk full");
  status->mut_obj().set_last_check(123456789);
  obj->write(status);

  {
    global_cache::lock l;
    auto* h = obj->get_host(1, l);
    ASSERT_TRUE(h);
    EXPECT_EQ(h->state(), 1);
    EXPECT_EQ(to_string(h->output()), "CRITICAL: disk full");
    EXPECT_EQ(h->last_check(), 123456789);
  }

  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Adaptive host updates check-configuration fields
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, AdaptiveHostUpdatesCheckCommand) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto host = std::make_shared<neb::pb_host>();
  host->mut_obj().set_host_id(1);
  host->mut_obj().set_name("h1");
  host->mut_obj().set_check_command("old_cmd");
  host->mut_obj().set_enabled(true);
  obj->write(host);

  auto adaptive = std::make_shared<neb::pb_adaptive_host>();
  adaptive->mut_obj().set_host_id(1);
  adaptive->mut_obj().set_check_command("new_cmd");
  obj->write(adaptive);

  {
    global_cache::lock l;
    auto* h = obj->get_host(1, l);
    ASSERT_TRUE(h);
    EXPECT_EQ(to_string(h->check_command()), "new_cmd");
  }

  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// BAM dimensions: ba_event, bv_event, ba_bv_relation, truncate
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, BAMDimensionsStoredAndQueried) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  // BA event
  auto ba = std::make_shared<bam::pb_dimension_ba_event>();
  ba->mut_obj().set_ba_id(1);
  ba->mut_obj().set_ba_name("BA-1");
  ba->mut_obj().set_ba_description("desc-1");
  obj->write(ba);

  // BV event
  auto bv = std::make_shared<bam::pb_dimension_bv_event>();
  bv->mut_obj().set_bv_id(10);
  bv->mut_obj().set_bv_name("BV-10");
  obj->write(bv);

  // BA-BV relation: ba_id=1 -> bv_id=10 and bv_id=20
  auto rel1 = std::make_shared<bam::pb_dimension_ba_bv_relation_event>();
  rel1->mut_obj().set_ba_id(1);
  rel1->mut_obj().set_bv_id(10);
  obj->write(rel1);
  auto rel2 = std::make_shared<bam::pb_dimension_ba_bv_relation_event>();
  rel2->mut_obj().set_ba_id(1);
  rel2->mut_obj().set_bv_id(20);
  obj->write(rel2);
  {
    global_cache::lock l;
    auto* stored_ba = obj->get_dimension_ba_event(1, l);
    ASSERT_TRUE(stored_ba);
    EXPECT_EQ(to_string(stored_ba->ba_name()), "BA-1");

    auto* stored_bv = obj->get_dimension_bv_event(10, l);
    ASSERT_TRUE(stored_bv);
    EXPECT_EQ(to_string(stored_bv->bv_name()), "BV-10");

    EXPECT_EQ(obj->get_dimension_ba_event(999, l), nullptr);
    EXPECT_EQ(obj->get_dimension_bv_event(999, l), nullptr);
  }
  // enumerate_bvs must return both bv_ids linked to ba_id=1
  std::vector<uint64_t> bvs;
  obj->enumerate_bvs(1, [&](uint64_t bv_id) { bvs.push_back(bv_id); });
  std::sort(bvs.begin(), bvs.end());
  ASSERT_EQ(bvs.size(), 2u);
  EXPECT_EQ(bvs[0], 10u);
  EXPECT_EQ(bvs[1], 20u);

  obj.reset();
  global_cache::unload();
}

TEST_F(global_cache_test, DimensionTruncateClearsBAMData) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  auto ba = std::make_shared<bam::pb_dimension_ba_event>();
  ba->mut_obj().set_ba_id(1);
  ba->mut_obj().set_ba_name("BA-1");
  obj->write(ba);

  auto bv = std::make_shared<bam::pb_dimension_bv_event>();
  bv->mut_obj().set_bv_id(10);
  bv->mut_obj().set_bv_name("BV-10");
  obj->write(bv);

  {
    global_cache::lock l;
    ASSERT_TRUE(obj->get_dimension_ba_event(1, l));
    ASSERT_TRUE(obj->get_dimension_bv_event(10, l));
  }

  // Sending the truncate signal with update_started=true clears all
  // dimensions.
  auto truncate = std::make_shared<bam::pb_dimension_truncate_table_signal>();
  truncate->mut_obj().set_update_started(true);
  obj->write(truncate);

  {
    global_cache::lock l;
    EXPECT_EQ(obj->get_dimension_ba_event(1, l), nullptr);
    EXPECT_EQ(obj->get_dimension_bv_event(10, l), nullptr);
  }
  obj.reset();
  global_cache::unload();
}

// ---------------------------------------------------------------------------
// Enumerate host/service group
// ---------------------------------------------------------------------------

TEST_F(global_cache_test, EnumerateHostGroup) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  // Two groups, each with host 1.
  for (unsigned gid : {1u, 2u}) {
    auto grp = std::make_shared<neb::pb_host_group>();
    grp->mut_obj().set_hostgroup_id(gid);
    grp->mut_obj().set_name(fmt::format("hg_{}", gid));
    grp->mut_obj().set_enabled(true);
    obj->write(grp);
    auto mem = std::make_shared<neb::pb_host_group_member>();
    mem->mut_obj().set_hostgroup_id(gid);
    mem->mut_obj().set_host_id(1);
    mem->mut_obj().set_poller_id(0);
    mem->mut_obj().set_enabled(true);
    obj->write(mem);
  }

  std::map<uint64_t, std::string> found;
  obj->enumerate_host_group(1, [&](uint64_t gid, const cache::string& name) {
    found[gid] = to_string(name);
  });

  EXPECT_EQ(found.size(), 2u);
  EXPECT_EQ(found[1], "hg_1");
  EXPECT_EQ(found[2], "hg_2");

  obj.reset();
  global_cache::unload();
}

TEST_F(global_cache_test, EnumerateServiceGroup) {
  global_cache::unload();
  ::remove("/tmp/cache_test.rt");
  ::remove("/tmp/cache_test.cnf");
  global_cache::pointer obj =
      global_cache::load(g_io_context, "/tmp/cache_test");

  for (unsigned gid : {10u, 20u}) {
    auto grp = std::make_shared<neb::pb_service_group>();
    grp->mut_obj().set_servicegroup_id(gid);
    grp->mut_obj().set_name(fmt::format("sg_{}", gid));
    grp->mut_obj().set_enabled(true);
    obj->write(grp);
    auto mem = std::make_shared<neb::pb_service_group_member>();
    mem->mut_obj().set_servicegroup_id(gid);
    mem->mut_obj().set_host_id(1);
    mem->mut_obj().set_service_id(2);
    mem->mut_obj().set_poller_id(0);
    mem->mut_obj().set_enabled(true);
    obj->write(mem);
  }

  std::map<uint64_t, std::string> found;
  obj->enumerate_service_group(1, 2,
                               [&](uint64_t gid, const cache::string& name) {
                                 found[gid] = to_string(name);
                               });

  EXPECT_EQ(found.size(), 2u);
  EXPECT_EQ(found[10], "sg_10");
  EXPECT_EQ(found[20], "sg_20");

  obj.reset();
  global_cache::unload();
}
