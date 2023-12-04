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

#include "com/centreon/engine/configuration/applier/severity.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/severity.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/severity.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  Add new severity.
 *
 *  @param[in] obj  The new severity to add into the monitoring engine.
 */
void applier::severity::add_object(const configuration::severity& obj) {
  // Logging.
  log_v2::config()->debug("Creating new severity ({}, {}).", obj.key().first,
                          obj.key().second);

  // Add severity to the global configuration set.
  config->mut_severities().insert(obj);

  auto sv{std::make_shared<engine::severity>(obj.key().first, obj.level(),
                                             obj.icon_id(), obj.severity_name(),
                                             obj.key().second)};
  if (!sv)
    throw engine_error() << "Could not register severity (" << obj.key().first
                         << ", " << obj.key().second << ")";

  // Add new items to the configuration state.
  engine::severity::severities.insert({obj.key(), sv});

  broker_adaptive_severity_data(NEBTYPE_SEVERITY_ADD, sv.get());
}

/**
 *  @brief Expand a contact.
 *
 *  During expansion, the contact will be added to its contact groups.
 *  These will be modified in the state.
 *
 *  @param[in,out] s  Configuration state.
 */
void applier::severity::expand_objects(configuration::state&) {}

/**
 *  Modify severity.
 *
 *  @param[in] obj  The severity to modify into the monitoring engine.
 */
void applier::severity::modify_object(const configuration::severity& obj) {
  // Logging.
  log_v2::config()->debug("Modifying severity ({}, {}).", obj.key().first,
                          obj.key().second);

  // Find old configuration.
  auto it_cfg = config->severities_find(obj.key());
  if (it_cfg == config->severities().end())
    throw engine_error() << "Cannot modify non-existing severity ("
                         << obj.key().first << ", " << obj.key().second << ")";

  // Find severity object.
  severity_map::iterator it_obj{engine::severity::severities.find(obj.key())};
  if (it_obj == engine::severity::severities.end())
    throw engine_error() << "Could not modify non-existing severity object "
                         << fmt::format("({}, {})", obj.key().first,
                                        obj.key().second);
  engine::severity* s = it_obj->second.get();

  // Update the global configuration set.
  configuration::severity old_cfg(*it_cfg);
  if (old_cfg != obj) {
    config->mut_severities().erase(it_cfg);
    config->mut_severities().insert(obj);

    s->set_name(obj.severity_name());
    s->set_level(obj.level());
    s->set_icon_id(obj.icon_id());

    // Notify event broker.
    broker_adaptive_severity_data(NEBTYPE_SEVERITY_UPDATE, s);
  } else
    log_v2::config()->debug("Severity ({}, {}) did not change", obj.key().first,
                            obj.key().second);
}

/**
 *  Remove old severity.
 *
 *  @param[in] obj  The severity to remove from the monitoring engine.
 */
void applier::severity::remove_object(const configuration::severity& obj) {
  // Logging.
  log_v2::config()->debug("Removing severity ({}, {}).", obj.key().first,
                          obj.key().second);

  // Find severity.
  severity_map::iterator it = engine::severity::severities.find(obj.key());
  if (it != engine::severity::severities.end()) {
    engine::severity* sv(it->second.get());

    // Notify event broker.
    broker_adaptive_severity_data(NEBTYPE_SEVERITY_DELETE, sv);

    // Erase severity object (this will effectively delete the object).
    engine::severity::severities.erase(it);
  }

  // Remove severity from the global configuration set.
  config->mut_severities().erase(obj);
}

/**
 *  Resolve a severity.
 *
 *  @param[in] obj  Object to resolve.
 */
void applier::severity::resolve_object(const configuration::severity& obj) {
  severity_map::const_iterator sv_it{
      engine::severity::severities.find(obj.key())};
  if (sv_it == engine::severity::severities.end() || !sv_it->second)
    throw engine_error() << "Cannot resolve non-existing severity "
                         << fmt::format("({}, {})", obj.key().first,
                                        obj.key().second);
}
