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
#include "common/engine_conf/tag_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/state.pb.h"

using com::centreon::exceptions::msg_fmt;
using google::protobuf::util::MessageDifferencer;

namespace com::centreon::engine::configuration {

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
                     Tag::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Tag objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool tag_helper::hook(std::string_view key, std::string_view value) {
  Tag* obj = static_cast<Tag*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
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
 *
 * @param err An error counter.
 */
void tag_helper::check_validity(error_cnt& err) const {
  const Tag* o = static_cast<const Tag*>(obj());

  if (o->tag_name().empty()) {
    ++err.config_errors;
    throw msg_fmt("Tag has no name (property 'tag_name')");
  }
  if (o->key().id() == 0) {
    ++err.config_errors;
    throw msg_fmt("Tag '{}' has a null id", o->tag_name());
  }
  if (o->key().type() == static_cast<uint32_t>(-1)) {
    ++err.config_errors;
    throw msg_fmt("Tag type must be specified");
  }
}

/**
 * @brief Initializer of the Tag object, in other words set its default values.
 */
void tag_helper::_init() {
  Tag* obj = static_cast<Tag*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->mutable_key()->set_id(0);
  obj->mutable_key()->set_type(-1);
}

/**
 * @brief Compare two tags objects and generate a DiffTag object.
 *
 * @param other The other Tag object to compare with.
 *
 * @return A DiffTag object that contains the differences between the two
 * Tag objects.
 */
void tag_helper::diff(const Container& old_list,
                      const Container& new_list,
                      const std::shared_ptr<spdlog::logger>& logger
                      [[maybe_unused]],
                      DiffTag* result) {
  if (logger->level() <= spdlog::level::trace) {
    logger->trace("tags::diff previous tags:");
    for (const auto& item : old_list)
      logger->trace(" * {}", item.DebugString());
    logger->trace("tags::diff new tags:");
    for (const auto& item : new_list)
      logger->trace(" * {}", item.DebugString());
  }

  result->Clear();
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, const Tag*> keys_values;

  // add old list keys to the map with the corresponding tag
  for (const auto& item : old_list) {
    keys_values[{item.key().id(), item.key().type()}] = &item;
  }

  absl::flat_hash_set<std::pair<uint64_t, uint32_t>> new_keys;
  for (const auto& item : new_list) {
    auto inserted = new_keys.insert({item.key().id(), item.key().type()});
    if (!keys_values.contains(*inserted.first)) {
      // New object to add
      result->add_added()->CopyFrom(item);
    } else {
      // Object to modify or equal
      if (!MessageDifferencer::Equals(item, *keys_values[*inserted.first])) {
        // There are changes in this object
        result->add_modified()->CopyFrom(item);
      }
    }
  }

  for (const auto& item : old_list) {
    if (!new_keys.contains({item.key().id(), item.key().type()}))
      result->add_deleted()->CopyFrom(item.key());
  }
}
}  // namespace com::centreon::engine::configuration
