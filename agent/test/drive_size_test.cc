/**
 * Copyright 2024 Centreon
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

#include <absl/synchronization/mutex.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

#include "com/centreon/common/rapidjson_helper.hh"

#include "drive_size.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_drive_size_detail;

struct sample {
  std::string_view fs;
  std::string_view mount_point;
  uint64_t fs_type;
  uint64_t used;
  uint64_t total;
};

std::array<sample, 9> _samples = {
    {{"udev", "/dev",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_other,
      0, 6024132000},

     {"tmpfs", "/run",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_other,
      16760000, 1212868000},
     {"/dev/sda12", "/",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_linux_ext4,
      136830444000, 346066920000},
     {"tmpfs", "/dev/shm",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_other,
      0, 6072708000},
     {"tmpfs", "/run/lock",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_other,
      4000, 5116000},
     {"tmpfs", "/sys/fs/cgroup",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_other,
      0, 6072708000},
     {"/dev/sda11", "/boot/efi",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_fat,
      24000, 524248000},
     {"/dev/sda5", "/data",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_fat32,
      3072708000, 6072708000},
     {"tmpfs", "/run/user/1001",
      check_drive_size_detail::e_drive_fs_type::hr_storage_fixed_disk |
          check_drive_size_detail::e_drive_fs_type::hr_fs_other,
      100000, 1214440000}}};

class drive_size_test : public ::testing::Test {
 public:
  static std::list<fs_stat> compute(
      filter& filt,
      const std::shared_ptr<spdlog::logger>& logger);

  static void SetUpTestCase() { drive_size_thread::os_fs_stats = compute; }
  static void TearDownTestCase() { check_drive_size::thread_kill(); }
};

std::list<fs_stat> drive_size_test::compute(
    filter& filt,
    const std::shared_ptr<spdlog::logger>&) {
  std::list<fs_stat> result;
  for (const auto& s : _samples) {
    if (filt.is_allowed(s.fs, s.mount_point,
                        static_cast<e_drive_fs_type>(s.fs_type))) {
      result.emplace_back(s.fs, s.mount_point, s.used, s.total);
    }
  }
  return result;
}

using namespace std::string_literals;

TEST_F(drive_size_test, test_fs_filter1) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "1000000", "critical" : 20000000, "unit": "b",
        "filter-type": "^hrfsother$"})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output, "WARNING: /run Total: 1G Used: 0G Free: 1G");
  ASSERT_EQ(perfs.size(), 6);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "B");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 1000000);
    ASSERT_EQ(p.critical(), 20000000);
    if (p.name() == "used_/dev") {
      ASSERT_EQ(p.value(), 0);
      ASSERT_EQ(p.max(), 6024132000);
    } else if (p.name() == "used_/run") {
      ASSERT_EQ(p.value(), 16760000);
      ASSERT_EQ(p.max(), 1212868000);
    } else if (p.name() == "used_/dev/shm") {
      ASSERT_EQ(p.value(), 0);
      ASSERT_EQ(p.max(), 6072708000);
    } else if (p.name() == "used_/run/lock") {
      ASSERT_EQ(p.value(), 4000);
      ASSERT_EQ(p.max(), 5116000);
    } else if (p.name() == "used_/sys/fs/cgroup") {
      ASSERT_EQ(p.value(), 0);
      ASSERT_EQ(p.max(), 6072708000);
    } else if (p.name() == "used_/run/user/1001") {
      ASSERT_EQ(p.value(), 100000);
      ASSERT_EQ(p.max(), 1214440000);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter_percent) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "1", "critical" : 5, "unit": "%",
        "filter-type": "^hrfsother$"})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output, "WARNING: /run Total: 1G Used: 1.38% Free: 98.62%");
  ASSERT_EQ(perfs.size(), 6);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "%");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 1);
    ASSERT_EQ(p.critical(), 5);
    if (p.name() == "used_/dev") {
      ASSERT_EQ(p.value(), 0);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run") {
      ASSERT_NEAR(p.value(), 1.38, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/dev/shm") {
      ASSERT_NEAR(p.value(), 0, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run/lock") {
      ASSERT_NEAR(p.value(), 0.08, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/sys/fs/cgroup") {
      ASSERT_EQ(p.value(), 0);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run/user/1001") {
      ASSERT_NEAR(p.value(), 0, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter2) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "1000000", "critical" : 20000000, "unit": "b",
        "filter-type": "^(hrfsfat$|hrfsfat32)$"})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output, "CRITICAL: /data Total: 5G Used: 2G Free: 2G");
  ASSERT_EQ(perfs.size(), 2);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "B");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 1000000);
    ASSERT_EQ(p.critical(), 20000000);
    if (p.name() == "used_/boot/efi") {
      ASSERT_EQ(p.value(), 24000);
      ASSERT_EQ(p.max(), 524248000);
    } else if (p.name() == "used_/data") {
      ASSERT_EQ(p.value(), 3072708000);
      ASSERT_EQ(p.max(), 6072708000);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter_percent_2) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "1", "critical" : "5", "unit": "%",
        "filter-type": "^hrfsother$", "filter-fs": "^tmp.*$"})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output, "WARNING: /run Total: 1G Used: 1.38% Free: 98.62%");
  ASSERT_EQ(perfs.size(), 5);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "%");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 1);
    ASSERT_EQ(p.critical(), 5);
    if (p.name() == "used_/run") {
      ASSERT_NEAR(p.value(), 1.38, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/dev/shm") {
      ASSERT_NEAR(p.value(), 0, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run/lock") {
      ASSERT_NEAR(p.value(), 0.08, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/sys/fs/cgroup") {
      ASSERT_EQ(p.value(), 0);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run/user/1001") {
      ASSERT_NEAR(p.value(), 0, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter_percent_3) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "1", "critical" : "5", "unit": "%",
        "filter-type": "^hrfsother$", "filter-fs": "tmpfs", "filter-mountpoint":"^/run/.*$" })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output, "OK: All storages are ok");
  ASSERT_EQ(perfs.size(), 2);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "%");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 1);
    ASSERT_EQ(p.critical(), 5);
    if (p.name() == "used_/run") {
      ASSERT_NEAR(p.value(), 1.38, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run/lock") {
      ASSERT_NEAR(p.value(), 0.08, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/run/user/1001") {
      ASSERT_NEAR(p.value(), 0, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter_percent_4) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "1", "critical" : "5", "unit": "%",
        "filter-type": "^hrfsother$", "filter-fs": "tmpfs", "filter-mountpoint":"^/run.*$", "exclude-mountpoint": ".*lock.*" })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));
  {
    absl::MutexLock lck(&wait_m);
    wait_m.Await(absl::Condition(&is_complete));

    ASSERT_EQ(output, "WARNING: /run Total: 1G Used: 1.38% Free: 98.62%");
    ASSERT_EQ(perfs.size(), 2);

    for (const auto& p : perfs) {
      ASSERT_EQ(p.unit(), "%");
      ASSERT_EQ(p.min(), 0);
      ASSERT_EQ(p.warning_low(), 0);
      ASSERT_EQ(p.critical_low(), 0);
      ASSERT_EQ(p.warning(), 1);
      ASSERT_EQ(p.critical(), 5);
      if (p.name() == "used_/run") {
        ASSERT_NEAR(p.value(), 1.38, 0.01);
        ASSERT_EQ(p.max(), 100);
      } else if (p.name() == "used_/run/user/1001") {
        ASSERT_NEAR(p.value(), 0, 0.01);
        ASSERT_EQ(p.max(), 100);
      } else {
        FAIL() << "Unexpected perfdata name: " << p.name();
      }
    }
  }
  // recheck to validate filter cache
  std::string output_save = output;
  std::list<com::centreon::common::perfdata> perfs_save = perfs;

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output, output_save);
  ASSERT_EQ(perfs, perfs_save);
}

TEST_F(drive_size_test, test_fs_filter_percent_5) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "30", "critical" : "50", "unit": "%",
         "exclude-fs": "tmpfs", "exclude-mountpoint":"/dev" })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output,
            "WARNING: / Total: 322G Used: 39.54% Free: 60.46% CRITICAL: /data "
            "Total: 5G Used: 50.60% Free: 49.40%");
  ASSERT_EQ(perfs.size(), 3);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "%");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 30);
    ASSERT_EQ(p.critical(), 50);
    if (p.name() == "used_/") {
      ASSERT_NEAR(p.value(), 39.54, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/data") {
      ASSERT_NEAR(p.value(), 50.60, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/boot/efi") {
      ASSERT_NEAR(p.value(), 0.0045, 0.0001);
      ASSERT_EQ(p.max(), 100);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter_percent_6) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "30", "critical" : "", "unit": "%",
         "exclude-fs": "tmpfs", "exclude-mountpoint":"/dev" })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output,
            "WARNING: / Total: 322G Used: 39.54% Free: 60.46% WARNING: /data "
            "Total: 5G Used: 50.60% Free: 49.40%");
  ASSERT_EQ(perfs.size(), 3);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "%");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_TRUE(std::isnan(p.critical_low()));
    ASSERT_EQ(p.warning(), 30);
    ASSERT_TRUE(std::isnan(p.critical()));
    if (p.name() == "used_/") {
      ASSERT_NEAR(p.value(), 39.54, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/data") {
      ASSERT_NEAR(p.value(), 50.60, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "used_/boot/efi") {
      ASSERT_NEAR(p.value(), 0.0045, 0.0001);
      ASSERT_EQ(p.max(), 100);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}

TEST_F(drive_size_test, test_fs_filter_free_percent) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning" : "70", "critical" : "50", "unit": "%", "free": true,
         "exclude-fs": "tmpfs", "exclude-mountpoint":"/dev" })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;

  auto is_complete = [&]() { return !perfs.empty(); };

  auto debug_logger = spdlog::default_logger();

  auto checker = std::make_shared<check_drive_size>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), std::chrono::seconds(60), "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        perfs = perfdata;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(1));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_EQ(output,
            "WARNING: / Total: 322G Used: 39.54% Free: 60.46% CRITICAL: /data "
            "Total: 5G Used: 50.60% Free: 49.40%");
  ASSERT_EQ(perfs.size(), 3);

  for (const auto& p : perfs) {
    ASSERT_EQ(p.unit(), "%");
    ASSERT_EQ(p.min(), 0);
    ASSERT_EQ(p.warning_low(), 0);
    ASSERT_EQ(p.critical_low(), 0);
    ASSERT_EQ(p.warning(), 70);
    ASSERT_EQ(p.critical(), 50);
    if (p.name() == "free_/") {
      ASSERT_NEAR(p.value(), 60.46, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "free_/data") {
      ASSERT_NEAR(p.value(), 49.40, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else if (p.name() == "free_/boot/efi") {
      ASSERT_NEAR(p.value(), 99.99, 0.01);
      ASSERT_EQ(p.max(), 100);
    } else {
      FAIL() << "Unexpected perfdata name: " << p.name();
    }
  }
}
