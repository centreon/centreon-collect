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
#include "absl/strings/ascii.h"
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
 * It contains both storage types (hrstorage*) and file system formats (hrfs*).
 * This array MUST be sorted by label because drive_fs_type_from_string does a
 * binary search on it.
 *
 */
constexpr std::array<std::pair<std::string_view, e_drive_fs_type>, 35>
    _drive_format_type = {
        std::make_pair("hrfsafs", hr_fs_afs),
        std::make_pair("hrfsappleshare", hr_fs_appleshare),
        std::make_pair("hrfsberkeleyffs", hr_fs_berkeley_ffs),
        std::make_pair("hrfsbfs", hr_fs_bfs),
        std::make_pair("hrfsdfs", hr_fs_dfs),
        std::make_pair("hrfsdgcfs", hr_fs_dgcfs),
        std::make_pair("hrfsexfat", hr_fs_exfat),
        std::make_pair("hrfsfat", hr_fs_fat),
        std::make_pair("hrfsfat32", hr_fs_fat32),
        std::make_pair("hrfshfs", hr_fs_hfs),
        std::make_pair("hrfshpfs", hr_fs_hpfs),
        std::make_pair("hrfsiso9660", hr_fs_iso9660),
        std::make_pair("hrfsjournaled", hr_fs_journaled),
        std::make_pair("hrfslinuxext2", hr_fs_linux_ext2),
        std::make_pair("hrfslinuxext4", hr_fs_linux_ext4),
        std::make_pair("hrfsmfs", hr_fs_mfs),
        std::make_pair("hrfsnetware", hr_fs_netware),
        std::make_pair("hrfsnfs", hr_fs_nfs),
        std::make_pair("hrfsntfs", hr_fs_ntfs),
        std::make_pair("hrfsother", hr_fs_other),
        std::make_pair("hrfsrfs", hr_fs_rfs),
        std::make_pair("hrfsrockridge", hr_fs_rock_ridge),
        std::make_pair("hrfssys5fs", hr_fs_sys5_fs),
        std::make_pair("hrfsunknown", hr_fs_unknown),
        std::make_pair("hrfsvnode", hr_fs_vnode),
        std::make_pair("hrstoragecompactdisc", hr_storage_compact_disc),
        std::make_pair("hrstoragefixeddisk", hr_storage_fixed_disk),
        std::make_pair("hrstorageflashmemory", hr_storage_flash_memory),
        std::make_pair("hrstoragefloppydisk", hr_storage_floppy_disk),
        std::make_pair("hrstoragenetworkdisk", hr_storage_network_disk),
        std::make_pair("hrstorageram", hr_storage_ram),
        std::make_pair("hrstorageramdisk", hr_storage_ram_disk),
        std::make_pair("hrstorageremovabledisk", hr_storage_removable_disk),
        std::make_pair("hrstoragevirtualmemory", hr_storage_virtual_memory),
        std::make_pair("hrunknown", hr_unknown)};

/**
 * @brief convert a flag mask into a human readable string
 *
 * @param drive_type mask of e_drive_fs_type flags
 * @return std::string labels of the flags set, separated by '|' (ex:
 * "hrstoragefixeddisk|hrfsntfs")
 */
std::string drive_type_to_string(unsigned drive_type) {
  std::string ret;
  for (const auto& to_test : _drive_format_type) {
    if (drive_type & to_test.second) {
      if (!ret.empty()) {
        ret.push_back('|');
        ret += to_test.first;
      } else {
        ret += to_test.first;
      }
    }
  }
  return ret;
}

/**
 * @brief get the e_drive_fs_type flag that matches exactly a label of
 * _drive_format_type (binary search, so the array must be sorted)
 *
 * @param str lower case label such as "hrfsntfs" or "hrstoragefixeddisk" or
 * "ntfs"
 * @return e_drive_fs_type the matching flag, hr_unknown if not found
 */
e_drive_fs_type drive_fs_type_from_string(const std::string_view& str) {
  auto comp = [](const decltype(_drive_format_type)::value_type& left,
                 const std::string_view& right) { return left.first < right; };

  auto found = std::lower_bound(_drive_format_type.begin(),
                                _drive_format_type.end(), str, comp);
  if (found != _drive_format_type.end() && found->first == str)
    return found->second;

  std::string hrfs_str = "hrfs";
  hrfs_str += str;
  found = std::lower_bound(_drive_format_type.begin(), _drive_format_type.end(),
                           hrfs_str, comp);
  if (found != _drive_format_type.end() && found->first == hrfs_str)
    return found->second;

  return e_drive_fs_type::hr_unknown;
}

/**
 * @brief Construct a new filter::filter object
 *
 *
 * @param args json array that can contain these keys:
 *    filter-storage-type or filter-type: regex applied on each label of
 *      _drive_format_type. Matching hrstorage* labels fill _drive_type_filter,
 *      matching hrfs* labels fill _format_filter. If no label of a family
 *      matches, every value of this family is allowed, except if the regex
 *      matches no label at all: in that case, it is applied on the format name
 *      returned by the os (ntfs, other_format...).
 *    filter-fs
 *    exclude-fs
 *    filter-mountpoint
 *    exclude-mountpoint
 * filter-fs, exclude-fs, filter-mountpoint and exclude-mountpoint regexes are
 * converted to lower case in order to be case insensitive (see is_fs_allowed)
 * @throw exceptions::msg_fmt if a regex is invalid
 */
filter::filter(const rapidjson::Value& args)
    : _drive_type_filter(std::numeric_limits<uint64_t>::max()),
      _format_filter(std::numeric_limits<uint64_t>::max()) {
  if (args.IsObject()) {
    std::string lower_re;
    for (auto member_iter = args.MemberBegin(); member_iter != args.MemberEnd();
         ++member_iter) {
      if (member_iter->name == "filter-storage-type" ||
          member_iter->name == "filter-type") {
        if (member_iter->value.IsString() &&
            member_iter->value.GetStringLength() > 0) {
          std::string sz_regexp(member_iter->value.GetString());
          boost::to_lower(sz_regexp);
          _filter_fs_type = std::make_unique<re2::RE2>(sz_regexp);
          if (!_filter_fs_type->ok()) {
            throw exceptions::msg_fmt(
                "invalid regex for filter-storage-type: {}",
                member_iter->value.GetString());
          }
          _drive_type_filter = 0;
          _format_filter = 0;
          for (const auto& [label, flag] : _drive_format_type) {
            if (RE2::FullMatch(label, *_filter_fs_type)) {
              if (flag >= hr_fs_other)
                _format_filter |= flag;
              else
                _drive_type_filter |= flag;
            }
          }
          // regex matches only hrstorage* labels => allow all formats
          // if it matches no label at all, it's a custom format regex (ntfs,
          // other_format...), so _format_filter stays at 0 and is_allowed
          // applies the regex on format name
          if (_format_filter == 0 && _drive_type_filter != 0) {
            _format_filter = std::numeric_limits<uint64_t>::max();
          }
          // no drive type filter => allow all
          if (_drive_type_filter == 0) {
            _drive_type_filter = std::numeric_limits<uint64_t>::max();
          }
        }
      } else if (member_iter->name == "filter-fs" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        lower_re = member_iter->value.GetString();
        absl::AsciiStrToLower(&lower_re);
        _filter_fs = std::make_unique<re2::RE2>(lower_re);
        if (!_filter_fs->ok()) {
          throw exceptions::msg_fmt("invalid regex for filter-fs: {}",
                                    member_iter->value.GetString());
        }
      } else if (member_iter->name == "exclude-fs" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        lower_re = member_iter->value.GetString();
        absl::AsciiStrToLower(&lower_re);
        _filter_exclude_fs = std::make_unique<re2::RE2>(lower_re);
        if (!_filter_exclude_fs->ok()) {  // NOLINT
          throw exceptions::msg_fmt("invalid regex for filter-exclude-fs: {}",
                                    member_iter->value.GetString());
        }
      } else if (member_iter->name == "filter-mountpoint" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        lower_re = member_iter->value.GetString();
        absl::AsciiStrToLower(&lower_re);
        _filter_mountpoint = std::make_unique<re2::RE2>(lower_re);
        if (!_filter_mountpoint->ok()) {
          throw exceptions::msg_fmt("invalid regex for filter-mountpoint: {}",
                                    member_iter->value.GetString());
        }
      } else if (member_iter->name == "exclude-mountpoint" &&
                 member_iter->value.IsString() &&
                 member_iter->value.GetStringLength() > 0) {
        lower_re = member_iter->value.GetString();
        absl::AsciiStrToLower(&lower_re);
        _filter_exclude_mountpoint = std::make_unique<re2::RE2>(lower_re);
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
 * @brief first filter step, test fs name and mount point against filter-fs,
 * exclude-fs, filter-mountpoint and exclude-mountpoint.
 * It needs no system call, so it is called before getting drive type and
 * format in order to avoid querying refused drives.
 * Test is case insensitive: fs and mount_point are converted to lower case as
 * the regexes.
 *
 * @param fs file system (C:\ on windows, /dev/sda1 on linux)
 * @param mount_point (linux only, empty on windows)
 * @param logger
 * @return true allowed by filter
 * @return false
 */
bool filter::is_fs_allowed(
    const std::string_view& fs,
    const std::string_view& mount_point,
    const std::shared_ptr<spdlog::logger>& logger) const {
  std::string lower_fs(fs);
  absl::AsciiStrToLower(&lower_fs);
  std::string lower_mount_point(mount_point);
  absl::AsciiStrToLower(&lower_mount_point);

  if (_filter_exclude_fs && RE2::FullMatch(lower_fs, *_filter_exclude_fs)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of exclude-fs", fs);
    return false;
  }

  if (_filter_fs && !RE2::FullMatch(lower_fs, *_filter_fs)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of filter-fs", fs);
    return false;
  }

  if (_filter_exclude_mountpoint &&
      RE2::FullMatch(lower_mount_point, *_filter_exclude_mountpoint)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of exclude-mountpoint", fs);
    return false;
  }

  if (_filter_mountpoint &&
      !RE2::FullMatch(lower_mount_point, *_filter_mountpoint)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of filter-mountpoint", fs);
    return false;
  }

  return true;
}

/**
 * @brief second filter step, test file system format and drive type against
 * filter-storage-type.
 * If format_type is not in _format_filter, filter-storage-type regex is
 * applied on unknown_format_type and on "hrfs" + unknown_format_type, so user
 * can allow formats unknown by the agent with other|<custom format>.
 * is_fs_allowed must have been called before.
 *
 * @param fs file system, only used for logs
 * @param format_type hrfs* flag of the fs format (hr_fs_unknown if not found)
 * @param unknown_format_type lower case format name returned by the os (ntfs,
 * exfat...)
 * @param media_type hrstorage* flag of the drive (fixed, network...)
 * @param logger
 * @return true allowed by filter
 * @return false
 */
bool filter::is_allowed(const std::string_view& fs,
                        e_drive_fs_type format_type,
                        const std::string_view& unknown_format_type,
                        e_drive_fs_type media_type,
                        const std::shared_ptr<spdlog::logger>& logger) const {
  // we can't known every fs formats, so we allow other|<custom format>
  if (!(_format_filter & format_type)) {
    if (_filter_fs_type &&
        !RE2::FullMatch(unknown_format_type, *_filter_fs_type)) {
      // there are special filter words, we traduce them and reapply filter
      std::string new_fs_type = "hrfs";
      new_fs_type += unknown_format_type;
      if (!RE2::FullMatch(new_fs_type, *_filter_fs_type)) {
        SPDLOG_LOGGER_TRACE(logger, "{} refused because of format-type", fs);
        return false;
      }
    }
  }

  if (!(_drive_type_filter & media_type)) {
    SPDLOG_LOGGER_TRACE(
        logger, "{} refused because of his drive type (network, removable...)",
        fs);
    return false;
  }

  SPDLOG_LOGGER_TRACE(logger, "{} allowed", fs);

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
    absl::MutexLock l(_queue_m);
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
      asio::post(*_io_context,
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
  absl::MutexLock l(_queue_m);
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
  absl::MutexLock lck(_queue_m);
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
    const Service& serv,
    const rapidjson::Value& args,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            serv,
            cnf,
            std::move(handler),
            stat),
      _filter(std::make_shared<check_drive_size_detail::filter>(args)),
      _prct_threshold(false),
      _free_threshold(false),
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

      // the default value should be empty "" , to disable the threshold
      _warning.extract_range(helper.get_string_or_int_as_string("warning", ""));
      _critical.extract_range(
          helper.get_string_or_int_as_string("critical", ""));

      _warning.set_default_low(0);
      _critical.set_default_low(0);

      if (!_warning.is_valid() || !_critical.is_valid()) {
        SPDLOG_LOGGER_ERROR(_logger, "check_drive_size invalid threshold");
        throw exceptions::msg_fmt("check_drive_size invalid threshold");
      }

      if (_prct_threshold) {
        if (!_warning.is_disabled() || !_critical.is_disabled()) {
          _warning.unit_multiplier(100);
          _critical.unit_multiplier(100);
          _fs_test = _free_threshold ? &check_drive_size::_prct_free_test
                                     : &check_drive_size::_prct_used_test;
        }
      } else {
        if (!_warning.is_disabled() || !_critical.is_disabled()) {
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
  if (!_critical.is_disabled() && fs.is_used_more_than_threshold(_critical)) {
    return e_status::critical;
  }
  if (!_warning.is_disabled() && fs.is_used_more_than_threshold(_warning)) {
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
  if (!_critical.is_disabled() &&
      fs.is_used_more_than_prct_threshold(_critical)) {
    return e_status::critical;
  }
  if (!_warning.is_disabled() &&
      fs.is_used_more_than_prct_threshold(_warning)) {
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
  if (!_critical.is_disabled() && fs.is_free_less_than_threshold(_critical)) {
    return e_status::critical;
  }
  if (!_warning.is_disabled() && fs.is_free_less_than_threshold(_warning)) {
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
  if (!_critical.is_disabled() &&
      fs.is_free_less_than_prct_threshold(_critical)) {
    return e_status::critical;
  }
  if (!_warning.is_disabled() &&
      fs.is_free_less_than_prct_threshold(_warning)) {
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
  const duration effective_timeout = get_custom_timeout().value_or(timeout);

  if (!_worker_thread) {
    _worker = std::make_shared<check_drive_size_detail::drive_size_thread>(
        _io_context, _logger);
    _worker_thread = new std::thread([worker = _worker] { worker->run(); });
  }

  unsigned running_check_index = _get_running_check_index();

  _worker->async_get_fs_stats(
      _filter, std::chrono::system_clock::now() + effective_timeout,
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
                              fs.mount_point.empty() ? fs.fs : fs.mount_point,
                              fs.total / 1024 / 1024 / 1024, fs.get_used_prct(),
                              fs.get_free_prct());
      } else {
        output += fmt::format("{} Total: {}G Used: {}G Free: {}G",
                              fs.mount_point.empty() ? fs.fs : fs.mount_point,
                              fs.total / 1024 / 1024 / 1024,
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
      _warning.set_pref_details_w(perf, 1 / 100.0);
      _critical.set_pref_details_c(perf, 1 / 100.0);
      perf.value(_free_threshold ? fs.get_free_prct() : fs.get_used_prct());
    } else {
      perf.unit("B");
      perf.min(0);
      perf.max(fs.total);
      _warning.set_pref_details_w(perf);
      _critical.set_pref_details_c(perf);
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
    free (default used): true: threshold is applied on free space.
                         false: threshold is applied on used space.
    warning: warning threshold
    critical: critical threshold
    filters:
      filter-storage-type: case insensitive regex to filter storage type it includes drive type (fixed, network...) and also fs type (fat32, ntfs..)
        types recognized by agent:
            hrunknown
            hrstorageram
            hrstoragevirtualmemory
            hrstoragefixeddisk
            hrstorageremovabledisk
            hrstoragefloppydisk
            hrstoragecompactdisc
            hrstorageramdisk
            hrstorageflashmemory
            hrstoragenetworkdisk
            hrfsother
            hrfsunknown
            hrfsberkeleyffs
            hrfssys5fs
            hrfsfat
            hrfshpfs
            hrfshfs
            hrfsmfs
            hrfsntfs
            hrfsvnode
            hrfsjournaled
            hrfsiso9660
            hrfsrockridge
            hrfsnfs
            hrfsnetware
            hrfsafs
            hrfsdfs
            hrfsappleshare
            hrfsrfs
            hrfsdgcfs
            hrfsbfs
            hrfsfat32
            hrfslinuxext2
            hrfslinuxext4
            hrfsexfat
        For other types, you can use this regular expression: other|<custom file system>
      filter-fs: regex to filter filesystem
        Example: [C-D]:\\.*
      exclude-fs: regex to exclude filesystem
  An example of configuration:
  { 
    "check": "storage",
    "args": {
        "unit": "%",
        "free": false,
        "warning": "80",
        "critical": "90",
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
