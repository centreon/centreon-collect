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
#include "configuration/servicegroup_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Servicegroup object.
 *
 * @param obj The Servicegroup object on which this helper works. The helper is
 * not the owner of this object.
 */
servicegroup_helper::servicegroup_helper(Servicegroup* obj)
    : message_helper(object_type::servicegroup, obj, {}, 10) {
  _init();
}

/**
 * @brief For several keys, the parser of Servicegroup objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool servicegroup_helper::hook(const absl::string_view& key,
                               const absl::string_view& value) {
  Servicegroup* obj = static_cast<Servicegroup*>(mut_obj());
  if (key == "members") {
    fill_pair_string_group(obj->mutable_members(), value);
    return true;
  } else if (key == "servicegroup_members") {
    fill_string_group(obj->mutable_servicegroup_members(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Servicegroup object.
 */
void servicegroup_helper::check_validity() const {
  const Servicegroup* o = static_cast<const Servicegroup*>(obj());

  if (o->servicegroup_name().empty())
    throw msg_fmt("Service group has no name (property 'servicegroup_name')");
}
void servicegroup_helper::_init() {}

}  // namespace com::centreon::engine::configuration