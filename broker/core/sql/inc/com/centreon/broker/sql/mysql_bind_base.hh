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

#ifndef CCB_MYSQL_BIND_BASE_HH
#define CCB_MYSQL_BIND_BASE_HH

#include <mysql.h>

namespace com::centreon::broker {

// Forward declarations
class mysql;

namespace database {
class mysql_bind_base {
  // A vector telling if bindings are already typed or not.
  std::vector<bool> _typed;
  bool _empty = true;

 protected:
  std::vector<MYSQL_BIND> _bind;
  std::shared_ptr<spdlog::logger> _logger;

  bool _prepared(size_t range) const;
  void _set_typed(uint32_t range);

 public:
  /**
   * @brief Default constructor
   */
  mysql_bind_base(const std::shared_ptr<spdlog::logger>& logger)
      : _logger{logger} {}
  mysql_bind_base(int size, const std::shared_ptr<spdlog::logger>& logger);
  /**
   * @brief Destructor
   */
  virtual ~mysql_bind_base() noexcept = default;
  MYSQL_BIND* get_bind();
  bool empty() const;
  void set_empty();
  void set_typed(uint32_t range);
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_BIND_BASE_HH
