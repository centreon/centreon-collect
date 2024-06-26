/**
 * Copyright 2011-2024 Centreon
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
#include "com/centreon/engine/dependency.hh"

#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

dependency::dependency(size_t key,
                       const std::string& dependent_hostname,
                       const std::string& hostname,
                       types dependency_type,
                       bool inherits_parent,
                       bool fail_on_pending,
                       const std::string& dependency_period)
    : _internal_key{key},
      _dependency_type{dependency_type},
      _dependent_hostname{dependent_hostname},
      _hostname{hostname},
      _dependency_period{dependency_period},
      _inherits_parent{inherits_parent},
      _fail_on_pending{fail_on_pending},
      _circular_path_checked{false},
      _contains_circular_path{false} {
  if (dependent_hostname.empty() || hostname.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: NULL host name in host dependency definition";
    config_logger->error("Error: NULL host name in host dependency definition");
    throw engine_error() << "Could not create execution "
                         << "dependency of '" << dependent_hostname << "' on '"
                         << hostname << "'";
  }
}

dependency::types dependency::get_dependency_type() const {
  return _dependency_type;
}

void dependency::set_dependency_type(types dependency_type) {
  _dependency_type = dependency_type;
}

std::string const& dependency::get_dependent_hostname() const {
  return _dependent_hostname;
}

void dependency::set_dependent_hostname(std::string const& dependent_hostname) {
  _dependent_hostname = dependent_hostname;
}

std::string const& dependency::get_hostname() const {
  return _hostname;
}

void dependency::set_hostname(std::string const& hostname) {
  _hostname = hostname;
}

std::string const& dependency::get_dependency_period() const {
  return _dependency_period;
}

void dependency::set_dependency_period(std::string const& dependency_period) {
  _dependency_period = dependency_period;
}

bool dependency::get_inherits_parent() const {
  return _inherits_parent;
}

void dependency::set_inherits_parent(bool inherits_parent) {
  _inherits_parent = inherits_parent;
}

bool dependency::get_fail_on_pending() const {
  return _fail_on_pending;
}

void dependency::set_fail_on_pending(bool fail_on_pending) {
  _fail_on_pending = fail_on_pending;
}

bool dependency::get_circular_path_checked() const {
  return _circular_path_checked;
}

void dependency::set_circular_path_checked(bool circular_path_checked) {
  _circular_path_checked = circular_path_checked;
}

bool dependency::get_contains_circular_path() const {
  return _contains_circular_path;
}

void dependency::set_contains_circular_path(bool contains_circular_path) {
  _contains_circular_path = contains_circular_path;
}

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool dependency::operator==(dependency const& obj) noexcept {
  return _dependency_type == obj.get_dependency_type() &&
         _dependent_hostname == obj.get_dependent_hostname() &&
         _hostname == obj.get_hostname() &&
         _dependency_period == obj.get_dependency_period() &&
         _inherits_parent == obj.get_inherits_parent() &&
         _fail_on_pending == obj.get_fail_on_pending() &&
         _circular_path_checked == obj.get_circular_path_checked() &&
         _contains_circular_path == obj.get_contains_circular_path();
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool dependency::operator!=(dependency const& obj) noexcept {
  return !(*this == obj);
}

/**
 *  Less-than operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if the first object is strictly less than the second.
 */
bool dependency::operator<(dependency const& obj) noexcept {
  if (_dependent_hostname != obj.get_dependent_hostname())
    return _dependent_hostname < obj.get_dependent_hostname();
  else if (_hostname != obj.get_hostname())
    return _hostname < obj.get_hostname();
  else if (_dependency_type != obj.get_dependency_type())
    return _dependency_type < obj.get_dependency_type();
  else if (_dependency_period != obj.get_dependency_period())
    return _dependency_period < obj.get_dependency_period();
  else if (_inherits_parent != obj.get_inherits_parent())
    return _inherits_parent < obj.get_inherits_parent();
  else if (_fail_on_pending != obj.get_fail_on_pending())
    return _fail_on_pending < obj.get_fail_on_pending();
  else if (_circular_path_checked != obj.get_circular_path_checked())
    return _circular_path_checked < obj.get_circular_path_checked();
  return _contains_circular_path < obj.get_contains_circular_path();
}

size_t dependency::internal_key() const {
  return _internal_key;
}
