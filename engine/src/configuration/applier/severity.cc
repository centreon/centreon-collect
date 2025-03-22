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

#include "com/centreon/engine/configuration/applier/severity.hh"
#include <google/protobuf/util/message_differencer.h>
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/severity.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;

/**
 * @brief Add new severity.
 *
 * @param obj The new severity to add into the monitoring engine.
 */
void applier::severity::add_object(const configuration::Severity& obj) {
  // Logging.
  config_logger->debug("Creating new severity ({}, {}).", obj.key().id(),
                       obj.key().type());

  // Add severity to the global configuration set.
  auto* new_sv = pb_indexed_config.state().add_severities();
  new_sv->CopyFrom(obj);

  auto sv{std::make_shared<engine::severity>(obj.key().id(), obj.level(),
                                             obj.icon_id(), obj.severity_name(),
                                             obj.key().type())};
  if (!sv)
    throw engine_error() << fmt::format("Could not register severity ({},{})",
                                        obj.key().id(), obj.key().type());

  // Add new items to the configuration state.
  engine::severity::severities.insert({{obj.key().id(), obj.key().type()}, sv});

  broker_adaptive_severity_data(NEBTYPE_SEVERITY_ADD, sv.get());
}

/**
 * @brief Modify severity.
 *
 * @param to_modify A pointer to the current configuration of a severity.
 * @param new_object The new configuration to apply.
 */
void applier::severity::modify_object(
    configuration::Severity* to_modify,
    const configuration::Severity& new_object) {
  // Logging.
  config_logger->debug("Modifying severity ({}, {}).", new_object.key().id(),
                       new_object.key().type());

  // Find severity object.
  severity_map::iterator it_obj = engine::severity::severities.find(
      {new_object.key().id(), new_object.key().type()});
  if (it_obj == engine::severity::severities.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing severity object ({}, {})",
        new_object.key().id(), new_object.key().type());
  engine::severity* s = it_obj->second.get();

  // Update the global configuration set.
  if (!MessageDifferencer::Equals(*to_modify, new_object)) {
    if (to_modify->severity_name() != new_object.severity_name()) {
      s->set_name(new_object.severity_name());
      to_modify->set_severity_name(new_object.severity_name());
    }
    if (to_modify->level() != new_object.level()) {
      s->set_level(new_object.level());
      to_modify->set_level(new_object.level());
    }
    if (to_modify->icon_id() != new_object.icon_id()) {
      s->set_icon_id(new_object.icon_id());
      to_modify->set_icon_id(new_object.icon_id());
    }

    // Notify event broker.
    broker_adaptive_severity_data(NEBTYPE_SEVERITY_UPDATE, s);
  } else
    config_logger->debug("Severity ({}, {}) did not change",
                         new_object.key().id(), new_object.key().type());
}

/**
 * @brief Remove old severity at index idx.
 *
 * @param idx The index of the object to remove.
 */
void applier::severity::remove_object(ssize_t idx) {
  const configuration::Severity& obj =
      pb_indexed_config.state().severities()[idx];

  // Logging.

  config_logger->debug("Removing severity ({}, {}).", obj.key().id(),
                       obj.key().type());

  // Find severity.
  severity_map::iterator it =
      engine::severity::severities.find({obj.key().id(), obj.key().type()});

  if (it != engine::severity::severities.end()) {
    engine::severity* sv = it->second.get();

    // Notify event broker.
    broker_adaptive_severity_data(NEBTYPE_SEVERITY_DELETE, sv);

    // Erase severity object (this will effectively delete the object).
    engine::severity::severities.erase(it);
  }

  // Remove severity from the global configuration set.
  pb_indexed_config.state().mutable_severities()->DeleteSubrange(idx, 1);
}
