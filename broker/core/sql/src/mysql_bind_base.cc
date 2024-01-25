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

#include "com/centreon/broker/sql/mysql_bind_base.hh"

#include <cfloat>
#include <cmath>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

/**
 * @brief Constructor
 *
 * @param size Number of columns in this bind
 */
mysql_bind_base::mysql_bind_base(int size) : _typed(size), _bind(size) {}

/**
 * @brief Return a boolean telling if the column at index range has been
 * prepared or not. A column is prepared when its column has a type defined.
 *
 * @param range a non negative integer.
 *
 * @return True if the column is prepared, False otherwise.
 */
bool mysql_bind_base::_prepared(size_t range) const {
  return _typed[range];
}

/**
 * @brief Accessor to the MYSQL_BIND* contained in this mysql_bind_base. This is
 * useful to call the MariaDB C connector functions.
 *
 * @return A MYSQL_BIND* pointer.
 */
MYSQL_BIND* mysql_bind_base::get_bind() {
  return &_bind[0];
}

/**
 * @brief Accessor to the number of columns in this bind.
 *
 * @return An integer.
 */

void mysql_bind_base::_set_typed(uint32_t range) {
  _empty = false;
  _typed[range] = true;
}

void mysql_bind_base::set_empty() {
  _empty = true;
}

bool mysql_bind_base::empty() const {
  return _empty;
}
