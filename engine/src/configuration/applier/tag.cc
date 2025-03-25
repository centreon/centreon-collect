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

#include "com/centreon/engine/configuration/applier/tag.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "gtest/gtest.h"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 * @brief Add a new tag.
 *
 * @param obj The new protobuf configuration tag to add.
 */
void applier::tag::add_object(const configuration::Tag& obj) {
  // Logging.
  config_logger->debug("Creating new tag ({},{}).", obj.key().id(),
                       obj.key().type());

  // Add tag to the global configuration set.
  configuration::Tag* new_tg = pb_indexed_config.mut_state().add_tags();
  new_tg->CopyFrom(obj);

  auto tg = std::make_shared<engine::tag>(
      new_tg->key().id(),
      static_cast<engine::tag::tagtype>(new_tg->key().type()),
      new_tg->tag_name());
  if (!tg)
    throw engine_error() << fmt::format("Could not register tag ({},{})",
                                        new_tg->key().id(),
                                        new_tg->key().type());

  // Add new items to the configuration state.
  auto res = engine::tag::tags.insert(
      {{new_tg->key().id(), new_tg->key().type()}, tg});
  if (!res.second)
    config_logger->error(
        "Could not insert tag ({},{}) into cache because it already exists",
        new_tg->key().id(), new_tg->key().type());

  broker_adaptive_tag_data(NEBTYPE_TAG_ADD, tg.get());
}

/**
 * @brief Modify tag.
 *
 * @param obj The new tag protobuf configuration.
 */
void applier::tag::modify_object(configuration::Tag* to_modify,
                                 const configuration::Tag& new_object) {
  // Logging.
  config_logger->debug("Modifying tag ({},{}).", to_modify->key().id(),
                       to_modify->key().type());

  // Find tag object.
  tag_map::iterator it_obj =
      engine::tag::tags.find({new_object.key().id(), new_object.key().type()});
  if (it_obj == engine::tag::tags.end()) {
    throw engine_error() << fmt::format(
        "Could not modify non-existing tag object ({},{})",
        new_object.key().id(), new_object.key().type());
  }

  engine::tag* t = it_obj->second.get();

  // Update the global configuration set.
  if (to_modify->tag_name() != new_object.tag_name()) {
    to_modify->set_tag_name(new_object.tag_name());
    t->set_name(new_object.tag_name());

    // Notify event broker.
    broker_adaptive_tag_data(NEBTYPE_TAG_UPDATE, t);
  } else
    config_logger->debug("Tag ({},{}) did not change", new_object.key().id(),
                         new_object.key().type());
}

/**
 * @brief Remove old tag.
 *
 * @param idx The idx in the tags configuration objects to remove.
 */
template <>
void applier::tag::remove_object(
    const std::pair<ssize_t, std::pair<uint64_t, uint32_t>>& p) {
  const configuration::Tag& obj = pb_indexed_config.state().tags().at(p.first);

  // Logging.
  config_logger->debug("Removing tag ({},{}).", obj.key().id(),
                       obj.key().type());

  // Find tag.
  tag_map::iterator it =
      engine::tag::tags.find({obj.key().id(), obj.key().type()});
  if (it != engine::tag::tags.end()) {
    engine::tag* tg = it->second.get();

    // Notify event broker.
    broker_adaptive_tag_data(NEBTYPE_TAG_DELETE, tg);

    // Erase tag object (this will effectively delete the object).
    engine::tag::tags.erase(it);
  }

  // Remove tag from the global configuration set.
  pb_indexed_config.mut_state().mutable_tags()->DeleteSubrange(p.first, 1);
}

/**
 *  Resolve a tag.
 *
 *  @param[in] obj  Object to resolve.
 */
void applier::tag::resolve_object(const configuration::Tag& obj,
                                  error_cnt& err [[maybe_unused]]) {
  tag_map::const_iterator tg_it{
      engine::tag::tags.find({obj.key().id(), obj.key().type()})};
  if (tg_it == engine::tag::tags.end() || !tg_it->second) {
    throw engine_error() << "Cannot resolve non-existing tag ("
                         << obj.key().id() << "," << obj.key().type() << ")";
  }
}
