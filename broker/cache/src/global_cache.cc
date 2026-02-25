/**
 * Copyright 2023-2024 Centreon
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

#include "boost/system/detail/error_code.hpp"
#include "com/centreon/broker/cache/global_cache_data.hh"
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

// not defined here but in broker engine in order to be accessed by broker
// engine and every modules
// std::shared_ptr<global_cache> global_cache::_instance;

global_cache::global_cache(const std::shared_ptr<asio::io_context> io_context,
                           const std::string& file_path,
                           const std::shared_ptr<spdlog::logger>& logger,
                           unsigned grow_step,
                           unsigned nb_update_before_save,
                           std::chrono::system_clock::duration save_interval)
    : _io_context(io_context),
      _save_timer(*io_context),
      _grow_step(grow_step),
      _nb_update_before_save(nb_update_before_save),
      _save_interval(save_interval),
      _file_size(0),
      _dirty(nullptr),
      _modif_counter(0),
      _last_save_time(0),
      _file_path(file_path),
      _logger{logger} {
  SPDLOG_LOGGER_DEBUG(_logger, "cache create global_cache {:p}",
                      static_cast<const void*>(this));
}

global_cache::~global_cache() {
  SPDLOG_LOGGER_DEBUG(_logger, "cache destroy global_cache {:p}",
                      static_cast<const void*>(this));
  stop();
}

void global_cache::_start_save_timer() {
  _save_timer.expires_after(_save_interval);
  _save_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_save_timer_handler(err);
      });
}

void global_cache::_save_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  } else {
    absl::WriterMutexLock l(&_protect);
    _flush();
    _start_save_timer();
  }
}

void global_cache::_set_dirty_and_increment_modif() {
  *_dirty = true;
  ++_modif_counter;
  time_t now = time(nullptr);
  if (_modif_counter > _nb_update_before_save && _last_save_time != now) {
    _modif_counter = 0;
    _last_save_time = now;
    _flush();
    // reset save timer in order after _save_interval
    _start_save_timer();
  }
}

void global_cache::_flush() {
  if (_file && _dirty && *_dirty) {
    SPDLOG_LOGGER_TRACE(_logger, "cache: begin flush of {}", _file_path);
    _file->flush();
    *_dirty = false;
    _file->flush();
    SPDLOG_LOGGER_TRACE(_logger, "cache: end flush of {}", _file_path);
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
          SPDLOG_LOGGER_INFO(_logger, "global_cache open file {}", _file_path);
          this->managed_map(false);
          _dirty = dirty;
          return;
        } else {
          SPDLOG_LOGGER_ERROR(
              _logger,
              "global_cache dirty flag not reset => erase file and recreate");
          _file.reset();
          _file_size = 0;
        }
      }
    } catch (const boost::exception& e) {
      std::string err_detail =
          fmt::format("cache: corrupted cache file {} => recreate {}",
                      _file_path, boost::diagnostic_information(e));

      SPDLOG_LOGGER_ERROR(_logger, err_detail);
      _file.reset();
      _file_size = 0;
      ::remove(_file_path.c_str());
    } catch (const std::exception& e) {
      std::string err_detail =
          fmt::format("cache: corrupted cache file {} => recreate {}",
                      _file_path, e.what());

      SPDLOG_LOGGER_ERROR(_logger, err_detail);
      _file.reset();
      _file_size = 0;
      ::remove(_file_path.c_str());
    }

    SPDLOG_LOGGER_INFO(_logger, "cache: create file {}", _file_path);

    ::remove(_file_path.c_str());
    _grow(initial_size_on_create);
    _dirty = _file->find_or_construct<bool>("dirty")(false);
    try {
      this->managed_map(true);
    } catch (const boost::interprocess::bad_alloc& e) {
      SPDLOG_LOGGER_ERROR(
          _logger, "cache: allocation error: {}, too small initial file size?",
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
  SPDLOG_LOGGER_INFO(_logger, "cache: resize file {} from {} to {}", _file_path,
                     _file_size, new_size);
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
      SPDLOG_LOGGER_DEBUG(
          _logger, "cache: file {} removed or not a file => remove and create",
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
    std::string err_msg =
        fmt::format("cache:fail to map file {} to size {} : {}", _file_path,
                    new_size, e.what());
    SPDLOG_LOGGER_ERROR(_logger, err_msg);
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
    _grow(_file_size + _grow_step);
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
global_cache::pointer global_cache::load(
    const std::shared_ptr<asio::io_context> io_context,
    const std::string& file_path,
    size_t initial_size,
    const void* address,
    unsigned grow_step,
    unsigned nb_update_before_save,
    std::chrono::system_clock::duration save_interval) {
  if (!_instance) {
    _instance =
        pointer(new global_cache_data(io_context, file_path, grow_step,
                                      nb_update_before_save, save_interval));
    _instance->_open(initial_size, address);
    absl::WriterMutexLock l(
        &_instance
             ->_protect);  // mandatory only for _start_save_timer attribute
    _instance->_start_save_timer();
  }
  return _instance;
}

/**
 * @brief reset the singleton pointer
 *
 */
void global_cache::unload() {
  if (_instance) {
    _instance->stop();
    _instance.reset();
  }
}

void global_cache::stop() {
  absl::WriterMutexLock l(&_protect);
  _save_timer.cancel();
  _flush();
  _file.reset();
  _file_size = 0;
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
