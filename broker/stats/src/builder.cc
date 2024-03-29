/**
 * Copyright 2011 - 2021 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/broker/stats/builder.hh"

#include <time.h>
#include <unistd.h>

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/endpoint.hh"
#include "com/centreon/broker/misc/filesystem.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/sql/mysql_manager.hh"
#include "com/centreon/broker/stats/helper.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::stats;

/**
 *  Default constructor.
 */
builder::builder() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The object to copy.
 */
builder::builder(builder const& right) {
  operator=(right);
}

/**
 *  Destructor.
 */
builder::~builder() noexcept {}

/**
 *  Copy operator.
 *
 *  @param[in] right The object to copy.
 *
 *  @return This object.
 */
builder& builder::operator=(builder const& right) {
  if (this != &right) {
    _data = right._data;
    _root = right._root;
  }
  return (*this);
}

/**
 *  Get and build statistics.
 *
 *  @param[in,out] srz  The serializer to use to serialize data.
 */
void builder::build() {
  // Cleanup.
  _data.clear();
  nlohmann::json object;
  stats::get_generic_stats(object);

  nlohmann::json mysql_object;
  stats::get_mysql_stats(mysql_object);
  object["mysql manager"] = mysql_object;

  std::vector<nlohmann::json> modules_objects;
  stats::get_loaded_module_stats(modules_objects);
  for (auto& obj : modules_objects) {
    std::string key{fmt::format("module{}", obj["name"].get<std::string>())};
    object[key] = std::move(obj);
  }

  std::vector<nlohmann::json> endpoint_objects;
  stats::get_endpoint_stats(endpoint_objects);
  for (auto& obj : endpoint_objects) {
    std::string key{fmt::format("endpoint {}", obj["name"].get<std::string>())};
    object[key] = std::move(obj);
  }

  _root = std::move(object);
  std::string buffer{_root.dump()};
  _data.insert(0, buffer);
}

/**
 *  Get data buffer.
 *
 *  @return The statistics buffer.
 */
std::string const& builder::data() const noexcept {
  return _data;
}

/**
 *  Get the properties tree.
 *
 *  @return The statistics tree.
 */
const nlohmann::json& builder::root() const noexcept {
  return _root;
}
