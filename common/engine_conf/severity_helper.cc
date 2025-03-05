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
#include "common/engine_conf/severity_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Severity object.
 *
 * @param obj The Severity object on which this helper works. The helper is not
 * the owner of this object.
 */
severity_helper::severity_helper(Severity* obj)
    : message_helper(object_type::severity,
                     obj,
                     {
                         {"severity_id", "id"},
                         {"severity_level", "level"},
                         {"severity_icon_id", "icon_id"},
                         {"severity_type", "type"},
                     },
                     Severity::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Severity objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool severity_helper::hook(std::string_view key, std::string_view value) {
  Severity* obj = static_cast<Severity*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key == "id" || key == "severity_id") {
    uint64_t id;
    if (absl::SimpleAtoi(value, &id))
      obj->mutable_key()->set_id(id);
    else
      return false;
    return true;
  } else if (key == "type" || key == "severity_type") {
    if (value == "host")
      obj->mutable_key()->set_type(SeverityType::host);
    else if (value == "service")
      obj->mutable_key()->set_type(SeverityType::service);
    else
      return false;
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Severity object.
 *
 * @param err An error counter.
 */
void severity_helper::check_validity(error_cnt& err) const {
  const Severity* o = static_cast<const Severity*>(obj());

  if (o->severity_name().empty())
    throw msg_fmt("Severity has no name (property 'severity_name')");
  if (o->key().id() == 0) {
    err.config_errors++;
    throw msg_fmt(
        "Severity id must not be less than 1 (property 'severity_id')");
  }
  if (o->level() == 0) {
    err.config_errors++;
    throw msg_fmt("Severity level must not be less than 1 (property 'level')");
  }
  if (o->key().type() == SeverityType::none) {
    err.config_errors++;
    throw msg_fmt("Severity type must be one of 'service' or 'host'");
  }
}

/**
 * @brief Initializer of the Severity object, in other words set its default
 * values.
 */
void severity_helper::_init() {
  Severity* obj = static_cast<Severity*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->mutable_key()->set_id(0);
  obj->mutable_key()->set_type(SeverityType::none);
}
}  // namespace com::centreon::engine::configuration
