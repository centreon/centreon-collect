/*
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
#include "configuration/tag_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

/**
 * @brief Constructor from a Tag object.
 *
 * @param obj The Tag object on which this helper works. The helper is not the
 * owner of this object.
 */
tag_helper::tag_helper(Tag* obj)
    : message_helper(object_type::tag,
                     obj,
                     {
                         {"tag_id", "id"},
                         {"tag_type", "type"},
                     },
                     4) {
  _init();
}

/**
 * @brief For several keys, the parser of Tag objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool tag_helper::hook(absl::string_view key, const absl::string_view& value) {
  Tag* obj = static_cast<Tag*>(mut_obj());
  key = validate_key(key);

  if (key == "id" || key == "tag_id") {
    uint64_t id;
    if (absl::SimpleAtoi(value, &id))
      obj->mutable_key()->set_id(id);
    else
      return false;
    return true;
  } else if (key == "type" || key == "tag_type") {
    if (value == "hostcategory")
      obj->mutable_key()->set_type(tag_hostcategory);
    else if (value == "servicecategory")
      obj->mutable_key()->set_type(tag_servicecategory);
    else if (value == "hostgroup")
      obj->mutable_key()->set_type(tag_hostgroup);
    else if (value == "servicegroup")
      obj->mutable_key()->set_type(tag_servicegroup);
    else
      return false;
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Tag object.
 */
void tag_helper::check_validity() const {
  const Tag* o = static_cast<const Tag*>(obj());

  if (o->tag_name().empty())
    throw msg_fmt("Tag has no name (property 'tag_name')");
  if (o->key().id() == 0)
    throw msg_fmt("Tag '{}' has a null id", o->tag_name());
  if (o->key().type() == static_cast<uint32_t>(-1))
    throw msg_fmt("Tag type must be specified");
}

/**
 * @brief Initializer of the Tag object, in other words set its default values.
 */
void tag_helper::_init() {
  Tag* obj = static_cast<Tag*>(mut_obj());
  obj->mutable_key()->set_id(0);
  obj->mutable_key()->set_type(-1);
}
}  // namespace configuration
}  // namespace engine
}  // namespace centreon

}  // namespace com