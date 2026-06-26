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
constexpr std::array<std::pair<std::string_view, e_drive_type>, 35>
    _drive_type = {
        std::make_pair("hrunknown", hr_unknown),
        std::make_pair("hrstorageram", hr_storage_ram),
        std::make_pair("hrstoragevirtualmemory", hr_storage_virtual_memory),
        std::make_pair("hrstoragefixeddisk", hr_storage_fixed_disk),
        std::make_pair("hrstorageremovabledisk", hr_storage_removable_disk),
        std::make_pair("hrstoragefloppydisk", hr_storage_floppy_disk),
        std::make_pair("hrstoragecompactdisc", hr_storage_compact_disc),
        std::make_pair("hrstorageramdisk", hr_storage_ram_disk),
        std::make_pair("hrstorageflashmemory", hr_storage_flash_memory),
        std::make_pair("hrstoragenetworkdisk", hr_storage_network_disk)};

std::string drive_type_to_string(unsigned drive_type) {
  std::string ret;
  for (const auto& to_test : _drive_type) {
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
filter::filter(const rapidjson::Value& args) : _drive_type_filter(0xFFFFFFFFU) {
  if (args.IsObject()) {
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
          for (const auto& [label, flag] : _drive_type) {
            if (RE2::FullMatch(label, *_filter_fs_type)) {
              _drive_type_filter |= flag;
            }
          }
          // no drive type filter => allow all
          if (_drive_type_filter == 0) {
            _drive_type_filter = 0xFFFFFFFFU;
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
 * @brief test a fs
 *
 * @param fs
 * @param mount_point (linux only)
 * @param fs_type e_drive_fs_type mask
 * @return true allowed by filter
 * @return false
 */
bool filter::is_allowed(const std::string_view& fs,
                        const std::string_view& fs_type,
                        const std::string_view& mount_point,
                        e_drive_type media_type,
                        const std::shared_ptr<spdlog::logger>& logger) const {
  if (!(_drive_type_filter & media_type)) {
    SPDLOG_LOGGER_TRACE(
        logger, "fs refused because of his drive type (network, removable...)");
    return false;
  }

  if (_filter_exclude_fs && RE2::FullMatch(fs, *_filter_exclude_fs)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of exclude-fs", fs);
    return false;
  }

  if (_filter_fs && !RE2::FullMatch(fs, *_filter_fs)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of filter-fs", fs);
    return false;
  }

  if (_filter_exclude_mountpoint &&
      RE2::FullMatch(mount_point, *_filter_exclude_mountpoint)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of exclude-mountpoint", fs);
    return false;
  }

  if (_filter_mountpoint && !RE2::FullMatch(mount_point, *_filter_mountpoint)) {
    SPDLOG_LOGGER_TRACE(logger, "{} refused because of filter-mountpoint", fs);
    return false;
  }

  if (_filter_fs_type && !RE2::FullMatch(fs_type, *_filter_fs_type)) {
    // there are special filter words, we traduce them and reapply filter
    std::string new_fs_type = "hrfs";
    new_fs_type += fs_type;
    if (!RE2::FullMatch(new_fs_type, *_filter_fs_type)) {
      SPDLOG_LOGGER_TRACE(logger, "{} refused because of filter-type", fs);
      return false;
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
