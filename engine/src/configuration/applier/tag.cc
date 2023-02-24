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

#include "com/centreon/engine/configuration/applier/tag.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/tag.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/tag.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  Add new tag.
 *
 *  @param[in] obj  The new tag to add into the monitoring engine.
 */
void applier::tag::add_object(const configuration::tag& obj) {
  // Logging.
  log_v2::config()->debug("Creating new tag ({},{}).", obj.key().first,
                          obj.key().second);

  // Add tag to the global configuration set.
  config->mut_tags().insert(obj);

  auto tg{std::make_shared<engine::tag>(
      obj.key().first, static_cast<engine::tag::tagtype>(obj.key().second),
      obj.tag_name())};
  if (!tg)
    throw engine_error() << "Could not register tag (" << obj.key().first << ","
                         << obj.key().second << ")";

  // Add new items to the configuration state.
  auto res = engine::tag::tags.insert({obj.key(), tg});
  if (!res.second)
    log_v2::config()->error(
        "Could not insert tag ({},{}) into cache because it already exists",
        obj.key().first, obj.key().second);

  timeval tv(get_broker_timestamp(nullptr));
  broker_adaptive_tag_data(NEBTYPE_TAG_ADD, NEBFLAG_NONE, NEBATTR_NONE,
                           tg.get(), &tv);
}

/**
 *  @brief Expand a contact.
 *
 *  During expansion, the contact will be added to its contact groups.
 *  These will be modified in the state.
 *
 *  @param[in,out] s  Configuration state.
 */
void applier::tag::expand_objects(configuration::state&) {}

/**
 *  Modify tag.
 *
 *  @param[in] obj  The tag to modify into the monitoring engine.
 */
void applier::tag::modify_object(const configuration::tag& obj) {
  // Logging.
  log_v2::config()->debug("Modifying tag ({},{}).", obj.key().first,
                          obj.key().second);

  // Find old configuration.
  auto it_cfg = config->tags_find(obj.key());
  if (it_cfg == config->tags().end())
    throw engine_error() << "Cannot modify non-existing tag "
                         << obj.key().first;

  // Find tag object.
  tag_map::iterator it_obj{engine::tag::tags.find(obj.key())};
  if (it_obj == engine::tag::tags.end()) {
    throw engine_error() << "Could not modify non-existing tag object ("
                         << obj.key().first << "," << obj.key().second << ")";
  }

  engine::tag* t = it_obj->second.get();

  // Update the global configuration set.
  configuration::tag old_cfg(*it_cfg);
  if (old_cfg != obj) {
    config->mut_tags().erase(it_cfg);
    config->mut_tags().insert(obj);

    t->set_name(obj.tag_name());

    // Notify event broker.
    timeval tv(get_broker_timestamp(nullptr));
    broker_adaptive_tag_data(NEBTYPE_TAG_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, t,
                             &tv);
  } else
    log_v2::config()->debug("Tag ({},{}) did not change", obj.key().first,
                            obj.key().second);
}

/**
 *  Remove old tag.
 *
 *  @param[in] obj  The tag to remove from the monitoring engine.
 */
void applier::tag::remove_object(const configuration::tag& obj) {
  // Logging.
  log_v2::config()->debug("Removing tag ({},{}).", obj.key().first,
                          obj.key().second);

  // Find tag.
  tag_map::iterator it = engine::tag::tags.find(obj.key());
  if (it != engine::tag::tags.end()) {
    engine::tag* tg(it->second.get());

    // Notify event broker.
    timeval tv(get_broker_timestamp(nullptr));
    broker_adaptive_tag_data(NEBTYPE_TAG_DELETE, NEBFLAG_NONE, NEBATTR_NONE, tg,
                             &tv);

    // Erase tag object (this will effectively delete the object).
    engine::tag::tags.erase(it);
  }

  // Remove tag from the global configuration set.
  config->mut_tags().erase(obj);
}

/**
 *  Resolve a tag.
 *
 *  @param[in] obj  Object to resolve.
 */
void applier::tag::resolve_object(const configuration::tag& obj) {
  tag_map::const_iterator tg_it{engine::tag::tags.find(obj.key())};
  if (tg_it == engine::tag::tags.end() || !tg_it->second) {
    throw engine_error() << "Cannot resolve non-existing tag ("
                         << obj.key().first << "," << obj.key().second << ")";
  }
}
