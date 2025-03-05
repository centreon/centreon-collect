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

#include "drive_size.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;

static std::shared_ptr<
    com::centreon::agent::check_drive_size_detail::drive_size_thread>
    _worker;
static std::thread* _worker_thread = nullptr;

namespace com::centreon::agent::check_drive_size_detail {

/********************************************************************************
 *              filter
 *********************************************************************************/

/**
 * @brief as filter parameter is a regex, we need to apply the regex on each
 * line of this array
 *
 */
constexpr std::array<std::pair<std::string_view, e_drive_fs_type>, 35>
    _fs_type = {
        std::make_pair("hrunknown", hr_unknown),
        std::make_pair("hrstorageram", hr_storage_ram),
        std::make_pair("hrstoragevirtualmemory", hr_storage_virtual_memory),
        std::make_pair("hrstoragefixeddisk", hr_storage_fixed_disk),
        std::make_pair("hrstorageremovabledisk", hr_storage_removable_disk),
        std::make_pair("hrstoragefloppydisk", hr_storage_floppy_disk),
        std::make_pair("hrstoragecompactdisc", hr_storage_compact_disc),
        std::make_pair("hrstorageramdisk", hr_storage_ram_disk),
        std::make_pair("hrstorageflashmemory", hr_storage_flash_memory),
        std::make_pair("hrstoragenetworkdisk", hr_storage_network_disk),
        std::make_pair("hrfsother", hr_fs_other),
        std::make_pair("hrfsunknown", hr_fs_unknown),
        std::make_pair("hrfsberkeleyffs", hr_fs_berkeley_ffs),
        std::make_pair("hrfssys5fs", hr_fs_sys5_fs),
        std::make_pair("hrfsfat", hr_fs_fat),
        std::make_pair("hrfshpfs", hr_fs_hpfs),
        std::make_pair("hrfshfs", hr_fs_hfs),
        std::make_pair("hrfsmfs", hr_fs_mfs),
        std::make_pair("hrfsntfs", hr_fs_ntfs),
        std::make_pair("hrfsvnode", hr_fs_vnode),
        std::make_pair("hrfsjournaled", hr_fs_journaled),
        std::make_pair("hrfsiso9660", hr_fs_iso9660),
        std::make_pair("hrfsrockridge", hr_fs_rock_ridge),
        std::make_pair("hrfsnfs", hr_fs_nfs),
        std::make_pair("hrfsnetware", hr_fs_netware),
        std::make_pair("hrfsafs", hr_fs_afs),
        std::make_pair("hrfsdfs", hr_fs_dfs),
        std::make_pair("hrfsappleshare", hr_fs_appleshare),
        std::make_pair("hrfsrfs", hr_fs_rfs),
        std::make_pair("hrfsdgcfs", hr_fs_dgcfs),
        std::make_pair("hrfsbfs", hr_fs_bfs),
        std::make_pair("hrfsfat32", hr_fs_fat32),
        std::make_pair("hrfslinuxext2", hr_fs_linux_ext2),
        std::make_pair("hrfslinuxext4", hr_fs_linux_ext4),
        std::make_pair("hrfsexfat", hr_fs_exfat)};

/**
 * @brief Construct a new filter::filter object
 *
 *
 * @param args json array that can contain these keys:
 *    filter-storage-type or filter-type
 *    filter-fs
 *    filter-exclude-fs
 *    filter-mountpoint
 *    filter-exclude-mountpoint
 */
filter::filter(const rapidjson::Value& args) : _fs_type_filter(0xFFFFFFFFU) {
  if (args.IsObject()) {
    for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
         ++member_iter) {
      if (member_iter->name == "filter-storage-type" ||
          member_iter->name == "filter-type") {
        if (member_iter->value.IsString() &&
            member_iter->value.GetStringLength() > 0) {
          std::string sz_regexp(member_iter->value.GetString());
          boost::to_lower(sz_regexp);
          re2::RE2 filter_typ_re(sz_regexp);
          if (!filter_typ_re.ok()) {
            throw exceptions::msg_fmt(
                "invalid regex for filter-storage-type: {}",
                member_iter->value.GetString());
          }
          _fs_type_filter = 0;
          for (const auto& [label, flag] : _fs_type) {
            if (RE2::FullMatch(label, filter_typ_re)) {
              _fs_type_filter |= flag;
            }
          }
        }
      } else if (member_iter->name == "filter-fs" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        _filter_fs = std::make_unique<re2::RE2>(member_iter->value.GetString());
        if (!_filter_fs->ok()) {
          throw exceptions::msg_fmt("invalid regex for filter-fs: {}",
                                    member_iter->value.GetString());
        }
      } else if (member_iter->name == "exclude-fs" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        _filter_exclude_fs =
            std::make_unique<re2::RE2>(member_iter->value.GetString());
        if (!_filter_exclude_fs->ok()) {  // NOLINT
          throw exceptions::msg_fmt("invalid regex for filter-exclude-fs: {}",
                                    member_iter->value.GetString());
        }
      } else if (member_iter->name == "filter-mountpoint" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        _filter_mountpoint =
            std::make_unique<re2::RE2>(member_iter->value.GetString());
        if (!_filter_mountpoint->ok()) {
          throw exceptions::msg_fmt("invalid regex for filter-mountpoint: {}",
                                    member_iter->value.GetString());
        }
      } else if (member_iter->name == "exclude-mountpoint" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        _filter_exclude_mountpoint =
            std::make_unique<re2::RE2>(member_iter->value.GetString());
        if (!_filter_exclude_mountpoint->ok()) {
          throw exceptions::msg_fmt(
              "invalid regex for filter-exclude-mountpoint: {}",
              member_iter->value.GetString());
        }
      }
    }
  }
}

/**
 * @brief test if fs has yet been tested and yet allowed
 *
 * @param fs
 * @return true tested and allowed
 * @return false not tested
 */
bool filter::is_fs_yet_allowed(const std::string_view& fs) const {
  absl::MutexLock l(&_protect);
  return _cache_allowed_fs.find(fs) != _cache_allowed_fs.end();
}

/**
 * @brief test if fs has yet been tested and yet excluded
 *
 * @param fs
 * @return true tested and excluded
 * @return false not tested
 */
bool filter::is_fs_yet_excluded(const std::string_view& fs) const {
  absl::MutexLock l(&_protect);
  return _cache_excluded_fs.find(fs) != _cache_excluded_fs.end();
}

/**
 * @brief test a fs
 *
 * @param fs
 * @param mount_point (linux only)
 * @param fs_type e_drive_fs_type mask
 * @return true allowed by filter
 * @return false
 */
bool filter::is_allowed(const std::string_view& fs,
                        const std::string_view& mount_point,
                        e_drive_fs_type fs_type) {
  if (!(_fs_type_filter & fs_type)) {
    return false;
  }

  absl::MutexLock l(&_protect);

  bool yet_allowed = _cache_allowed_fs.find(fs) != _cache_allowed_fs.end();
  bool yet_excluded = _cache_excluded_fs.find(fs) != _cache_excluded_fs.end();
  if (yet_excluded) {
    return false;
  }

  if (!yet_allowed) {
    if (_filter_exclude_fs && RE2::FullMatch(fs, *_filter_exclude_fs)) {
      _cache_excluded_fs.emplace(fs);
      return false;
    }

    if (_filter_fs) {
      if (RE2::FullMatch(fs, *_filter_fs)) {
        _cache_allowed_fs.emplace(fs);
      } else {
        _cache_excluded_fs.emplace(fs);
        return false;
      }
    } else {
      _cache_allowed_fs.emplace(fs);
    }
  }

  yet_allowed = _cache_allowed_mountpoint.find(mount_point) !=
                _cache_allowed_mountpoint.end();
  yet_excluded = _cache_excluded_mountpoint.find(mount_point) !=
                 _cache_excluded_mountpoint.end();
  if (yet_excluded) {
    return false;
  }

  if (!yet_allowed) {
    if (_filter_exclude_mountpoint &&
        RE2::FullMatch(mount_point, *_filter_exclude_mountpoint)) {
      _cache_excluded_mountpoint.emplace(mount_point);
      return false;
    }

    if (_filter_mountpoint) {
      if (RE2::FullMatch(mount_point, *_filter_mountpoint)) {
        _cache_allowed_mountpoint.emplace(mount_point);
      } else {
        _cache_excluded_mountpoint.emplace(mount_point);
        return false;
      }
    } else {
      _cache_allowed_mountpoint.emplace(mount_point);
    }
  }

  return true;
}

/********************************************************************************
 *              drive_size_thread
 *********************************************************************************/

drive_size_thread::get_fs_stats drive_size_thread::os_fs_stats;

/**
 * @brief function run in a separate thread started by
 * check_drive_size::start_check
 *
 */
void drive_size_thread::run() {
  auto keep_object_alive = shared_from_this();
  while (_active) {
    absl::MutexLock l(&_queue_m);
    _queue_m.Await(absl::Condition(this, &drive_size_thread::has_to_stop_wait));
    if (!_active) {
      return;
    }
    time_point now = std::chrono::system_clock::now();
    while (!_queue.empty()) {
      if (_queue.begin()->timeout < now) {
        _queue.pop_front();
      } else {
        break;
      }
    }

    if (!_queue.empty()) {
      auto to_execute = _queue.begin();
      std::list<fs_stat> stats =
          os_fs_stats(*to_execute->request_filter, _logger);
      // main code of this program is not thread safe, so we use io_context
      // launched from main thread to call callback
      _io_context->post(
          [result = std::move(stats),
           completion_handler = std::move(to_execute->handler)]() {
            completion_handler(result);
          });
      _queue.erase(to_execute);
    }
  }
}

/**
 * @brief wake up thread and tell him it's time to die
 *
 */
void drive_size_thread::kill() {
  absl::MutexLock l(&_queue_m);
  _active = false;
}

/**
 * @brief start an asynchronous check
 *
 * @tparam handler_type
 * @param request_filter
 * @param timeout
 * @param handler
 */
template <class handler_type>
void drive_size_thread::async_get_fs_stats(
    const std::shared_ptr<filter>& request_filter,
    const time_point& timeout,
    handler_type&& handler) {
  absl::MutexLock lck(&_queue_m);
  _queue.push_back(
      {request_filter, std::forward<handler_type>(handler), timeout});
}

}  // namespace com::centreon::agent::check_drive_size_detail

/********************************************************************************
 *              check_drive_size
 *********************************************************************************/

check_drive_size::check_drive_size(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration check_interval,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat),
      _filter(std::make_shared<check_drive_size_detail::filter>(args)),
      _prct_threshold(false),
      _free_threshold(false),
      _warning(0),
      _critical(0),
      _fs_test(&check_drive_size::_no_test) {
  using namespace std::literals;
  try {
    if (args.IsObject()) {
      common::rapidjson_helper helper(args);

      if (args.HasMember("unit")) {
        _prct_threshold = helper.get_string("unit", "%") == "%"sv;
      } else {
        _prct_threshold = helper.get_string("units", "%") == "%"sv;
      }
      _free_threshold = helper.get_bool("free", false);

      _warning = helper.get_uint64_t("warning", 0);
      _critical = helper.get_uint64_t("critical", 0);
      if (_prct_threshold) {
        if (_warning || _critical) {
          _warning *= 100;
          _critical *= 100;
          _fs_test = _free_threshold ? &check_drive_size::_prct_free_test
                                     : &check_drive_size::_prct_used_test;
        }
      } else {
        if (_warning || _critical) {
          _fs_test = _free_threshold ? &check_drive_size::_free_test
                                     : &check_drive_size::_used_test;
        }
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        _logger, "check_drive_size fail to parse check params: {}", e.what());
    throw;
  }
}

/**
 * @brief used in case of no threshold
 *
 * @param fs
 * @return e_status
 */
e_status check_drive_size::_no_test(
    [[maybe_unused]] const check_drive_size_detail::fs_stat& fs) const {
  return e_status::ok;
}

/**
 * @brief test used fs with fixed thresholds
 *
 * @param fs
 * @return e_status
 */
e_status check_drive_size::_used_test(
    const check_drive_size_detail::fs_stat& fs) const {
  if (_critical && fs.is_used_more_than_threshold(_critical)) {
    return e_status::critical;
  }
  if (_warning && fs.is_used_more_than_threshold(_warning)) {
    return e_status::warning;
  }
  return e_status::ok;
}

/**
 * @brief test used fs with percent thresholds
 *
 * @param fs
 * @return e_status
 */
e_status check_drive_size::_prct_used_test(
    const check_drive_size_detail::fs_stat& fs) const {
  if (_critical && fs.is_used_more_than_prct_threshold(_critical)) {
    return e_status::critical;
  }
  if (_warning && fs.is_used_more_than_prct_threshold(_warning)) {
    return e_status::warning;
  }
  return e_status::ok;
}

/**
 * @brief test free fs with fixed thresholds
 *
 * @param fs
 * @return e_status
 */
e_status check_drive_size::_free_test(
    const check_drive_size_detail::fs_stat& fs) const {
  if (_critical && fs.is_free_less_than_threshold(_critical)) {
    return e_status::critical;
  }
  if (_warning && fs.is_free_less_than_threshold(_warning)) {
    return e_status::warning;
  }
  return e_status::ok;
}

/**
 * @brief test free fs with percent thresholds
 *
 * @param fs
 * @return e_status
 */
e_status check_drive_size::_prct_free_test(
    const check_drive_size_detail::fs_stat& fs) const {
  if (_critical && fs.is_free_less_than_prct_threshold(_critical)) {
    return e_status::critical;
  }
  if (_warning && fs.is_free_less_than_prct_threshold(_warning)) {
    return e_status::warning;
  }
  return e_status::ok;
}

/**
 * @brief start a check
 * start _worker thread if not yet done and pass query to it
 *
 * @param timeout
 */
void check_drive_size::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  if (!_worker_thread) {
    _worker = std::make_shared<check_drive_size_detail::drive_size_thread>(
        _io_context, _logger);
    _worker_thread = new std::thread([worker = _worker] { worker->run(); });
  }

  unsigned running_check_index = _get_running_check_index();

  _worker->async_get_fs_stats(
      _filter, std::chrono::system_clock::now() + timeout,
      [me = shared_from_this(), running_check_index](
          const std::list<check_drive_size_detail::fs_stat>& result) {
        me->_completion_handler(running_check_index, result);
      });
}

/**
 * @brief called by _worker once work is done
 * As it is not thread safe, _worker use io_context to post result
 *
 * @param start_check_index
 * @param result
 */
void check_drive_size::_completion_handler(
    unsigned start_check_index,
    const std::list<check_drive_size_detail::fs_stat>& result) {
  e_status status = e_status::ok;

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  for (const auto& fs : result) {
    e_status fs_status = (this->*_fs_test)(fs);
    if (fs_status > status) {
      status = fs_status;
    }
    if (fs_status != e_status::ok) {
      if (!output.empty()) {
        output.push_back(' ');
      }
      output += fs_status == e_status::critical ? "CRITICAL: " : "WARNING: ";
      if (_prct_threshold) {
        output += fmt::format("{} Total: {}G Used: {:.2f}% Free: {:.2f}%",
                              fs.mount_point, fs.total / 1024 / 1024 / 1024,
                              fs.get_used_prct(), fs.get_free_prct());
      } else {
        output += fmt::format("{} Total: {}G Used: {}G Free: {}G",
                              fs.mount_point, fs.total / 1024 / 1024 / 1024,
                              fs.used / 1024 / 1024 / 1024,
                              (fs.total - fs.used) / 1024 / 1024 / 1024);
      }
    }

    centreon::common::perfdata& perf = perfs.emplace_back();
    perf.name((_free_threshold ? "free_" : "used_") + fs.mount_point);

    if (_prct_threshold) {
      perf.unit("%");
      perf.min(0);
      perf.max(100.0);
      if (_warning) {
        perf.warning_low(0);
        perf.warning(static_cast<double>(_warning) / 100);
      }
      if (_critical) {
        perf.critical_low(0);
        perf.critical(static_cast<double>(_critical) / 100);
      }
      perf.value(_free_threshold ? fs.get_free_prct() : fs.get_used_prct());
    } else {
      perf.unit("B");
      perf.min(0);
      perf.max(fs.total);
      if (_warning) {
        perf.warning_low(0);
        perf.warning(_warning);
      }
      if (_critical) {
        perf.critical_low(0);
        perf.critical(_critical);
      }
      perf.value(_free_threshold ? (fs.total - fs.used) : fs.used);
    }
  }
  if (output.empty()) {
    using namespace std::literals;
    if (perfs.empty()) {
      output = "No storage found (filters issue)"sv;
      status = e_status::critical;
    } else {
      output = "OK: All storages are ok"sv;
    }
  }

  on_completion(start_check_index, status, perfs, {output});
}

/**
 * @brief stop _worker
 *
 */
void check_drive_size::thread_kill() {
  if (_worker_thread) {
    _worker->kill();
    _worker_thread->join();
    delete _worker_thread;
    _worker_thread = nullptr;
  }
}

void check_drive_size::help(std::ostream& help_stream) {
  help_stream <<
      R"(
- storage  params: 
    unit (default %): unit of threshold. If different from % threshold are in bytes
    free (default used): true: threshold is applied on free space and service become warning if free sapce is lower than threshold
                         false: threshold is applied on used space and service become warning if used space is higher than threshold
    warning: warning threshold
    critical: critical threshold
    filters:
      filter-storage-type: case insensitive regex to filter storage type it includes drive type (fixed, network...) and also fs type (fat32, ntfs..)
        types recognized by agent:
           hrunknown
           hrstoragefixeddisk
           hrstorageremovabledisk
           hrstoragecompactdisc
           hrstorageramdisk
           hrstoragenetworkdisk
           hrfsunknown
           hrfsfat
           hrfsntfs
           hrfsfat32
           hrfsexfat
      filter-fs: regex to filter filesystem
        Example: [C-D]:\\.*
      exclude-fs: regex to exclude filesystem
  An example of configuration:
  { 
    "check": "storage",
    "args": {
        "unit": "%",
        "free": false,
        "warning": 80,
        "critical": 90,
        "filter-storage-type": "hrstoragefixeddisk",
        "filter-fs": "[C-D]:\\"
    }
  }
  Examples of output:
    WARNING: C:\ Total: 322G Used: 39.54% Free: 60.46% CRITICAL: D:\ Total: 5G Used: 50.60% Free: 49.40%
  Metrics:
    if free flag = true
      free_C:\
      free_D:\
    if free flag = false
      used_C:\
      used_D:\
)";
}
