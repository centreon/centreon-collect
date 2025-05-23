/**
 * Copyright 2020-2021 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/persistent_cache.hh"

#include <unistd.h>

#include <cerrno>

#include "broker/core/bbdo/stream.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/file/opener.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] cache_file  Path to the cache file.
 */
persistent_cache::persistent_cache(
    const std::string& cache_file,
    const std::shared_ptr<spdlog::logger>& logger)
    : _cache_file(cache_file), _logger{logger} {
  _open();
}

/**
 *  @brief Add an event to the persistent cache.
 *
 *  The event will be serialized and stored in the cache file.
 *
 *  @param[in] d  Object to store.
 */
void persistent_cache::add(std::shared_ptr<io::data> const& d) {
  if (!_write_file)
    throw msg_fmt("core: cache file '{}' is not open for writing", _cache_file);
  _write_file->write(d);
}

/**
 *  @brief Apply changes to the persistent cache.
 *
 *  After a temporary persistent cache has been written through multiple
 *  add() calls, use this function to delete the previous cache and make
 *  the new file the persistent cache.
 */
void persistent_cache::commit() {
  // Perform changes only if a transaction was started.
  if (_write_file) {
    _write_file.reset();
    _read_file.reset();
    if (::rename(_cache_file.c_str(), _old_file().c_str())) {
      char const* msg(strerror(errno));
      throw msg_fmt("core: cache file '{}' could not be renamed to '{}' : {}",
                    _cache_file, _old_file(), msg);
    } else if (::rename(_new_file().c_str(), _cache_file.c_str())) {
      // .old file will be renamed by the _open() method.
      char const* msg(strerror(errno));
      throw msg_fmt("core: cache file '{}' could not be renamed to '{}' : {}",
                    _new_file(), _cache_file, msg);
    }
    // No error checking, this is a secondary issue.
    if (unlink(_old_file().c_str()))
      _logger->error("removing persistent cache '{}' failed", _old_file());
  }
}

/**
 *  Get the next event from the persistent cache.
 *
 *  @param[out] d  Pointer to the next event of the persistent cache. A
 *                 NULL pointer is provided on EOF.
 */
void persistent_cache::get(std::shared_ptr<io::data>& d) {
  if (!_read_file)
    _open();
  try {
    _read_file->read(d);
  } catch (exceptions::shutdown const& e) {
    (void)e;
    d.reset();
  }
}

/**
 *  @brief Start a transaction (a new persistent cache file).
 *
 *  The old cache won't be erased until commit() is called.
 */
void persistent_cache::transaction() {
  if (_write_file)
    throw msg_fmt("core: cache file '{}' is already open for writing",
                  _cache_file);
  file::opener opnr;
  opnr.set_filename(_new_file());
  opnr.set_auto_delete(false);
  opnr.set_max_size(0);
  std::shared_ptr<io::stream> fs(opnr.open());
  std::shared_ptr<bbdo::stream> bs{std::make_shared<bbdo::stream>(true)};
  bs->set_substream(fs);
  bs->set_coarse(true);
  _write_file = std::static_pointer_cast<io::stream>(bs);
}

/**
 *  Get the name of the cache file.
 *
 *  @return  The name of the cache file.
 */
const std::string& persistent_cache::get_cache_file() const {
  return _cache_file;
}

/**
 *  Get the new file name.
 *
 *  @return Cache file name appended with ".new".
 */
std::string persistent_cache::_new_file() const {
  std::string new_file(fmt::format("{}.new", _cache_file));
  return new_file;
}

/**
 *  Get the old file name.
 *
 *  @return Cache file name appended with ".old".
 */
std::string persistent_cache::_old_file() const {
  std::string old_file(fmt::format("{}.old", _cache_file));
  return old_file;
}

/**
 *  Open persistent cache file.
 */
void persistent_cache::_open() {
  // Open either cache file or old cache file.
  std::ifstream if_cache(_cache_file);
  std::ifstream if_old(_old_file());
  if (!if_cache.good()) {
    if (if_old.good())
      ::rename(_old_file().c_str(), _cache_file.c_str());
  }

  // Create file stream.
  file::opener opnr;
  opnr.set_filename(_cache_file);
  opnr.set_auto_delete(false);
  opnr.set_max_size(0);
  std::shared_ptr<io::stream> fs(opnr.open());

  // Create BBDO layer.
  std::shared_ptr<bbdo::stream> bs(new bbdo::stream(true));
  bs->set_substream(fs);
  bs->set_coarse(true);

  // We will access only the BBDO layer.
  _read_file = std::static_pointer_cast<io::stream>(bs);
}

/**
 * @brief Accessor to the logger.
 *
 * @return A shared pointer to the logger.
 */
std::shared_ptr<spdlog::logger> persistent_cache::logger() const {
  return _logger;
}
