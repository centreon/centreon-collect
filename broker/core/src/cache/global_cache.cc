/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::cache;

global_cache::lock::lock() : _lock(&global_cache::_instance->_protect) {}

constexpr absl::string_view checksum_extension = ".chk";

inline std::string operator+(const std::string& left,
                             const absl::string_view& to_append) {
  std::string ret(left);
  ret.append(to_append.data(), to_append.length());
  return ret;
}

std::shared_ptr<global_cache> global_cache::_instance;

global_cache::global_cache(const std::string& file_path,
                           const std::shared_ptr<asio::io_context>& io_context)
    : _file_path(file_path),
      _file_size(0),
      _io_context(io_context),
      _checksum_timer(*io_context),
      _checksum_to_compute(false) {}

global_cache::~global_cache() {
  write_checksum();
}

void global_cache::open(size_t initial_size_on_create, const void* address) {
  {
    absl::WriterMutexLock l(&_protect);
    try {
      struct ::stat exist_info;
      // size must be a multiple of uint64_t size
      if (!::stat(_file_path.c_str(), &exist_info) &&
          S_ISREG(exist_info.st_mode) && (exist_info.st_size & 0x07) == 0) {
        _file_size = exist_info.st_size;
        _file = std::make_unique<managed_mapped_file>(
            interprocess::open_only, _file_path.c_str(), address);
        uint64_t chk = calc_checksum();

        if (chk != read_checksum()) {
          SPDLOG_LOGGER_ERROR(log_v2::core(),
                              "bad checksum on {} =>erase it and recreate",
                              _file_path);
        } else {
          this->managed_map(false);
          return;
        }
      }
    } catch (const std::exception& e) {
      std::string err_detail = fmt::format(
          "corrupted cache file {} or checksum_file {} => recreate {}",
          _file_path, _file_path + checksum_extension, e.what());

      SPDLOG_LOGGER_ERROR(log_v2::core(), err_detail);
      _file.reset();
      _file_size = 0;
      ::remove(_file_path.c_str());
    }

    SPDLOG_LOGGER_DEBUG(log_v2::core(), "create file {}", _file_path);

    ::remove(_file_path.c_str());
    ::remove((_file_path + checksum_extension).c_str());
    grow(initial_size_on_create);
    this->managed_map(true);
    write_checksum_no_lock();
  }
}

/**
 * @brief calc the checksum of the data part of the file
 * beware, this method don't lock _protect
 * in order to improve performance, we sum uint64
 *
 * @return uint64_t checksum value
 */
uint64_t global_cache::calc_checksum() const {
  uint64_t checksum = 0;
  const uint64_t* to_sum = static_cast<const uint64_t*>(_file->get_address());
  const uint64_t* end = reinterpret_cast<const uint64_t*>(
      static_cast<const uint8_t*>(_file->get_address()) + _file->get_size());
  for (; to_sum != end; ++to_sum) {
    checksum += *to_sum;
  }
  return checksum;
}

/**
 * @brief this function grow file
 * first it's release mapping, grow file on disk and remap
 * managed_map need to be called after
 * _protect is not locked inside
 *
 *
 * @param new_size
 * @param address used only for tests
 */
void global_cache::grow(size_t new_size, void* address) {
  if (new_size <= _file_size) {
    return;
  }
  SPDLOG_LOGGER_DEBUG(log_v2::core(), "resize file {} from {} to {}",
                      _file_path, _file_size, new_size);
  if (_file) {
    _file->flush();
    _file.reset();
    _file_size = 0;
  }

  // need to have a multiple of sizeof(uint64_t) size
  new_size = ((new_size + 7) / 8) * 8;
  struct stat exist;
  try {
    // file doesn't exist
    if (stat(_file_path.c_str(), &exist) || !S_ISREG(exist.st_mode)) {
      ::remove(_file_path.c_str());
      SPDLOG_LOGGER_DEBUG(log_v2::core(),
                          "file {} removed or not a file => remove and create",
                          _file_path);
      _file = std::make_unique<managed_mapped_file>(
          interprocess::create_only, _file_path.c_str(), new_size, address);
      _file_size = new_size;
    } else {  // file exist
      managed_mapped_file::grow(_file_path.c_str(), new_size);
      _file = std::make_unique<managed_mapped_file>(
          interprocess::open_only, _file_path.c_str(), address);
      _file_size = new_size;
    }
  } catch (const std::exception& e) {
    std::string err_msg = fmt::format("fail to map file {} to size {} : {}",
                                      _file_path, new_size, e.what());
    SPDLOG_LOGGER_ERROR(log_v2::core(), err_msg);
    _file.reset();
    _file_size = 0;
    throw msg_fmt(err_msg);
  }
}

/**
 * @brief this handler is called when an allocation exception occurs
 * it increases file size by 256Mo
 *
 */
void global_cache::allocation_exception_handler() {
  {
    absl::WriterMutexLock l(&_protect);
    grow(_file_size + 0x1000000);
    this->managed_map(false);
    write_checksum_no_lock();
  }
}

/**
 * @brief this static method create a global_cache_data object
 *
 * @param file_path path of the file
 * @param initial_size initial file size on creation or error, 1Mo by default
 * @param address where to map file in memory, 0 means system decides where
 * @return global_cache::pointer
 */
global_cache::pointer global_cache::load(
    const std::string& file_path,
    const std::shared_ptr<asio::io_context>& io_context,
    size_t initial_size,
    const void* address) {
  if (!_instance) {
    _instance = pointer(new global_cache_data(file_path, io_context));
    _instance->open(initial_size, address);
  }
  return _instance;
}

/**
 * @brief reset the singleton pointer
 *
 */
void global_cache::unload() {
  if (_instance) {
    _instance->cancel_checksum_timer();
  }
  _instance.reset();
}

/**
 * @brief Get the start address of the mapping
 *
 * @return const void*
 */
const void* global_cache::get_address() const {
  if (_file) {
    return _file->get_address();
  }
  return nullptr;
}

void global_cache::start_checksum_timer() {
  absl::WriterMutexLock l(&_protect);
  _checksum_timer.expires_after(std::chrono::seconds(30));
  _checksum_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->checksum_timer_handler(err);
      });
}

void global_cache::cancel_checksum_timer() {
  absl::WriterMutexLock l(&_protect);
  _checksum_timer.cancel();
}

void global_cache::checksum_timer_handler(
    const boost::system::error_code& err) {
  if (err) {
    return;
  }
  write_checksum();
  start_checksum_timer();
}

void global_cache::write_checksum() const {
  absl::ReaderMutexLock l(&_protect);
  write_checksum_no_lock();
}

void global_cache::write_checksum_no_lock() const {
  if (!_checksum_to_compute) {
    return;
  }
  std::string check_path = _file_path + checksum_extension;
  std::ofstream f(check_path.c_str(),
                  std::ios_base::trunc | std::ios_base::out);
  f << calc_checksum();
  _checksum_to_compute = false;
}

uint64_t global_cache::read_checksum() const {
  std::string check_path = _file_path + checksum_extension;
  std::ifstream f(check_path.c_str());
  uint64_t ret;
  f >> ret;
  return ret;
}
