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

#ifndef CCB_FILE_DISK_ACCESSOR_HH
#define CCB_FILE_DISK_ACCESSOR_HH
#include <atomic>
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace file {
/**
 * @class disk_accessor disk_accessor.hh
 * "com/centreon/broker/file/disk_accessor.hh"
 * @brief Encapsulate fonctions to access disk and also manage the size occupied
 * by these files. Raise errors if the maximum size is reached.
 */
class disk_accessor {
  const size_t _limit_size;
  std::atomic<size_t> _current_size;

  disk_accessor(size_t limit_size);
  static disk_accessor* _instance;

 public:
  using fd = FILE*;
  static void load(size_t limit_size);
  static void unload();
  static disk_accessor& instance();

  void set_current_size(size_t current_size);
  size_t current_size() const;
  size_t fwrite(const void* ptr, size_t size, size_t nmemb, fd stream);
  size_t fread(void* ptr, size_t size, size_t nmemb, fd stream);
  void remove(const std::string& name);
  fd fopen(const std::string& name, const char* mode);
  void fclose(fd f);
};
}  // namespace file

CCB_END()

#endif  // !CCB_FILE_DISK_ACCESSOR_HH
