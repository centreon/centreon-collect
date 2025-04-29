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

#ifndef CENTREON_AGENT_NATIVE_DRIVE_SIZE_BASE_HH
#define CENTREON_AGENT_NATIVE_DRIVE_SIZE_BASE_HH

#include "check.hh"
#include "re2/re2.h"

namespace com::centreon::agent {
namespace check_drive_size_detail {

/**
 * @brief these flags are passed in check parameter:filter-storage-type and
 * filter-type
 *
 */
enum e_drive_fs_type : uint64_t {
  hr_unknown = 0,
  hr_storage_ram = 1 << 0,
  hr_storage_virtual_memory = 1 << 1,
  hr_storage_fixed_disk = 1 << 2,
  hr_storage_removable_disk = 1 << 3,
  hr_storage_floppy_disk = 1 << 4,
  hr_storage_compact_disc = 1 << 5,
  hr_storage_ram_disk = 1 << 6,
  hr_storage_flash_memory = 1 << 7,
  hr_storage_network_disk = 1 << 8,
  hr_fs_other = 1 << 9,
  hr_fs_unknown = 1 << 10,
  hr_fs_berkeley_ffs = 1 << 11,
  hr_fs_sys5_fs = 1 << 12,
  hr_fs_fat = 1 << 13,
  hr_fs_hpfs = 1 << 14,
  hr_fs_hfs = 1 << 15,
  hr_fs_mfs = 1 << 16,
  hr_fs_ntfs = 1 << 17,
  hr_fs_vnode = 1 << 18,
  hr_fs_journaled = 1 << 19,
  hr_fs_iso9660 = 1 << 20,
  hr_fs_rock_ridge = 1 << 21,
  hr_fs_nfs = 1 << 22,
  hr_fs_netware = 1 << 23,
  hr_fs_afs = 1 << 24,
  hr_fs_dfs = 1 << 25,
  hr_fs_appleshare = 1 << 26,
  hr_fs_rfs = 1 << 27,
  hr_fs_dgcfs = 1 << 28,
  hr_fs_bfs = 1 << 29,
  hr_fs_fat32 = 1 << 30,
  hr_fs_linux_ext2 = 1U << 31,
  hr_fs_linux_ext4 = 1ULL << 32,
  hr_fs_exfat = 1ULL << 33
};

/**
 * @brief user can check only some fs by using filters
 * This is the goal of this class
 * In order to improve perf, results of previous tests are saved
 * in cache sets. That's why is_allowed is not const
 *
 */
class filter {
  using string_set = absl::flat_hash_set<std::string>;

  string_set _cache_allowed_fs ABSL_GUARDED_BY(_protect);
  string_set _cache_excluded_fs ABSL_GUARDED_BY(_protect);
  string_set _cache_allowed_mountpoint ABSL_GUARDED_BY(_protect);
  string_set _cache_excluded_mountpoint ABSL_GUARDED_BY(_protect);

  mutable absl::Mutex _protect;

  unsigned _fs_type_filter;

  std::unique_ptr<re2::RE2> _filter_fs, _filter_exclude_fs;
  std::unique_ptr<re2::RE2> _filter_mountpoint, _filter_exclude_mountpoint;

 public:
  filter(const rapidjson::Value& args);

  bool is_allowed(const std::string_view& fs,
                  const std::string_view& mount_point,
                  e_drive_fs_type fs_type);

  bool is_fs_yet_allowed(const std::string_view& fs) const;

  bool is_fs_yet_excluded(const std::string_view& fs) const;
};

/**
 * @brief tupple where we store statistics of a fs
 *
 */
struct fs_stat {
  fs_stat() = default;
  fs_stat(std::string&& fs_in, uint64_t used_in, uint64_t total_in)
      : fs(fs_in), mount_point(fs), used(used_in), total(total_in) {}

  fs_stat(std::string&& fs_in,
          std::string&& mount_point_in,
          uint64_t used_in,
          uint64_t total_in)
      : fs(fs_in),
        mount_point(mount_point_in),
        used(used_in),
        total(total_in) {}

  fs_stat(const std::string_view& fs_in,
          const std::string_view& mount_point_in,
          uint64_t used_in,
          uint64_t total_in)
      : fs(fs_in),
        mount_point(mount_point_in),
        used(used_in),
        total(total_in) {}

  std::string fs;
  std::string mount_point;
  uint64_t used;
  uint64_t total;

  bool is_used_more_than_threshold(uint64_t threshold) const {
    return used >= threshold;
  }

  bool is_free_less_than_threshold(uint64_t threshold) const {
    return total - used < threshold;
  }

  bool is_used_more_than_prct_threshold(uint64_t percent_hundredth) const {
    if (!total) {
      return true;
    }
    return (used * 10000) / total >= percent_hundredth;
  }

  bool is_free_less_than_prct_threshold(uint64_t percent_hundredth) const {
    if (!total) {
      return true;
    }
    return ((total - used) * 10000) / total < percent_hundredth;
  }

  double get_used_prct() const {
    if (!total)
      return 0.0;
    return static_cast<double>(used * 100) / total;
  }

  double get_free_prct() const {
    if (!total)
      return 0.0;
    return static_cast<double>((total - used) * 100) / total;
  }
};

/**
 * @brief get fs statistics can block on network drives, so we use this thread
 * to do the job and not block main thread
 *
 */
class drive_size_thread
    : public std::enable_shared_from_this<drive_size_thread> {
  std::shared_ptr<asio::io_context> _io_context;

  using completion_handler = std::function<void(std::list<fs_stat>)>;

  struct async_data {
    std::shared_ptr<filter> request_filter;
    completion_handler handler;
    time_point timeout;
  };

  std::list<async_data> _queue ABSL_GUARDED_BY(_queue_m);
  absl::Mutex _queue_m;

  bool _active = true;

  std::shared_ptr<spdlog::logger> _logger;

  bool has_to_stop_wait() const { return !_active || !_queue.empty(); }

 public:
  typedef std::list<fs_stat> (
      *get_fs_stats)(filter&, const std::shared_ptr<spdlog::logger>& logger);

  static get_fs_stats os_fs_stats;

  drive_size_thread(const std::shared_ptr<asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger)
      : _io_context(io_context), _logger(logger) {}

  void run();

  void kill();

  template <class handler_type>
  void async_get_fs_stats(const std::shared_ptr<filter>& request_filter,
                          const time_point& timeout,
                          handler_type&& handler);
};

}  // namespace check_drive_size_detail

/**
 * @brief drive size check object (same for linux and windows)
 *
 */
class check_drive_size : public check {
  std::shared_ptr<check_drive_size_detail::filter> _filter;
  bool _prct_threshold;
  bool _free_threshold;
  uint64_t _warning;  // value in bytes or percent * 100
  uint64_t _critical;

  typedef e_status (check_drive_size::*fs_stat_test)(
      const check_drive_size_detail::fs_stat&) const;

  fs_stat_test _fs_test;

  e_status _used_test(const check_drive_size_detail::fs_stat& fs) const;
  e_status _prct_used_test(const check_drive_size_detail::fs_stat& fs) const;

  e_status _free_test(const check_drive_size_detail::fs_stat& fs) const;
  e_status _prct_free_test(const check_drive_size_detail::fs_stat& fs) const;

  e_status _no_test(const check_drive_size_detail::fs_stat& fs) const;

  void _completion_handler(
      unsigned start_check_index,
      const std::list<check_drive_size_detail::fs_stat>& result);

 public:
  check_drive_size(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   time_point first_start_expected,
                   duration inter_check_delay,
                   duration check_interval,
                   const std::string& serv,
                   const std::string& cmd_name,
                   const std::string& cmd_line,
                   const rapidjson::Value& args,
                   const engine_to_agent_request_ptr& cnf,
                   check::completion_handler&& handler,
                   const checks_statistics::pointer& stat);

  virtual ~check_drive_size() = default;

  std::shared_ptr<check_drive_size> shared_from_this() {
    return std::static_pointer_cast<check_drive_size>(
        check::shared_from_this());
  }

  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  static void thread_kill();
};

}  // namespace com::centreon::agent

#endif  // CENTREON_AGENT_NATIVE_DRIVE_SIZE_HH
