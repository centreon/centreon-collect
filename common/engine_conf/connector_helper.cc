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
#include "common/engine_conf/connector_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Connector object.
 *
 * @param obj The Connector object on which this helper works. The helper is not
 * the owner of this object.
 */
connector_helper::connector_helper(Connector* obj)
    : message_helper(object_type::connector,
                     obj,
                     {},
                     Connector::descriptor()->field_count()) {
  _init();
}

/**
 * @brief Check the validity of the Connector object.
 *
 * @param err An error counter.
 */
void connector_helper::check_validity(error_cnt& err) const {
  const Connector* o = static_cast<const Connector*>(obj());

  if (o->connector_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Connector has no name (property 'connector_name')");
  }
  if (o->connector_line().empty()) {
    err.config_errors++;
    throw msg_fmt(
        "Connector '{}' has no command line (property 'connector_line')",
        o->connector_name());
  }
}

/**
 * @brief The initializer of the Connector message.
 */
void connector_helper::_init() {
  Connector* obj = static_cast<Connector*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}
}  // namespace com::centreon::engine::configuration
