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

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "common/log_v2/log_v2.hh"

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
  ::remove("/tmp/cache_test");
  global_cache::pointer obj = global_cache::load("/tmp/cache_test");

  auto fill = [&](unsigned index_min, unsigned index_max) {
    unsigned ii;
    for (ii = index_min; ii < index_max; ++ii) {
      obj->set_metric_info(ii, ii + 1000, fmt::format("metric_name_{}", ii),
                           "metric_unit", double(ii) / 100,
                           double(ii + 10.5) / 100);
      obj->store_instance(ii, fmt::format("instance_{}", ii));
      obj->store_host(ii, fmt::format("host_{}", ii), ii + 1000000, 1);
      obj->store_service(ii, ii + 1, fmt::format("service_{}", ii),
                         ii + 10000000, 2);
      obj->set_index_mapping(ii + 2, ii, ii + 1);
    }
  };

  fill(0, 1000);

  auto check_data = [&](unsigned nb_metric) {
    global_cache::lock l;
    unsigned ii;
    for (ii = 0; ii < nb_metric; ++ii) {
      const metric_info* infos = obj->get_metric_info(ii);
      ASSERT_NE(infos, nullptr);
      std::string name_expected = fmt::format("metric_name_{}", ii);
      ASSERT_FALSE(infos->name.compare(0, name_expected.length(),
                                       name_expected.c_str()));
      ASSERT_EQ(infos->unit, "metric_unit");
      ASSERT_EQ(infos->min, double(ii) / 100);
      ASSERT_EQ(infos->max, double(ii + 10.5) / 100);
      const string* instance_name = obj->get_instance_name(ii);
      name_expected = fmt::format("instance_{}", ii);
      ASSERT_FALSE(instance_name->compare(0, name_expected.length(),
                                          name_expected.c_str()));
      name_expected = fmt::format("host_{}", ii);
      const resource_info* res_info = obj->get_host(ii);
      ASSERT_NE(res_info, nullptr);
      ASSERT_FALSE(res_info->name.compare(0, name_expected.length(),
                                          name_expected.c_str()));
      ASSERT_EQ(res_info->resource_id, ii + 1000000);
      ASSERT_EQ(res_info->severity_id, 1);
      const resource_info* res_info2 = obj->get_host_from_index_id(ii + 2);
      ASSERT_EQ(res_info, res_info2);

      res_info = obj->get_service(ii, ii + 1);
      name_expected = fmt::format("service_{}", ii);
      ASSERT_NE(res_info, nullptr);
      ASSERT_FALSE(res_info->name.compare(0, name_expected.length(),
                                          name_expected.c_str()));
      ASSERT_EQ(res_info->resource_id, ii + 10000000);
      ASSERT_EQ(res_info->severity_id, 2);
      res_info2 = obj->get_service_from_index_id(ii + 2);
      ASSERT_EQ(res_info, res_info2);
    }
  };

  check_data(1000);
  const void* mapping_begin = obj->get_address();

  std::cout << "first mapping at " << mapping_begin << std::endl;
  obj.reset();

  global_cache::unload();

  obj = global_cache::load("/tmp/cache_test");

  check_data(1000);
  obj.reset();

  global_cache::unload();

  char temp_path[] = "/tmp/cache_test_XXXXXX";
  mkstemp(temp_path);
  ::remove(temp_path);

  // use old map address to force global cache to use another one
  boost::interprocess::managed_mapped_file dummy1(
      interprocess::create_only, temp_path, 0x10000, mapping_begin);

  obj = global_cache::load("/tmp/cache_test");

  check_data(1000);

  fill(1000, 2000);
  check_data(2000);

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

  obj = global_cache::load("/tmp/cache_test");
  mapping_begin = obj->get_address();
  std::cout << "third mapping at " << mapping_begin << std::endl;
  check_data(2000);
}

TEST_F(global_cache_test, Group) {
  global_cache::unload();
  ::remove("/tmp/cache_test");
  global_cache::pointer obj = global_cache::load("/tmp/cache_test");

  unsigned ii;
  for (ii = 0; ii < 1000; ++ii) {
    obj->store_host(ii, fmt::format("host_{}", ii), ii + 1000000, 1);
    obj->store_service(ii, ii + 1, fmt::format("service_{}", ii), ii + 10000000,
                       2);
    for (unsigned jj = 0; jj < 10; ++jj)
      obj->add_host_to_group(ii / 10 + jj, ii, ii & 1);
    for (unsigned jj = 0; jj < 10; ++jj)
      obj->add_service_to_group(ii / 10 + 1000 + jj, ii, ii + 1, ii & 1);
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

  obj->remove_service_from_group(1000, 0, 1);
  {
    std::ostringstream serv_grp;
    obj->append_service_group(0, 1, serv_grp);
    ASSERT_EQ(serv_grp.str(), "1001,1002,1003,1004,1005,1006,1007,1008,1009");
  }

  // service group remove from poller 0 but not from poller 1
  obj->remove_service_group_members(1005, 0);
  {
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
  obj->remove_service_group_members(1005, 1);
  {
    std::ostringstream serv_grp;
    obj->append_service_group(1, 2, serv_grp);
    ASSERT_EQ(serv_grp.str(), "1000,1001,1002,1003,1004,1006,1007,1008,1009");
  }

  obj->remove_host_from_group(10, 55);
  {
    std::ostringstream host_grp;
    obj->append_host_group(55, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,11,12,13,14");
  }

  obj->remove_host_group_members(13, 1);
  {
    std::ostringstream host_grp;
    obj->append_host_group(55, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,11,12,14");
  }
  {
    std::ostringstream host_grp;
    obj->append_host_group(54, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,10,11,12,13,14");
  }
  obj->remove_host_group_members(13, 0);
  {
    std::ostringstream host_grp;
    obj->append_host_group(54, host_grp);
    ASSERT_EQ(host_grp.str(), "5,6,7,8,9,10,11,12,14");
  }
}

TEST_F(global_cache_test, Tag) {
  global_cache::unload();
  ::remove("/tmp/cache_test");
  global_cache::pointer obj = global_cache::load("/tmp/cache_test");

  unsigned ii;

  class enumerator {
    const uint64_t _min, _max;
    uint64_t _current;

   public:
    enumerator(uint64_t min, uint64_t max)
        : _min(min), _max(max), _current(min) {}

    uint64_t operator()() { return _current >= _max ? 0 : (_current++); }
  };

  for (ii = 0; ii < 100; ++ii) {
    obj->add_tag(ii, fmt::format("tag_service_group_{}", ii),
                 TagType::SERVICEGROUP, 1);
    obj->add_tag(ii + 100, fmt::format("tag_host_group_{}", ii + 100),
                 TagType::HOSTGROUP, 1);
    obj->add_tag(ii + 200, fmt::format("tag_service_category_{}", ii + 200),
                 TagType::SERVICECATEGORY, 1);
    obj->add_tag(ii + 300, fmt::format("tag_host_category_{}", ii + 300),
                 TagType::HOSTCATEGORY, 1);
  }
  for (ii = 0; ii < 1000; ++ii) {
    obj->store_host(ii, fmt::format("host_{}", ii), ii + 1000000, 1);
    obj->store_service(ii, ii + 1, fmt::format("service_{}", ii), ii + 10000000,
                       2);

    obj->set_host_tag(
        ii, enumerator(100 + (ii % 10), 110 + (ii % 10)));  // host group
    obj->set_host_tag(
        ii, enumerator(300 + (ii % 10), 310 + (ii % 10)));  // host category
    obj->set_serv_tag(
        ii, ii + 1,
        enumerator(10 + (ii % 10), 20 + (ii % 10)));  // service group
    obj->set_serv_tag(
        ii, ii + 1,
        enumerator(200 + (ii % 10), 210 + (ii % 10)));  // service category
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
  ::remove("/tmp/cache_test");
  global_cache::pointer obj = global_cache::load("/tmp/cache_test");

  unsigned ii;

  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "begin construct cache");
  // 10000 hosts with 30 services with 20 metrics
  unsigned serv_id = 1;
  unsigned resource_id = 1;
  unsigned index_id = 1;
  unsigned metric_id = 1;
  for (unsigned host_id = 1; host_id < 10000; ++host_id) {
    obj->store_host(host_id, fmt::format("host_{}", host_id), ++resource_id, 1);
    for (unsigned cpt_service = 0; cpt_service < 30;
         ++cpt_service, ++serv_id, ++index_id) {
      obj->store_service(host_id, serv_id, fmt::format("service_{}", serv_id),
                         ++resource_id, 2);
      obj->set_index_mapping(index_id, host_id, serv_id);
      for (unsigned cpt_metric = 0; cpt_metric < 20;
           ++cpt_metric, ++metric_id) {
        obj->set_metric_info(
            metric_id, index_id,
            fmt::format("metric_index_{}_id_{}", index_id, metric_id), "%", 0,
            100);
      }
    }
  }
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "end construct cache");
  // we search 10000 metrics infos
  unsigned metric_info_increment = metric_id / 10000;
  unsigned id_search = 0;
  for (ii = 0; ii < 10000; ++ii, id_search += metric_info_increment)
    obj->get_metric_info(id_search);
  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                     "end of 10000 metric search");
}
