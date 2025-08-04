/**
 * Copyright 2014-2023 Centreon
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

#ifndef CCB_PERSISTENT_CACHE_HH
#define CCB_PERSISTENT_CACHE_HH

#include "com/centreon/broker/io/stream.hh"

namespace com::centreon::broker {

/**
 *  @class persistent_cache persistent_cache.hh
 * "com/centreon/broker/persistent_cache.hh"
 *  @brief Provide a generic persistent cache mechanism.
 *
 *  Provide a generic persistent cache used by some modules to store
 *  data on disk between restarts.
 */
class persistent_cache {
  const std::string _cache_file;
  std::shared_ptr<io::stream> _read_file;
  std::shared_ptr<io::stream> _write_file;

 protected:
  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

 private:
  std::string _new_file() const;
  std::string _old_file() const;
  void _open();

 public:
  persistent_cache(const std::string& cache_file,
                   const std::shared_ptr<spdlog::logger>& logger);
  /**
   *  Destructor.
   */
  ~persistent_cache() noexcept = default;
  persistent_cache(const persistent_cache&) = delete;
  persistent_cache& operator=(const persistent_cache&) = delete;
  void add(std::shared_ptr<io::data> const& d);
  void commit();
  void get(std::shared_ptr<io::data>& d);
  void transaction();
  std::shared_ptr<spdlog::logger> logger() const;

  const std::string& get_cache_file() const;
};

}  // namespace com::centreon::broker
#endif  // !CCB_PERSISTENT_CACHE_HH
