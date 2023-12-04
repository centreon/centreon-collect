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
#include "com/centreon/engine/tag.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

tag_map tag::tags;

/**
 * @brief Constructor of a tag.
 *
 * @param id      Its id.
 * @param typ     tag::hostcategory, tag::servicecategory, tag::hostgroup or
 *                tag::servicegroup.
 * @param name    Its name.
 */
tag::tag(uint64_t id, tag::tagtype type, const std::string& name)
    : _id{id}, _type{type}, _name{name} {
}

/**
 * @brief Accessor to the id.
 *
 * @return The id.
 */
uint64_t tag::id() const {
  return _id;
}

/**
 * @brief Accessor to the name
 *
 * @return the name.
 */
const std::string& tag::name() const {
  return _name;
}

/**
 * @brief Setter of the name
 *
 * @param name The new name.
 */
void tag::set_name(const std::string& name) {
  _name = name;
}

/**
 * @brief Accessor to the type
 *
 * @return the type.
 */
tag::tagtype tag::type() const {
  return _type;
}

/**
 * @brief Setter of the type
 *
 * @param type The new type.
 */
void tag::set_type(const tag::tagtype typ) {
  _type = typ;
}
