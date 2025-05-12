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
#include "com/centreon/engine/servicedependency.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

servicedependency_mmap servicedependency::servicedependencies;

/**
 *  Create a service dependency definition.
 *
 *  @param[in] dependent_hostname            Dependent host name.
 *  @param[in] dependent_service_description Dependent service
 *                                           description.
 *  @param[in] hostname                      Host name.
 *  @param[in] service_description           Service description.
 *  @param[in] dependency_type               Type of dependency.
 *  @param[in] inherits_parent               Inherits parent ?
 *  @param[in] fail_on_ok                    Does dependency fail on
 *                                           ok state ?
 *  @param[in] fail_on_warning               Does dependency fail on
 *                                           warning state ?
 *  @param[in] fail_on_unknown               Does dependency fail on
 *                                           unknown state ?
 *  @param[in] fail_on_critical              Does dependency fail on
 *                                           critical state ?
 *  @param[in] fail_on_pending               Does dependency fail on
 *                                           pending state ?
 *  @param[in] dependency_period             Dependency timeperiod name.
 *
 */
servicedependency::servicedependency(size_t key,
                                     std::string const& dependent_hostname,
                                     std::string const& dependent_svc_desc,
                                     std::string const& hostname,
                                     std::string const& service_description,
                                     dependency::types dependency_type,
                                     bool inherits_parent,
                                     bool fail_on_ok,
                                     bool fail_on_warning,
                                     bool fail_on_unknown,
                                     bool fail_on_critical,
                                     bool fail_on_pending,
                                     std::string const& dependency_period)
    : dependency{key,
                 dependent_hostname,
                 hostname,
                 dependency_type,
                 inherits_parent,
                 fail_on_pending,
                 dependency_period},
      _dependent_service_description{dependent_svc_desc},
      _service_description{service_description},
      _fail_on_ok{fail_on_ok},
      _fail_on_warning{fail_on_warning},
      _fail_on_unknown{fail_on_unknown},
      _fail_on_critical{fail_on_critical},
      master_service_ptr{nullptr},
      dependent_service_ptr{nullptr} {}

std::string const& servicedependency::get_dependent_service_description()
    const {
  return _dependent_service_description;
}

void servicedependency::set_dependent_service_description(
    std::string const& dependent_service_desciption) {
  _dependent_service_description = dependent_service_desciption;
}

std::string const& servicedependency::get_service_description() const {
  return _service_description;
}

void servicedependency::set_service_description(
    std::string const& service_description) {
  _service_description = service_description;
}

bool servicedependency::get_fail_on(int state) const {
  std::array<bool, 4> retval{_fail_on_ok, _fail_on_warning, _fail_on_critical,
                             _fail_on_unknown};
  return retval[state];
}

bool servicedependency::get_fail_on_ok() const {
  return _fail_on_ok;
}

void servicedependency::set_fail_on_ok(bool fail_on_ok) {
  _fail_on_ok = fail_on_ok;
}

bool servicedependency::get_fail_on_warning() const {
  return _fail_on_warning;
}

void servicedependency::set_fail_on_warning(bool fail_on_warning) {
  _fail_on_warning = fail_on_warning;
}

bool servicedependency::get_fail_on_unknown() const {
  return _fail_on_unknown;
}

void servicedependency::set_fail_on_unknown(bool fail_on_unknown) {
  _fail_on_unknown = fail_on_unknown;
}

bool servicedependency::get_fail_on_critical() const {
  return _fail_on_critical;
}

void servicedependency::set_fail_on_critical(bool fail_on_critical) {
  _fail_on_critical = fail_on_critical;
}

/**
 *  Dump servicedependency content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The servicedependency to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, servicedependency const& obj) {
  std::string dependency_period_str;
  if (obj.dependency_period_ptr)
    dependency_period_str = obj.dependency_period_ptr->get_name();
  std::string dependent_svc_str("\"NULL\"");
  if (obj.dependent_service_ptr) {
    dependent_svc_str = obj.dependent_service_ptr->get_hostname();
    dependent_svc_str += ", ";
    dependent_svc_str += obj.dependent_service_ptr->description();
  }
  std::string master_svc_str("\"NULL\"");
  if (obj.master_service_ptr) {
    master_svc_str = obj.master_service_ptr->get_hostname();
    master_svc_str += ", ";
    master_svc_str += obj.master_service_ptr->description();
  }

  os << "servicedependency {\n"
        "  dependency_type:               "
     << obj.get_dependency_type()
     << "\n"
        "  dependent_hostname:            "
     << obj.get_dependent_hostname()
     << "\n"
        "  dependent_service_description: "
     << obj.get_dependent_service_description()
     << "\n"
        "  hostname:                      "
     << obj.get_hostname()
     << "\n"
        "  service_description:           "
     << obj.get_service_description()
     << "\n"
        "  dependency_period:             "
     << obj.get_dependency_period()
     << "\n"
        "  inherits_parent:               "
     << obj.get_inherits_parent()
     << "\n"
        "  fail_on_ok:                    "
     << obj.get_fail_on_ok()
     << "\n"
        "  fail_on_warning:               "
     << obj.get_fail_on_warning()
     << "\n"
        "  fail_on_unknown:               "
     << obj.get_fail_on_unknown()
     << "\n"
        "  fail_on_critical:              "
     << obj.get_fail_on_critical()
     << "\n"
        "  fail_on_pending:               "
     << obj.get_fail_on_pending()
     << "\n"
        "  circular_path_checked:         "
     << obj.get_circular_path_checked()
     << "\n"
        "  contains_circular_path:        "
     << obj.get_contains_circular_path()
     << "\n"
        "  master_service_ptr:            "
     << master_svc_str
     << "\n"
        "  dependent_service_ptr:         "
     << dependent_svc_str
     << "\n"
        "  dependency_period_ptr:         "
     << dependency_period_str
     << "\n"
        "}\n";
  return os;
}

/**
 *  Checks to see if there exists a circular dependency for a service.
 *
 *  @param[in] root_dep        Root dependency.
 *  @param[in] dep             Dependency.
 *  @param[in] dependency_type Dependency type.
 *
 *  @return true if circular path was found, false otherwise.
 */
bool servicedependency::check_for_circular_servicedependency_path(
    servicedependency* dep,
    types dependency_type) {
  if (!dep)
    return false;

  // This is not the proper dependency type.
  if (_dependency_type != dependency_type ||
      dep->get_dependency_type() != dependency_type)
    return false;

  // Don't go into a loop, don't bother checking anymore if we know this
  // dependency already has a loop.
  if (_contains_circular_path)
    return true;

  // Dependency has already been checked - there is a path somewhere,
  // but it may not be for this particular dep... This should speed up
  // detection for some loops.
  if (dep->get_circular_path_checked())
    return false;

  // Set the check flag so we don't get into an infinite loop.
  dep->set_circular_path_checked(true);

  // Is this service dependent on the root service?
  // Is this host dependent on the root host?
  if (dep != this) {
    if (dependent_service_ptr == dep->master_service_ptr) {
      _contains_circular_path = true;
      dep->set_contains_circular_path(true);
      return true;
    }
  }

  // Notification dependencies are ok at this point as long as they
  // don't inherit.
  if (dependency_type == dependency::notification &&
      !dep->get_inherits_parent())
    return false;

  // Check all parent dependencies.
  for (servicedependency_mmap::iterator
           it(servicedependency::servicedependencies.begin()),
       end(servicedependency::servicedependencies.end());
       it != end; ++it) {
    // Only check parent dependencies.
    if (dep->master_service_ptr != it->second->dependent_service_ptr)
      continue;

    if (check_for_circular_servicedependency_path(it->second.get(),
                                                  dependency_type))
      return true;
  }

  return false;
}

void servicedependency::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  uint32_t errors = 0;

  // Find the dependent service.
  service_map::const_iterator found{service::services.find(
      {get_dependent_hostname(), get_dependent_service_description()})};

  if (found == service::services.end() || !found->second) {
    engine_logger(log_verification_error, basic)
        << "Error: Dependent service '" << get_dependent_service_description()
        << "' on host '" << get_dependent_hostname()
        << "' specified in service dependency for service '"
        << get_service_description() << "' on host '" << get_hostname()
        << "' is not defined anywhere!";
    config_logger->error(
        "Error: Dependent service '{}' on host '{}' specified in service "
        "dependency for service '{}' on host '{}' is not defined anywhere!",
        get_dependent_service_description(), get_dependent_hostname(),
        get_service_description(), get_service_description());
    errors++;
    dependent_service_ptr = nullptr;
  } else
    dependent_service_ptr = found->second.get();

  // Save pointer for later.
  found = service::services.find({get_hostname(), get_service_description()});

  // Find the service we're depending on.
  if (found == service::services.end() || !found->second) {
    engine_logger(log_verification_error, basic)
        << "Error: Service '" << get_service_description() << "' on host '"
        << get_hostname() << "' specified in service dependency for service '"
        << get_dependent_service_description() << "' on host '"
        << get_dependent_hostname() << "' is not defined anywhere!";
    config_logger->error(
        "Error: Service '{}' on host '{}' specified in service dependency for "
        "service '{}' on host '{}' is not defined anywhere!",
        get_service_description(), get_hostname(),
        get_dependent_service_description(), get_dependent_hostname());
    errors++;
    master_service_ptr = nullptr;
  } else
    // Save pointer for later.
    master_service_ptr = found->second.get();

  // Make sure they're not the same service.
  if (dependent_service_ptr == master_service_ptr &&
      dependent_service_ptr != nullptr) {
    engine_logger(log_verification_error, basic)
        << "Error: Service dependency definition for service '"
        << get_dependent_service_description() << "' on host '"
        << get_dependent_hostname() << "' is circular (it depends on itself)!";
    config_logger->error(
        "Error: Service dependency definition for service '{}' on host '{}' is "
        "circular (it depends on itself)!",
        get_dependent_service_description(), get_dependent_hostname());
    errors++;
  }

  // Find the timeperiod.
  if (!get_dependency_period().empty()) {
    timeperiod_map::const_iterator it{
        timeperiod::timeperiods.find(get_dependency_period())};

    if (it == timeperiod::timeperiods.end() || !it->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Dependency period '" << get_dependency_period()
          << "' specified in service dependency for service '"
          << get_dependent_service_description() << "' on host '"
          << get_dependent_hostname() << "' is not defined anywhere!";
      config_logger->error(
          "Error: Dependency period '{}' specified in service dependency for "
          "service '{}' on host '{}' is not defined anywhere!",
          get_dependency_period(), get_dependent_service_description(),
          get_dependent_hostname());
      errors++;
      dependency_period_ptr = nullptr;
    } else
      // Save the timeperiod pointer for later.
      dependency_period_ptr = it->second.get();
  }

  // Add errors.
  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve service dependency";
  }
}

/**
 * @brief Find a service dependency from the given key.
 *
 * @param key A tuple containing a host name, a service description and a hash
 * matching the service dependency.
 *
 * @return Iterator to the element if found, servicedependencies().end()
 * otherwise.
 */
servicedependency_mmap::iterator servicedependency::servicedependencies_find(
    const std::tuple<std::string, std::string, size_t>& key) {
  size_t k = std::get<2>(key);
  std::pair<servicedependency_mmap::iterator, servicedependency_mmap::iterator>
      p = servicedependencies.equal_range({std::get<0>(key), std::get<1>(key)});
  while (p.first != p.second) {
    if (p.first->second->internal_key() == k)
      break;
    ++p.first;
  }
  return p.first == p.second ? servicedependencies.end() : p.first;
}
