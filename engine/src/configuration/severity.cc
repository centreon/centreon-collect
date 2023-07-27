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

#include "com/centreon/engine/configuration/severity.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

#define SETTER(type, method) \
  &object::setter<severity, type, &severity::method>::generic

const absl::flat_hash_map<std::string, severity::setter_func>
    severity::_setters{
        {"severity_name", SETTER(const std::string&, _set_severity_name)},
        {"id", SETTER(uint64_t, _set_id)},
        {"severity_id", SETTER(uint64_t, _set_id)},
        {"level", SETTER(uint32_t, _set_level)},
        {"severity_level", SETTER(uint32_t, _set_level)},
        {"icon_id", SETTER(uint64_t, _set_icon_id)},
        {"severity_icon_id", SETTER(uint64_t, _set_icon_id)},
        {"severity_type", SETTER(const std::string&, _set_type)},
        {"type", SETTER(const std::string&, _set_type)},
    };

/**
 * @brief Constructor
 *
 * @param key The unique id corresponding to this severity.
 */
severity::severity(const key_type& key)
    : object(object::severity), _key{key}, _level{0}, _icon_id{0} {}

/**
 * @brief Copy constructor.
 *
 * @param other The severity to copy.
 */
severity::severity(const severity& other)
    : object(other),
      _key{other._key},
      _level{other._level},
      _icon_id{other._icon_id},
      _severity_name{other._severity_name} {}

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
    _key = other._key;
    _level = other._level;
    _icon_id = other._icon_id;
    _severity_name = other._severity_name;
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
  return _key == other._key && _level == other._level &&
         _icon_id == other._icon_id && _severity_name == other._severity_name;
}

/**
 * @brief Inequality operator.
 *
 * @param other The severity to compare with.
 *
 * @return False if objects are equal, True otherwise.
 */
bool severity::operator!=(const severity& other) const noexcept {
  return _key != other._key || _level != other._level ||
         _icon_id != other._icon_id || _severity_name != other._severity_name;
}

/**
 * @brief Less-than operator.
 *
 * @param other The severity to compare with.
 *
 * @return True if this objects is less than other.
 */
bool severity::operator<(const severity& other) const noexcept {
  if (_key != other._key)
    return _key < other._key;
  else if (_level != other._level)
    return _level < other._level;
  else if (_icon_id != other._icon_id)
    return _icon_id < other._icon_id;
  return _severity_name < other._severity_name;
}

/**
 * @brief Check if the object is valid.
 *
 * * name must not be empty
 * * id must be > 0
 * * level must be > 0
 * * no criteria on icon_id, if 0 there is no icon.
 *
 * If the object is not valid, an exception is thrown.
 */
void severity::check_validity() const {
  if (_severity_name.empty())
    throw exceptions::msg_fmt(
        "Severity has no name (property 'severity_name')");
  if (_key.first == 0)
    throw exceptions::msg_fmt(
        "Severity id must not be less than 1 (property 'id')");
  if (_level == 0)
    throw exceptions::msg_fmt(
        "Severity level must not be less than 1 (property 'level')");
  if (_key.second == severity::none)
    throw exceptions::msg_fmt(
        "Severity type must be one of 'service' or 'host'");
}

/**
 * @brief Get severity key.
 *
 * @return Severity id.
 */
const severity::key_type& severity::key() const noexcept {
  return _key;
}

/**
 * @brief Get severity level.
 *
 * @return Severity level.
 */
uint32_t severity::level() const noexcept {
  return _level;
}

/**
 * @brief Get severity icon_id.
 *
 * @return Severity icon_id.
 */
uint64_t severity::icon_id() const noexcept {
  return _icon_id;
}

/**
 * @brief Get severity name.
 *
 * @return Severity name.
 */
const std::string& severity::severity_name() const noexcept {
  return _severity_name;
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
bool severity::_set_id(uint64_t id) {
  if (id > 0) {
    _key.first = id;
    return true;
  } else {
    _key.first = 0;
    return false;
  }
}

/**
 * @brief Set level value.
 *
 * @param level A positive integer.
 *
 * @return True on success.
 */
bool severity::_set_level(uint32_t level) {
  if (level > 0) {
    _level = level;
    return true;
  } else
    return false;
}

/**
 * @brief Set icon_id value.
 *
 * @param icon_id A positive integer.
 *
 * @return True on success.
 */
bool severity::_set_icon_id(uint64_t icon_id) {
  _icon_id = icon_id;
  return true;
}

/**
 * @brief Set name value.
 *
 * @param name A string.
 *
 * @return True on success.
 */
bool severity::_set_severity_name(const std::string& name) {
  _severity_name = name;
  return true;
}

/**
 * @brief Set the type of the severity. We only have two possibilities,
 * the type can be "service" or "host".
 *
 * @param typ A string
 *
 * @return true on success, false otherwise.
 */
bool severity::_set_type(const std::string& typ) {
  if (typ == "service") {
    _key.second = severity::service;
    return true;
  } else if (typ == "host") {
    _key.second = severity::host;
    return true;
  } else {
    _key.second = severity::none;
    return false;
  }
}

uint16_t severity::type() const noexcept {
  return _key.second;
}
