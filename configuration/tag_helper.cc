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

namespace com::centreon::engine::configuration {
tag_helper::tag_helper(Tag* obj)
    : message_helper(object_type::tag,
                     obj,
                     {
                         {"name", "tag_name"},
                         {"tag_id", "id"},
                         {"tag_type", "type"},
                     },
                     4) {
  init_tag(static_cast<Tag*>(mut_obj()));
}

bool tag_helper::hook(const absl::string_view& key,
                      const absl::string_view& value) {
  Tag* obj = static_cast<Tag*>(mut_obj());

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
void tag_helper::check_validity() const {
  const Tag* o = static_cast<const Tag*>(obj());

  if (o->tag_name().empty())
    throw msg_fmt("Tag has no name (property 'tag_name')");
  if (o->key().id() == 0)
    throw msg_fmt("Tag '{}' has a null id", o->tag_name());
}
}  // namespace com::centreon::engine::configuration
