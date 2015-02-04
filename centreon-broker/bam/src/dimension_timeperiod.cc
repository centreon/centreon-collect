/*
** Copyright 2014 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/broker/bam/dimension_timeperiod.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/io/events.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 */
dimension_timeperiod::dimension_timeperiod() {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
dimension_timeperiod::dimension_timeperiod(
    dimension_timeperiod const& other)
  : io::data(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
dimension_timeperiod::~dimension_timeperiod() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
dimension_timeperiod& dimension_timeperiod::operator=(
                                              dimension_timeperiod const& other) {
  if (this != &other) {
    io::data::operator=(other);
    _internal_copy(other);
  }
  return (*this);
}

/**
 *  Equality test operator.
 *
 *  @param[in] other  The object to test for equality.
 *
 *  @return  True if the two objects are equal.
 */
bool dimension_timeperiod::operator==(
                             dimension_timeperiod const& other) const {
  return ((id == other.id)
          && (name == other.name)
          && (monday == other.monday)
          && (tuesday == other.tuesday)
          && (wednesday == other.wednesday)
          && (thursday == other.thursday)
          && (friday == other.friday)
          && (saturday == other.saturday)
          && (sunday == other.sunday));
}


/**
 *  Get the event type.
 *
 *  @return Event type.
 */
unsigned int dimension_timeperiod::type() const {
  return (io::events::data_type<io::events::bam,
                                bam::de_dimension_timeperiod>::value);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] other Object to copy.
 */
void dimension_timeperiod::_internal_copy(
                             dimension_timeperiod const& other) {
  id = other.id;
  name = other.name;
  monday = other.monday;
  tuesday = other.tuesday;
  wednesday = other.wednesday;
  thursday = other.thursday;
  friday = other.friday;
  saturday = other.saturday;
  sunday = other.sunday;
  return ;
}
