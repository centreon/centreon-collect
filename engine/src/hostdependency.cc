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
#include "com/centreon/engine/hostdependency.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/hostdependency.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

hostdependency_mmap hostdependency::hostdependencies;

/**
 *  Create a host dependency definition. The key is given by the
 *  applier::configuration::hostdependency object from its attributes.
 *
 *  @param[in] key                 key representing this hostdependency.
 *  @param[in] dependent_hostname  Dependant host name.
 *  @param[in] hostname            Host name.
 *  @param[in] dependency_type     Dependency type.
 *  @param[in] inherits_parent     Do we inherits from parent ?
 *  @param[in] fail_on_up          Does dependency fail on up ?
 *  @param[in] fail_on_down        Does dependency fail on down ?
 *  @param[in] fail_on_unreachable Does dependency fail on unreachable ?
 *  @param[in] fail_on_pending     Does dependency fail on pending ?
 *  @param[in] dependency_period   Dependency period.
 *
 *  @return New host dependency.
 */
hostdependency::hostdependency(size_t key,
                               std::string const& dependent_hostname,
                               std::string const& hostname,
                               dependency::types dependency_type,
                               bool inherits_parent,
                               bool fail_on_up,
                               bool fail_on_down,
                               bool fail_on_unreachable,
                               bool fail_on_pending,
                               std::string const& dependency_period)
    : dependency(key,
                 dependent_hostname,
                 hostname,
                 dependency_type,
                 inherits_parent,
                 fail_on_pending,
                 dependency_period),
      master_host_ptr{nullptr},
      dependent_host_ptr{nullptr},
      _fail_on_up{fail_on_up},
      _fail_on_down{fail_on_down},
      _fail_on_unreachable{fail_on_unreachable} {}

bool hostdependency::get_fail_on(int state) const {
  std::array<bool, 3> retval{_fail_on_up, _fail_on_down, _fail_on_unreachable};
  return retval[state];
}

bool hostdependency::get_fail_on_up() const {
  return _fail_on_up;
}

void hostdependency::set_fail_on_up(bool fail_on_up) {
  _fail_on_up = fail_on_up;
}

bool hostdependency::get_fail_on_down() const {
  return _fail_on_down;
}

void hostdependency::set_fail_on_down(bool fail_on_down) {
  _fail_on_down = fail_on_down;
}

bool hostdependency::get_fail_on_unreachable() const {
  return _fail_on_unreachable;
}

void hostdependency::set_fail_on_unreachable(bool fail_on_unreachable) {
  _fail_on_unreachable = fail_on_unreachable;
}

/**
 *  Dump hostdependency content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The hostdependency to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, hostdependency const& obj) {
  std::string dependency_period_str;
  if (obj.dependency_period_ptr)
    dependency_period_str = obj.dependency_period_ptr->get_name();

  os << "hostdependency {\n"
        "  dependency_type:        "
     << obj.get_dependency_type()
     << "\n"
        "  dependent_hostname:    "
     << obj.get_dependent_hostname()
     << "\n"
        "  hostname:               "
     << obj.get_hostname()
     << "\n"
        "  dependency_period:      "
     << obj.get_dependency_period()
     << "\n"
        "  inherits_parent:        "
     << obj.get_inherits_parent()
     << "\n"
        "  fail_on_up:             "
     << obj.get_fail_on_up()
     << "\n"
        "  fail_on_down:           "
     << obj.get_fail_on_down()
     << "\n"
        "  fail_on_unreachable:    "
     << obj.get_fail_on_unreachable()
     << "\n"
        "  fail_on_pending:        "
     << obj.get_fail_on_pending()
     << "\n"
        "  circular_path_checked:  "
     << obj.get_circular_path_checked()
     << "\n"
        "  contains_circular_path: "
     << obj.get_contains_circular_path()
     << "\n"
        "  master_host_ptr:        "
     << (obj.master_host_ptr ? obj.master_host_ptr->name() : "\"NULL\"")
     << "\n"
        "  dependent_host_ptr:     "
     << (obj.dependent_host_ptr ? obj.dependent_host_ptr->name() : "\"NULL\"")
     << "\n"
        "  dependency_period_ptr:  "
     << dependency_period_str
     << "\n"
        "}\n";
  return os;
}

/**
 *  Checks to see if there exists a circular dependency for a host.
 *
 *  @param[in] root_dep        Root dependency.
 *  @param[in] dep             Dependency.
 *  @param[in] dependency_type Dependency type.
 *
 *  @return true if circular path was found, false otherwise.
 */
bool hostdependency::check_for_circular_hostdependency_path(
    hostdependency* dep,
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

  // Is this host dependent on the root host?
  if (dep != this) {
    if (dependent_host_ptr == dep->master_host_ptr) {
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
  for (hostdependency_mmap::iterator
           it(hostdependency::hostdependencies.begin()),
       end(hostdependency::hostdependencies.end());
       it != end; ++it) {
    // Only check parent dependencies.
    if (dep->master_host_ptr != it->second->dependent_host_ptr)
      continue;

    if (check_for_circular_hostdependency_path(it->second.get(),
                                               dependency_type))
      return true;
  }

  return false;
}

void hostdependency::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  int errors = 0;

  // Find the dependent host.
  host_map::const_iterator it = host::hosts.find(_dependent_hostname);
  if (it == host::hosts.end() || !it->second) {
    engine_logger(log_verification_error, basic)
        << "Error: Dependent host specified in host dependency for "
           "host '"
        << _dependent_hostname << "' is not defined anywhere!";
    config_logger->error(
        "Error: Dependent host specified in host dependency for "
        "host '{}' is not defined anywhere!",
        _dependent_hostname);
    errors++;
    dependent_host_ptr = nullptr;
  } else
    dependent_host_ptr = it->second.get();

  // Find the host we're depending on.
  it = host::hosts.find(_hostname);
  if (it == host::hosts.end() || !it->second) {
    engine_logger(log_verification_error, basic)
        << "Error: Host specified in host dependency for host '"
        << _dependent_hostname << "' is not defined anywhere!";
    config_logger->error(
        "Error: Host specified in host dependency for host '{}' is not defined "
        "anywhere!",
        _dependent_hostname);
    errors++;
    master_host_ptr = nullptr;
  } else
    master_host_ptr = it->second.get();

  // Make sure they're not the same host.
  if (dependent_host_ptr == master_host_ptr && dependent_host_ptr != nullptr) {
    engine_logger(log_verification_error, basic)
        << "Error: Host dependency definition for host '" << _dependent_hostname
        << "' is circular (it depends on itself)!";
    config_logger->error(
        "Error: Host dependency definition for host '{}' is circular (it "
        "depends on itself)!",
        _dependent_hostname);
    errors++;
  }

  // Find the timeperiod.
  if (!_dependency_period.empty()) {
    timeperiod_map::const_iterator it{
        timeperiod::timeperiods.find(_dependency_period)};

    if (it == timeperiod::timeperiods.end() || !it->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Dependency period '" << this->get_dependency_period()
          << "' specified in host dependency for host '" << _dependent_hostname
          << "' is not defined anywhere!";
      config_logger->error(
          "Error: Dependency period '{}' specified in host dependency for host "
          "'{}' is not defined anywhere!",
          this->get_dependency_period(), _dependent_hostname);
      errors++;
      dependency_period_ptr = nullptr;
    } else
      dependency_period_ptr = it->second.get();
  }

  // Add errors.
  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve host dependency";
  }
}

/**
 *  Find a service dependency from its key.
 *
 *  @param[in] k The service dependency configuration.
 *
 *  @return Iterator to the element if found,
 *          servicedependencies().end() otherwise.
 */
hostdependency_mmap::iterator hostdependency::hostdependencies_find(
    const std::pair<std::string_view, size_t>& key) {
  std::pair<hostdependency_mmap::iterator, hostdependency_mmap::iterator> p;

  p = hostdependencies.equal_range(key.first);
  while (p.first != p.second) {
    if (p.first->second->internal_key() == key.second)
      break;
    ++p.first;
  }
  return p.first == p.second ? hostdependencies.end() : p.first;
}
