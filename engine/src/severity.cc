/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/severity.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

severity_map severity::severities;

/**
 * @brief Constructor of a severity.
 *
 * @param id      Its id.
 * @param level   Its level (1 is more important than 2, ...).
 * @param icon_id Its icon_id.
 * @param name    Its name.
 */
severity::severity(uint64_t id,
                   uint32_t level,
                   uint64_t icon_id,
                   const std::string& name,
                   uint16_t typ)
    : _id{id},
      _level{level},
      _icon_id{icon_id},
      _name{name},
      _type{static_cast<severity_type>(typ)} {}

/**
 * @brief Accessor to the id.
 *
 * @return The id.
 */
uint64_t severity::id() const {
  return _id;
}

/**
 * @brief Accessor to the level
 *
 * @return the level.
 */
uint32_t severity::level() const {
  return _level;
}

/**
 * @brief Accessor to the icon_id
 *
 * @return the icon_id.
 */
uint64_t severity::icon_id() const {
  return _icon_id;
}

/**
 * @brief Level setter
 *
 * @param level The new level.
 */
void severity::set_level(uint32_t level) {
  _level = level;
}

/**
 * @brief Icon_id setter
 *
 * @param icon_id The new icon_id.
 */
void severity::set_icon_id(uint64_t icon_id) {
  _icon_id = icon_id;
}

/**
 * @brief Accessor to the name
 *
 * @return the name.
 */
const std::string& severity::name() const {
  return _name;
}

/**
 * @brief Setter of the name
 *
 * @param name The new name.
 */
void severity::set_name(const std::string& name) {
  _name = name;
}

/**
 * @brief Setter of the type.
 *
 * @param typ The new type
 */
void severity::set_type(const severity::severity_type typ) {
  _type = typ;
}

/**
 * @brief Getter for the type
 *
 * @return the current severity type.
 */
severity::severity_type severity::type() const {
  return _type;
}
