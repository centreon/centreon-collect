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

#include "com/centreon/broker/cache/global_cache.hh"
#include <gtest/gtest.h>

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/log_v2/log_v2.hh"
#include "storage/metric_mapping.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

extern std::string to_string(const com::centreon::broker::cache::string& src);

using namespace com::centreon::broker;
using namespace com::centreon::broker::cache;

using log_v2 = com::centreon::common::log_v2::log_v2;

class global_cache_test : public testing::Test {
 public:
  static void SetUpTestSuite() {
    log_v2::instance().get(log_v2::CORE)->set_level(spdlog::level::trace);
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
    global_cache::lock l;
    for (unsigned ii = index_min; ii < index_max; ++ii) {
      auto inst = obj->get_instance(ii);
      ASSERT_TRUE(inst);
      ASSERT_EQ(fmt::format("instance_{}", ii), to_string(inst->name()));

      auto hst = obj->get_host(ii);
      ASSERT_TRUE(hst);
      ASSERT_EQ(fmt::format("host_{}", ii), to_string(hst->name()));
      ASSERT_EQ(fmt::format("host_check_command {}", ii),
                to_string(hst->check_command()));
      ASSERT_EQ(fmt::format("host_check_command_output {}", ii),
                to_string(hst->output()));
      ASSERT_EQ(obj->get_severity(ii, 0), 1);
      auto serv = obj->get_service(ii, ii + 1);
      ASSERT_TRUE(serv);
      ASSERT_EQ(fmt::format("service_{}", ii + 1),
                to_string(serv->description()));
      ASSERT_EQ(fmt::format("service_check_command {}", ii + 1),
                to_string(serv->check_command()));
      ASSERT_EQ(fmt::format("service_check_command_output {}", ii + 1),
                to_string(serv->output()));
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

  // use old map address to force global cache to use another one
  boost::interprocess::managed_mapped_file dummy2(
      interprocess::create_only, temp_path, 0x10000, mapping_begin);

  obj = global_cache::load(g_io_context, "/tmp/cache_test");
  mapping_begin = obj->get_address();
  std::cout << "third mapping at " << mapping_begin << std::endl;
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
    for (int tag_id = 100 + (ii % 10); tag_id < 110 + (ii % 10); ++tag_id) {
      auto tg = host->mut_obj().add_tags();
      tg->set_id(tag_id);
      tg->set_type(TagType::HOSTGROUP);
    }
    // host cat
    for (int tag_id = 300 + (ii % 10); tag_id < 310 + (ii % 10); ++tag_id) {
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
    for (int tag_id = 10 + (ii % 10); tag_id < 20 + (ii % 10); ++tag_id) {
      auto tg = service->mut_obj().add_tags();
      tg->set_id(tag_id);
      tg->set_type(TagType::SERVICEGROUP);
    }
    // service cat
    for (int tag_id = 200 + (ii % 10); tag_id < 210 + (ii % 10); ++tag_id) {
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
  unsigned resource_id = 1;
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
    const host_serv_pair* hst_serv_id = obj->get_host_serv_id(index_id);
    ASSERT_TRUE(hst_serv_id);
    auto hst = obj->get_host(hst_serv_id->first);
    ASSERT_TRUE(hst);
    auto srv = obj->get_service(hst_serv_id->first, hst_serv_id->second);
  }
  obj.reset();
  cache::global_cache::unload();
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "end of 10000 metric search");
}
