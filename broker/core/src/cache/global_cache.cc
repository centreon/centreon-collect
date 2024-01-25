/**
* Copyright 2023 Centreon
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

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::cache;

global_cache::lock::lock() : _lock(&global_cache::_instance->_protect) {}

inline std::string operator+(const std::string& left,
                             const std::string_view& to_append) {
  std::string ret(left);
  ret.append(to_append.data(), to_append.length());
  return ret;
}

std::shared_ptr<global_cache> global_cache::_instance;

global_cache::global_cache(const std::string& file_path)
    : _file_size(0), _file_path(file_path) {}

global_cache::~global_cache() {
  if (_file) {
    // file is gracefully closed => no dirty
    bool* dirty = _file->find<bool>("dirty").first;
    if (dirty) {
      *dirty = false;
    }
  }
}

void global_cache::_open(size_t initial_size_on_create, const void* address) {
  {
    try {
      struct ::stat exist_info;
      // size must be a multiple of uint64_t size
      if (!::stat(_file_path.c_str(), &exist_info) &&
          S_ISREG(exist_info.st_mode) && (exist_info.st_size & 0x07) == 0) {
        _file_size = exist_info.st_size;
        _file = std::make_unique<managed_mapped_file>(
            interprocess::open_only, _file_path.c_str(), address);
        bool* dirty = _file->find<bool>("dirty").first;
        if (dirty && !*dirty) {
          // dirty will be erased by destructor
          *dirty = true;
          SPDLOG_LOGGER_INFO(log_v2::core(), "global_cache open file {}",
                             _file_path);
          this->managed_map(false);
          return;
        } else {
          SPDLOG_LOGGER_ERROR(
              log_v2::core(),
              "global_cache dirty flag not reset => erase file and recreate");
        }
      }
    } catch (const boost::exception& e) {
      std::string err_detail =
          fmt::format("corrupted cache file {} => recreate {}", _file_path,
                      boost::diagnostic_information(e));

      SPDLOG_LOGGER_ERROR(log_v2::core(), err_detail);
      _file.reset();
      _file_size = 0;
      ::remove(_file_path.c_str());
    } catch (const std::exception& e) {
      std::string err_detail = fmt::format(
          "corrupted cache file {} => recreate {}", _file_path, e.what());

      SPDLOG_LOGGER_ERROR(log_v2::core(), err_detail);
      _file.reset();
      _file_size = 0;
      ::remove(_file_path.c_str());
    }

    SPDLOG_LOGGER_INFO(log_v2::core(), "global_cache create file {}",
                       _file_path);

    ::remove(_file_path.c_str());
    _grow(initial_size_on_create);
    *_file->find_or_construct<bool>("dirty")() = true;
    try {
      this->managed_map(true);
    } catch (const boost::interprocess::bad_alloc& e) {
      SPDLOG_LOGGER_ERROR(log_v2::core(),
                          "allocation error: {}, too small initial file size?",
                          boost::diagnostic_information(e));
      throw;
    }
  }
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
void global_cache::_grow(size_t new_size, void* address) {
  if (new_size <= _file_size) {
    return;
  }
  SPDLOG_LOGGER_DEBUG(log_v2::core(), "resize file {} from {} to {}",
                      _file_path, _file_size, new_size);
  size_t old_size = 0;
  if (_file) {
    _file->flush();
    _file.reset();
    old_size = _file_size;
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
      managed_mapped_file::grow(_file_path.c_str(), new_size - old_size);
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
    _grow(_file_size + 0x20000000);
    this->managed_map(false);
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
global_cache::pointer global_cache::load(const std::string& file_path,
                                         size_t initial_size,
                                         const void* address) {
  if (!_instance) {
    _instance = pointer(new global_cache_data(file_path));
    _instance->_open(initial_size, address);
  }
  return _instance;
}

/**
 * @brief reset the singleton pointer
 *
 */
void global_cache::unload() {
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
