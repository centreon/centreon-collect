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
 * Values lower than hr_fs_other are storage types (hrstorage*), the others are
 * file system formats (hrfs*).
 *
 */
enum e_drive_fs_type : uint64_t {
  // not 0 in order to be selectable in masks (unknown drive type or format
  // unknown by the agent are allowed when there is no filter)
  hr_unknown = 1ULL << 0,
  hr_storage_ram = 1ULL << 1,
  hr_storage_virtual_memory = 1ULL << 2,
  hr_storage_fixed_disk = 1ULL << 3,
  hr_storage_removable_disk = 1ULL << 4,
  hr_storage_floppy_disk = 1ULL << 5,
  hr_storage_compact_disc = 1ULL << 6,
  hr_storage_ram_disk = 1ULL << 7,
  hr_storage_flash_memory = 1ULL << 8,
  hr_storage_network_disk = 1ULL << 9,
  hr_fs_other = 1ULL << 10,  // keep it always in first fs value
  hr_fs_unknown = 1ULL << 11,
  hr_fs_berkeley_ffs = 1ULL << 12,
  hr_fs_sys5_fs = 1ULL << 13,
  hr_fs_fat = 1ULL << 14,
  hr_fs_hpfs = 1ULL << 15,
  hr_fs_hfs = 1ULL << 16,
  hr_fs_mfs = 1ULL << 17,
  hr_fs_ntfs = 1ULL << 18,
  hr_fs_vnode = 1ULL << 19,
  hr_fs_journaled = 1ULL << 20,
  hr_fs_iso9660 = 1ULL << 21,
  hr_fs_rock_ridge = 1ULL << 22,
  hr_fs_nfs = 1ULL << 23,
  hr_fs_netware = 1ULL << 24,
  hr_fs_afs = 1ULL << 25,
  hr_fs_dfs = 1ULL << 26,
  hr_fs_appleshare = 1ULL << 27,
  hr_fs_rfs = 1ULL << 28,
  hr_fs_dgcfs = 1ULL << 29,
  hr_fs_bfs = 1ULL << 30,
  hr_fs_fat32 = 1ULL << 31,
  hr_fs_linux_ext2 = 1ULL << 32,
  hr_fs_linux_ext4 = 1ULL << 33,
  hr_fs_exfat = 1ULL << 34
};
std::string drive_type_to_string(uint64_t drive_type);
e_drive_fs_type drive_fs_type_from_string(const std::string_view& str);

/**
 * @brief user can check only some fs by using filters
 * This is the goal of this class
 *
 */
class filter {
  // mask of allowed hrstorage* flags
  uint64_t _drive_type_filter;
  // mask of allowed hrfs* flags
  uint64_t _format_filter;

  std::unique_ptr<re2::RE2> _filter_fs, _filter_exclude_fs, _filter_fs_type;
  std::unique_ptr<re2::RE2> _filter_mountpoint, _filter_exclude_mountpoint;

 public:
  filter(const rapidjson::Value& args);

  bool is_fs_allowed(const std::string_view& fs,
                     const std::string_view& mount_point,
                     const std::shared_ptr<spdlog::logger>& logger) const;
  bool is_allowed(const std::string_view& fs,
                  e_drive_fs_type format_type,
                  const std::string_view& unknown_format_type,
                  e_drive_fs_type media_type,
                  const std::shared_ptr<spdlog::logger>& logger) const;
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

  bool is_used_more_than_threshold(common::threshold threshold) const {
    return threshold.is_triggered(used);
  }

  bool is_free_less_than_threshold(common::threshold threshold) const {
    return threshold.is_triggered(total - used);
  }

  bool is_used_more_than_prct_threshold(common::threshold threshold) const {
    if (!total) {
      return true;
    }
    return threshold.is_triggered((used * 10000.0) / total);
  }

  bool is_free_less_than_prct_threshold(common::threshold threshold) const {
    if (!total) {
      return true;
    }
    return threshold.is_triggered((total - used) * 10000.0 / total);
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

  bool has_to_stop_wait() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(_queue_m) {
    return !_active || !_queue.empty();
  }

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
  common::threshold _warning;  // value in bytes or percent * 100
  common::threshold _critical;

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
                   const Service& serv,
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
