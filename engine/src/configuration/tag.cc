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

#include "com/centreon/engine/configuration/tag.hh"
#include "com/centreon/engine/exceptions/error.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

#define SETTER(type, method) &object::setter<tag, type, &tag::method>::generic

const absl::flat_hash_map<std::string, tag::setter_func> tag::_setters{
    {"tag_name", SETTER(const std::string&, _set_tag_name)},
    {"id", SETTER(uint64_t, _set_id)},
    {"tag_id", SETTER(uint64_t, _set_id)},
    {"type", SETTER(const std::string&, _set_type)},
    {"tag_type", SETTER(const std::string&, _set_type)},
};

/**
 * @brief Constructor
 *
 * @param key The unique id corresponding to this tag.
 */
tag::tag(const key_type& key) : object(object::tag), _key{key} {}

/**
 * @brief Copy constructor.
 *
 * @param other The tag to copy.
 */
tag::tag(const tag& other)
    : object(other), _key{other._key}, _tag_name{other._tag_name} {}

/**
 * @brief Assign operator.
 *
 * @param other The tag to copy.
 *
 * @return A reference to the copy.
 */
tag& tag::operator=(const tag& other) {
  if (this != &other) {
    object::operator=(other);
    _key = other._key;
    _tag_name = other._tag_name;
  }
  return *this;
}

/**
 * @brief Equal operator.
 *
 * @param other The tag to compare with.
 *
 * @return True if objects are equal, False otherwise.
 */
bool tag::operator==(const tag& other) const noexcept {
  return _key == other._key && _tag_name == other._tag_name;
}

/**
 * @brief Inequality operator.
 *
 * @param other The tag to compare with.
 *
 * @return False if objects are equal, True otherwise.
 */
bool tag::operator!=(const tag& other) const noexcept {
  return _key != other._key || _tag_name != other._tag_name;
}

/**
 * @brief Less-than operator.
 *
 * @param other The tag to compare with.
 *
 * @return True if this objects is less than other.
 */
bool tag::operator<(const tag& other) const noexcept {
  if (_key != other._key)
    return _key < other._key;
  return _tag_name < other._tag_name;
}

/**
 * @brief Check if the object is valid.
 *
 * * name must not be empty
 * * id must be > 0
 * * type must be > 0
 *
 * If the object is not valid, an exception is thrown.
 */
void tag::check_validity() const {
  if (_tag_name.empty())
    throw engine_error() << "Tag has no name (property 'name')";
  if (_key.first == 0)
    throw engine_error() << "Tag id must not be less than 1 (property 'id')";
  if (_key.second == static_cast<uint16_t>(-1))
    throw engine_error() << "Tag type must be defined (property 'type')";
}

/**
 * @brief Get tag key.
 *
 * @return Severity id.
 */
const tag::key_type& tag::key() const noexcept {
  return _key;
}

/**
 * @brief Get tag name.
 *
 * @return Severity name.
 */
const std::string& tag::tag_name() const noexcept {
  return _tag_name;
}

/**
 * @brief Parse and set the tag property.
 *
 * @param key    The property name.
 * @param value  The property value.
 *
 * @return True on success, otherwise false.
 */
bool tag::parse(const char* key, const char* value) {
  absl::flat_hash_map<std::string, tag::setter_func>::const_iterator it =
      _setters.find(key);
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 * @brief Merge object.
 *
 * @param obj The object to merge.
 *
 * Not used in this context.
 */
void tag::merge(const object&) {}

/**
 * @brief Set id value.
 *
 * @param id A positive integer.
 *
 * @return True on success.
 */
bool tag::_set_id(uint64_t id) {
  if (id > 0) {
    _key.first = id;
    return true;
  } else
    return false;
}

/**
 * @brief Set type value.
 *
 * @param type a string among hostcategory, servicecategory, hostgroup,
 * servicegroup.
 *
 * @return True on success.
 */
bool tag::_set_type(const std::string& typ) {
  if (typ == "hostcategory")
    _key.second = tag::hostcategory;
  else if (typ == "servicecategory")
    _key.second = tag::servicecategory;
  else if (typ == "hostgroup")
    _key.second = tag::hostgroup;
  else if (typ == "servicegroup")
    _key.second = tag::servicegroup;
  else
    return false;
  return true;
}

/**
 * @brief Set name value.
 *
 * @param name A string.
 *
 * @return True on success.
 */
bool tag::_set_tag_name(const std::string& name) {
  _tag_name = name;
  return true;
}
