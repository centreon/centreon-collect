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

#include "com/centreon/broker/cache/protobuf.hh"

#include "boost/system/detail/error_code.hpp"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::cache;

global_cache::lock::~lock() {
  switch (_state) {
    case e_state::shared:
      _mut->unlock_shared();
      break;
    case e_state::upgrade:
      _mut->unlock_upgrade();
      break;
    case e_state::unique:
      _mut->unlock();
      break;
    default:
      break;
  }
}

void global_cache::lock::unique(boost::upgrade_mutex* mut) {
  if (_state == e_state::unique && mut == _mut) {
    return;
  }
  if (_state != e_state::no) {
    throw std::runtime_error(
        fmt::format("can't lock state={}", static_cast<unsigned>(_state)));
  }
  _mut = mut;
  _mut->lock();
  _state = e_state::unique;
}

void global_cache::lock::upgrade_lock(boost::upgrade_mutex* mut) {
  if (_state == e_state::upgrade && mut == _mut) {
    return;
  }
  if (_state != e_state::no) {
    throw std::runtime_error(fmt::format("can't upgrade lock state={}",
                                         static_cast<unsigned>(_state)));
  }
  _mut = mut;
  _mut->lock_upgrade();
  _state = e_state::upgrade;
}

void global_cache::lock::shared_lock(boost::upgrade_mutex* mut) {
  if (_state == e_state::shared && mut == _mut) {
    return;
  }
  if (_state != e_state::no) {
    throw std::runtime_error(fmt::format("can't shared lock state={}",
                                         static_cast<unsigned>(_state)));
  }
  _mut = mut;
  _mut->lock_shared();
  _state = e_state::shared;
}

void global_cache::lock::upgrade_to_unique() {
  if (_state != e_state::upgrade) {
    throw std::runtime_error(
        fmt::format("can't promote lock to unique state={}",
                    static_cast<unsigned>(_state)));
  }
  _mut->unlock_upgrade_and_lock();
  _state = e_state::unique;
}

void global_cache::lock::unique_to_shared() {
  if (_state != e_state::unique) {
    throw std::runtime_error(
        fmt::format("can't demote lock from unique to shared, current state={}",
                    static_cast<unsigned>(_state)));
  }
  _mut->unlock_and_lock_shared();
  _state = e_state::shared;
}

inline std::string operator+(const std::string& left,
                             const std::string_view& to_append) {
  std::string ret(left);
  ret.append(to_append.data(), to_append.length());
  return ret;
}

namespace com::centreon::broker::cache {
struct collect_version {
  unsigned major;
  unsigned minor;
  unsigned patch;

  bool operator!=(const collect_version& other) const {
    return major != other.major || minor != other.minor || patch != other.patch;
  }
};
}  // namespace com::centreon::broker::cache

std::shared_ptr<com::centreon::broker::cache::global_cache>
    com::centreon::broker::cache::global_cache::_instance;

absl::Mutex com::centreon::broker::cache::global_cache::_instance_m;

/**
 * @brief Construct a global_cache base object.
 *
 * Does not open or create the underlying file; call _open() after construction.
 *
 * @param io_context  ASIO context used by the periodic save timer.
 * @param file_path   Base path without extension; ".rt" or ".cnf" is appended
 *                    depending on cache_type.
 * @param logger      spdlog logger.
 * @param cache_type  conf (stable) or real_time (includes status updates).
 * @param conf_cache  Pointer to the conf cache used as a fallback when the RT
 *                    cache does not hold a requested object.  nullptr for the
 *                    conf cache itself.
 * @param grow_step            Bytes added to the file on each grow (default 512
 * MB).
 * @param nb_update_before_save Flush the dirty flag after this many writes.
 * @param save_interval        Periodic flush interval (default 10 s).
 */
global_cache::global_cache(const std::shared_ptr<asio::io_context> io_context,
                           const std::string& file_path,
                           const std::shared_ptr<spdlog::logger>& logger,
                           e_cache_type cache_type,
                           const std::shared_ptr<global_cache>& conf_cache,
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
      _file_path(file_path +
                 (cache_type == e_cache_type::real_time ? ".rt" : ".cnf")),
      _conf_cache(conf_cache),
      _cache_type(cache_type),
      _logger{logger} {
  SPDLOG_LOGGER_DEBUG(_logger, "cache create global_cache {:p} {}",
                      static_cast<const void*>(this), _file_path);
}

/**
 * @brief Flush the file and release the memory mapping.
 */
global_cache::~global_cache() {
  SPDLOG_LOGGER_DEBUG(_logger, "cache destroy global_cache {:p} {}",
                      static_cast<const void*>(this), _file_path);
  stop();
}

/**
 * @brief static method use to get access to global_cache singleton
 *
 * @return com::centreon::broker::cache::global_cache::pointer
 */
com::centreon::broker::cache::global_cache::pointer
com::centreon::broker::cache::global_cache::instance_ptr() {
  absl::MutexLock l(&_instance_m);
  return _instance;
}

/**
 * @brief Schedule the next periodic flush.
 *
 * Arms the ASIO timer for _save_interval from now.  Must be called while
 * holding _protect (the timer is an ABSL_GUARDED_BY member).
 */
void global_cache::_start_save_timer() {
  _save_timer.expires_after(_save_interval);
  _save_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_save_timer_handler(err);
      });
}

/**
 * @brief ASIO callback fired when the periodic save timer expires.
 *
 * Flushes the mapped file to disk and re-arms the timer.  If the timer was
 * cancelled (err is set), the handler returns immediately without flushing.
 *
 * @param err  Error code set by ASIO when the timer is cancelled.
 */
void global_cache::_save_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  } else {
    boost::unique_lock l(_protect);
    _flush();
    _start_save_timer();
  }
}

/**
 * @brief Mark the file dirty and trigger a flush when the write threshold
 * is reached.
 *
 * Sets the in-segment dirty flag so that an unclean shutdown can be detected
 * on the next open.  After _nb_update_before_save writes within the same
 * wall-clock second, the file is flushed immediately and the periodic timer
 * is reset.  Must be called while holding _protect exclusively.
 */
void global_cache::_set_dirty_and_increment_modif() {
  if (_dirty) {
    *_dirty = true;
  }
  ++_modif_counter;
  time_t now = time(nullptr);
  if (_modif_counter > _nb_update_before_save && _last_save_time != now) {
    _modif_counter = 0;
    _last_save_time = now;
    _flush();
    // reset save timer in order to save after _save_interval
    _start_save_timer();
  }
}

/**
 * @brief Flush the memory-mapped file to disk and clear the dirty flag.
 *
 * The dirty flag is cleared between two flushes so that, if the process
 * crashes after the first flush but before the second, the flag is found
 * false on the next open and the file is considered clean.
 * No-op if the file is not open or is already clean.
 * Must be called while holding _protect exclusively.
 */
void global_cache::_flush() {
  if (_file && _dirty && *_dirty) {
    SPDLOG_LOGGER_TRACE(_logger, "cache: begin flush of {}", _file_path);
    _file->flush();
    *_dirty = false;
    _file->flush();
    SPDLOG_LOGGER_TRACE(_logger, "cache: end flush of {}", _file_path);
  }
}

/**
 * @brief Open an existing cache file or create a new one.
 *
 * Tries to open the file at _file_path in read-write mode.  A file is
 * considered valid only if:
 *  - it exists, is a regular file and its size is a multiple of 8 bytes,
 *  - its dirty flag is false (clean shutdown).
 *
 * On success, managed_map(false) is called so that the derived class can
 * locate its named containers inside the segment.
 *
 * In all error cases (corrupted file, wrong version, dirty flag set) the file
 * is deleted and recreated from scratch at initial_size_on_create bytes.
 * managed_map(true) is then called so that the derived class constructs its
 * containers in the new segment.
 *
 * @param initial_size_on_create  Size of the file when created from scratch.
 * @param address                 Preferred virtual address for the mapping
 *                                (0 lets the OS choose).  Used in tests to
 *                                force a remap to a different address.
 */
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
          if (dirty) {
            SPDLOG_LOGGER_ERROR(_logger,
                                "global_cache dirty flag not reset for {} => "
                                "erase file and recreate",
                                _file_path);
          } else {
            SPDLOG_LOGGER_ERROR(_logger,
                                "global_cache dirty flag not found for {} => "
                                "erase file and recreate",
                                _file_path);
          }
          _file.reset();
          _file_size = 0;
        }
      }
    } catch (const boost::exception& e) {
      std::string err_detail =
          fmt::format("cache: corrupted cache file {} => recreate {}",
                      _file_path, boost::diagnostic_information(e));

      SPDLOG_LOGGER_ERROR(_logger, err_detail);
      if (_cache_type == e_cache_type::conf) {
        SPDLOG_LOGGER_ERROR(_logger, "cache: you have to restart pollers");
      }
      _file.reset();
      _file_size = 0;
    } catch (const std::invalid_argument&
                 e) {  // upgrade broker => erase cache and recreate
      SPDLOG_LOGGER_ERROR(_logger, e.what());
      if (_cache_type == e_cache_type::conf) {
        SPDLOG_LOGGER_ERROR(_logger, "cache: you have to restart pollers");
      }
      _file.reset();
      _file_size = 0;
    } catch (const std::exception& e) {
      std::string err_detail =
          fmt::format("cache: corrupted cache file {} => recreate {}",
                      _file_path, e.what());

      SPDLOG_LOGGER_ERROR(_logger, err_detail);
      if (_cache_type == e_cache_type::conf) {
        SPDLOG_LOGGER_ERROR(_logger, "cache: you have to restart pollers");
      }
      _file.reset();
      _file_size = 0;
    }

    if (_cache_type == e_cache_type::conf) {
      SPDLOG_LOGGER_INFO(_logger,
                         "cache: create file {}, you have to restart pollers",
                         _file_path);
    } else {
      SPDLOG_LOGGER_INFO(_logger, "cache: create file {}", _file_path);
    }
    ::remove(_file_path.c_str());
    _grow(initial_size_on_create);
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
 * @brief Initialize or validate the version section in the memory-mapped file.
 *
 * This method is called by the derived class after the memory-mapped file has
 * been opened or created. It acts as a hook so that each level of the class
 * hierarchy can initialize its own named objects in the segment.
 *
 * - create=true  : constructs the named object "collect_version" in the
 *                  segment with the current binary version.
 * - create=false : looks up "collect_version" in the existing file and checks
 *                  that it matches the current binary version. If absent or
 *                  mismatched, throws std::invalid_argument, which causes
 *                  _open() to delete and recreate the file.
 *
 * @param create true if the file was just created, false if reopened.
 * @throws std::invalid_argument if the version object is missing or does not
 *         match the current binary version.
 */
void global_cache::managed_map(bool create) {
  collect_version expected{COLLECT_MAJOR, COLLECT_MINOR, COLLECT_PATCH};
  if (create) {
    _file->find_or_construct<collect_version>("collect_version")(expected);
    _dirty = _file->find_or_construct<bool>("dirty")(false);
    return;
  } else {
    collect_version* version =
        _file->find<collect_version>("collect_version").first;
    if (!version) {
      throw std::invalid_argument("version not found");
    }
    if (*version != expected) {
      std::string detail = fmt::format(
          "cache: collect version in cache: {}.{}.{}, expected: {}.{}.{} => "
          "erase cache {}",
          version->major, version->minor, version->patch, expected.major,
          expected.minor, expected.patch, _file_path);
      throw std::invalid_argument(detail);
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
    boost::unique_lock l(_protect);
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
  absl::MutexLock instance_lock(&_instance_m);
  if (!_instance) {
    std::shared_ptr<global_cache> conf_cache(new global_cache_data(
        io_context, file_path, e_cache_type::conf, nullptr, grow_step,
        nb_update_before_save, save_interval));
    conf_cache->_open(initial_size, address);
    {
      boost::unique_lock l(
          conf_cache
              ->_protect);  // mandatory only for _start_save_timer attribute
      conf_cache->_start_save_timer();
    }
    _instance = pointer(new global_cache_data(
        io_context, file_path, e_cache_type::real_time, conf_cache, grow_step,
        nb_update_before_save, save_interval));
    _instance->_open(initial_size, address);
    boost::unique_lock l(
        _instance->_protect);  // mandatory only for _start_save_timer attribute
    _instance->_start_save_timer();
  }
  return _instance;
}

/**
 * @brief reset the singleton pointer
 *
 */
void global_cache::unload() {
  absl::MutexLock instance_lock(&_instance_m);
  if (_instance) {
    _instance->stop();
    _instance.reset();
  }
}

/**
 * @brief Flush, cancel the save timer, and close the mapped file.
 *
 * For the real-time cache, also stops the conf cache recursively.
 * Safe to call multiple times.
 */
void global_cache::stop() {
  {
    boost::unique_lock l(_protect);
    _save_timer.cancel();
    if (_file) {
      SPDLOG_LOGGER_INFO(_logger, "cache: stop {}", _file_path);
      _flush();
      _file.reset();
      _file_size = 0;
    }
  }
  if (_cache_type == e_cache_type::real_time && _conf_cache) {
    _conf_cache->stop();
  }
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
