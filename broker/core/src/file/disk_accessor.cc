/*
** Copyright 2022 Centreon
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
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::file;

std::shared_ptr<disk_accessor> disk_accessor::_instance;
/**
 * @brief Constructor. limit_size is the maximum allowed size for the generated
 * files.
 *
 * @param limit_size
 */
disk_accessor::disk_accessor(size_t limit_size)
    : _limit_size{limit_size}, _current_size{0u} {}

void disk_accessor::load(size_t limit_size) {
  if (!_instance)
    _instance = std::shared_ptr<disk_accessor>(new disk_accessor(limit_size));
  else
    log_v2::core()->warn("disk accessor already loaded");
}

/**
 * @brief Static function used to unload the disk accessor instance.
 */
void disk_accessor::unload() {
  _instance.reset();
}

/**
 * @brief Accessor to the disk accessor instance.
 *
 * @return The instance reference.
 */
disk_accessor& disk_accessor::instance() {
  assert(_instance);
  return *_instance;
}

/**
 * @brief Accessor to the disk accessor instance.
 *
 * @return The instance shared_ptr.
 */
std::shared_ptr<disk_accessor> disk_accessor::instance_ptr() {
  assert(_instance);
  return _instance;
}

/**
 * @brief Initialize the current occupied size. If there are already files
 * created by previous instances of cbd, we must take them into account.
 *
 * @param current_size
 */
void disk_accessor::set_current_size(size_t current_size) {
  _current_size = current_size;
}

/**
 * @brief Encapsulation of fwrite() function. A check of the limit size is
 * added. In case this limit size is reached, the return value is 0 and nothing
 * is written on disk.
 *
 * @param ptr
 * @param size
 * @param nmemb
 * @param stream
 *
 * @return The number of bytes written. Should be equal to nmemb if size = 1.
 */
size_t disk_accessor::fwrite(const void* ptr,
                             size_t size,
                             size_t nmemb,
                             fd stream) {
  if (_limit_size == 0 || _current_size + size * nmemb <= _limit_size) {
    _current_size += size * nmemb;
    return ::fwrite(ptr, size, nmemb, stream);
  } else {
    errno = ENOSPC;
    log_v2::core()->error(
        "disk_accessor: the limit size of {} bytes is reached for queue files. "
        "New events written to disk are lost",
        _limit_size);
    return 0;
  }
}

/**
 * @brief Call the libc fread function. Its interest is to gather all the needed
 * functions into one class, to plan evolutions for later. Why not an
 * asynchronous api.
 *
 * @param ptr
 * @param size
 * @param nmemb
 * @param stream
 *
 * @return
 */
size_t disk_accessor::fread(void* ptr, size_t size, size_t nmemb, fd stream) {
  return ::fread(ptr, size, nmemb, stream);
}

/**
 * @brief Call the standard remove() function. Moreover it is linked to the
 * current_size of the disk_accessor that is updated when a file is removed.
 *
 * @param name The name of the file to remove.
 */
void disk_accessor::remove(const std::string& name) {
  struct stat file_stat;
  if (stat(name.c_str(), &file_stat) == 0)
    _current_size -= file_stat.st_size;
  std::remove(name.c_str());
}

/**
 * @brief Accessor to the current used size.
 *
 * @return A size in bytes.
 */
size_t disk_accessor::current_size() const {
  return _current_size;
}

/**
 * @brief A binding to the libc fopen() function.
 *
 * @param name
 * @param mode
 *
 * @return
 */
disk_accessor::fd disk_accessor::fopen(const std::string& name,
                                       const char* mode) {
  return ::fopen(name.c_str(), mode);
}

/**
 * @brief A binding to the standard fclose() function.
 *
 * @param f
 */
void disk_accessor::fclose(disk_accessor::fd f) {
  ::fclose(f);
}
