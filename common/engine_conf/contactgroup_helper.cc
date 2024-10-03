/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/contactgroup_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Contactgroup object.
 *
 * @param obj The Contactgroup object on which this helper works. The helper is
 * not the owner of this object.
 */
contactgroup_helper::contactgroup_helper(Contactgroup* obj)
    : message_helper(object_type::contactgroup,
                     obj,
                     {},
                     Contactgroup::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Contactgroup objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool contactgroup_helper::hook(std::string_view key,
                               const std::string_view& value) {
  Contactgroup* obj = static_cast<Contactgroup*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "contactgroup_members") {
    fill_string_group(obj->mutable_contactgroup_members(), value);
    return true;
  } else if (key == "members") {
    fill_string_group(obj->mutable_members(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Contactgroup object.
 *
 * @param err An error counter.
 */
void contactgroup_helper::check_validity(error_cnt& err) const {
  const Contactgroup* o = static_cast<const Contactgroup*>(obj());

  if (o->contactgroup_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Contactgroup has no name (property 'contactgroup_name')");
  }
}

/**
 * @brief The initializer of the Contactgroup message.
 */
void contactgroup_helper::_init() {
  Contactgroup* obj = static_cast<Contactgroup*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}
}  // namespace com::centreon::engine::configuration
