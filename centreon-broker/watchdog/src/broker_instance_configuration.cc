/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/watchdog/broker_instance_configuration.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::watchdog;

/**
 *  Default constructor.
 */
broker_instance_configuration::broker_instance_configuration()
  : _run(false), _reload(false), _seconds_per_tentative(0) {}

/**
 *  Constructor.
 *
 *  @param[in] name           The name of the instance.
 *  @param[in] config_file    The config file of the instance.
 *  @param[in] should_run     Should this instance be run?
 *  @param[in] should_reload  Should this instance be reloaded on SIGHUP?
 *  @param[in] seconds_per_tentative  The number of seconds between tentatives.
 */
broker_instance_configuration::broker_instance_configuration(
                                 std::string const& name,
                                 std::string const& config_file,
                                 bool should_run,
                                 bool should_reload,
                                 unsigned int seconds_per_tentative)
  : _name(name),
    _config_file(config_file),
    _run(should_run),
    _reload(should_reload),
    _seconds_per_tentative(seconds_per_tentative) {}
/**
 *  Destructor.
 */
broker_instance_configuration::~broker_instance_configuration() {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
broker_instance_configuration::broker_instance_configuration(
                                 broker_instance_configuration const& other)
  : _name(other._name),
    _config_file(other._config_file),
    _run(other._run),
    _reload(other._reload),
   _seconds_per_tentative(other._seconds_per_tentative) {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return  A reference to this object.
 */
broker_instance_configuration& broker_instance_configuration::operator=(
                                 broker_instance_configuration const& other) {
  if (this != &other) {
    _name = other._name;
    _config_file = other._config_file;
    _run = other._run;
    _reload = other._reload;
    _seconds_per_tentative = other._seconds_per_tentative;
  }
  return (*this);
}

/**
 *  Get the name of this instance.
 *
 *  @return[in]  The name of this instance.
 */
std::string const& broker_instance_configuration::get_name() const throw() {
  return (_name);
}

/**
 *  Get the configuration file for this instance.
 *
 *  @return[in]  The configuration file for this instance.
 */
std::string const&
  broker_instance_configuration::get_config_file() const throw() {
  return (_config_file);
}

/**
 *  Should this instance be run?
 *
 *  @return  True if this instance should be run.
 */
bool broker_instance_configuration::should_run() const throw() {
  return (_run);
}

/**
 *  Should this instance be reloaded?
 *
 *  @return  True if this instance should be reloaded.
 */
bool broker_instance_configuration::should_reload() const throw() {
  return (_reload);
}

/**
 *  How many seconds between each tentative?
 *
 *  @return  How many seconds between restart.
 */
unsigned int broker_instance_configuration::seconds_per_tentative() const throw() {
  return (_seconds_per_tentative);
}
