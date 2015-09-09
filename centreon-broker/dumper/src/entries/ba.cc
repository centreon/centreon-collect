/*
** Copyright 2015 Centreon
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

#include <cmath>
#include "com/centreon/broker/dumper/entries/ba.hh"
#include "com/centreon/broker/dumper/internal.hh"
#include "com/centreon/broker/io/events.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::dumper::entries;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
ba::ba()
  : enable(true),
    poller_id(0),
    ba_id(0),
    level_critical(NAN),
    level_warning(NAN),
    organization_id(0),
    type_id(0) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
ba::ba(ba const& other) : io::data(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
ba::~ba() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
ba& ba::operator=(ba const& other) {
  if (this != &other) {
    io::data::operator=(other);
    _internal_copy(other);
  }
  return (*this);
}

/**
 *  Equality operator.
 *
 *  @param[in] other  Object to compare to.
 *
 *  @return True if both objects are equal.
 */
bool ba::operator==(ba const& other) const {
  return ((enable == other.enable)
          && (poller_id == other.poller_id)
          && (ba_id == other.ba_id)
          && (description == other.description)
          && (level_critical == other.level_critical)
          && (level_warning == other.level_warning)
          && (name == other.name)
          && (organization_id == other.organization_id)
          && (type_id == other.type_id));
}

/**
 *  Inequality operator.
 *
 *  @param[in] other  Object to compare to.
 *
 *  @return True if both objects are not equal.
 */
bool ba::operator!=(ba const& other) const {
  return (!operator==(other));
}

/**
 *  Get event type.
 *
 *  @return Event type.
 */
unsigned int ba::type() const {
  return (static_type());
}

/**
 *  Static type of event class.
 *
 *  @return Static type of event class.
 */
unsigned int ba::static_type() {
  return (io::events::data_type<io::events::dumper, dumper::de_entries_ba>::value);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data objects.
 *
 *  @param[in] other  Object to copy.
 */
void ba::_internal_copy(ba const& other) {
  enable = other.enable;
  poller_id = other.poller_id;
  ba_id = other.ba_id;
  description = other.description;
  level_critical = other.level_critical;
  level_warning = other.level_warning;
  name = other.name;
  organization_id = other.organization_id;
  type_id = other.type_id;
  return ;
}

/**************************************
*                                     *
*           Static Objects            *
*                                     *
**************************************/

// Mapping.
mapping::entry const ba::entries[] = {
  mapping::entry(
    &ba::enable,
    ""),
  mapping::entry(
    &ba::poller_id,
    "",
    mapping::entry::invalid_on_zero),
  mapping::entry(
    &ba::ba_id,
    "ba_id",
    mapping::entry::invalid_on_zero),
  mapping::entry(
    &ba::description,
    "description"),
  mapping::entry(
    &ba::level_critical,
    "level_c"),
  mapping::entry(
    &ba::level_warning,
    "level_w"),
  mapping::entry(
    &ba::name,
    "name"),
  mapping::entry(
    &ba::organization_id,
    "organization_id",
    mapping::entry::invalid_on_zero),
  mapping::entry(
    &ba::type_id,
    "ba_type_id",
    mapping::entry::invalid_on_zero),
  mapping::entry()
};

// Operations.
static io::data* new_ba() {
  return (new ba);
}
io::event_info::event_operations const ba::operations = {
  &new_ba
};
