/*
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/configuration/severity.hh"
#include "com/centreon/engine/exceptions/error.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

#define SETTER(type, method) \
  &object::setter<severity, type, &severity::method>::generic

const absl::flat_hash_map<std::string, severity::setter_func>
    severity::_setters{
        {"name", SETTER(const std::string&, _set_name)},
        {"severity_name", SETTER(const std::string&, _set_name)},
        {"id", SETTER(int, _set_id)},
        {"severity_id", SETTER(int, _set_id)},
        {"level", SETTER(int32_t, _set_level)},
        {"severity_level", SETTER(int32_t, _set_level)},
    };

/**
 * @brief Constructor
 *
 * @param key The unique id corresponding to this severity.
 */
severity::severity(const key_type& key)
    : object(object::severity), _id{key}, _level{0} {}

/**
 * @brief Copy constructor.
 *
 * @param other The severity to copy.
 */
severity::severity(const severity& other)
    : object(other), _id{other._id}, _level{other._level}, _name{other._name} {}

/**
 * @brief Assign operator.
 *
 * @param other The severity to copy.
 *
 * @return A reference to the copy.
 */
severity& severity::operator=(const severity& other) {
  if (this != &other) {
    object::operator=(other);
    _id = other._id;
    _level = other._level;
    _name = other._name;
  }
  return *this;
}

/**
 * @brief Equal operator.
 *
 * @param other The severity to compare with.
 *
 * @return True if objects are equal, False otherwise.
 */
bool severity::operator==(const severity& other) const noexcept {
  return _id == other._id && _level == other._level && _name == other._name;
}

/**
 * @brief Inequality operator.
 *
 * @param other The severity to compare with.
 *
 * @return False if objects are equal, True otherwise.
 */
bool severity::operator!=(const severity& other) const noexcept {
  return _id != other._id || _level != other._level || _name != other._name;
}

/**
 * @brief Less-than operator.
 *
 * @param other The severity to compare with.
 *
 * @return True if this objects is less than other.
 */
bool severity::operator<(const severity& other) const noexcept {
  if (_id != other._id)
    return _id < other._id;
  else if (_level != other._level)
    return _level < other._level;
  return _name < other._name;
}

/**
 * @brief Check if the object is valid.
 *
 * If the object is not valid, an exception is thrown.
 */
void severity::check_validity() const {
  if (_name.empty())
    throw engine_error() << "Severity has no name (property 'name')";
  if (_id <= 0)
    throw engine_error()
        << "Severity id must not be less than 1 (property 'id')";
  if (_level <= 0)
    throw engine_error()
        << "Severity level must not be less than 1 (property 'level')";
}

/**
 * @brief Get severity key.
 *
 * @return Severity id.
 */
const severity::key_type& severity::key() const noexcept {
  return _id;
}

/**
 * @brief Get severity level.
 *
 * @return Severity level.
 */
int32_t severity::level() const noexcept {
  return _level;
}

/**
 * @brief Get severity name.
 *
 * @return Severity name.
 */
const std::string& severity::name() const noexcept {
  return _name;
}

/**
 * @brief Parse and set the severity property.
 *
 * @param key    The property name.
 * @param value  The property value.
 *
 * @return True on success, otherwise false.
 */
bool severity::parse(const char* key, const char* value) {
  absl::flat_hash_map<std::string, severity::setter_func>::const_iterator it =
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
void severity::merge(const object&) {}

/**
 * @brief Set id value.
 *
 * @param id A positive integer.
 *
 * @return True on success.
 */
bool severity::_set_id(int32_t id) {
  if (id > 0) {
    _id = id;
    return true;
  } else
    return false;
}

/**
 * @brief Set level value.
 *
 * @param level A positive integer.
 *
 * @return True on success.
 */
bool severity::_set_level(int32_t level) {
  if (level > 0) {
    _level = level;
    return true;
  } else
    return false;
}

/**
 * @brief Set name value.
 *
 * @param name A string.
 *
 * @return True on success.
 */
bool severity::_set_name(const std::string& name) {
  _name = name;
  return true;
}
