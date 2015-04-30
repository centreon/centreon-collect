/*
** Copyright 2014-2015 Merethis
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

#include "com/centreon/broker/bam/dimension_ba_event.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/io/events.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 */
dimension_ba_event::dimension_ba_event()
  : ba_id(0),
    sla_month_percent_crit(0),
    sla_month_percent_warn(0),
    sla_duration_crit(0),
    sla_duration_warn(0) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
dimension_ba_event::dimension_ba_event(dimension_ba_event const& other)
  : io::data(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
dimension_ba_event::~dimension_ba_event() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
dimension_ba_event& dimension_ba_event::operator=(
    dimension_ba_event const& other) {
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
bool dimension_ba_event::operator==(
    dimension_ba_event const& other) const {
  return ((ba_id == other.ba_id));
}

/**
 *  Get the event type.
 *
 *  @return Event type.
 */
unsigned int dimension_ba_event::type() const {
  return (dimension_ba_event::static_type());
}

/**
 *  Get the event type.
 *
 *  @return Event type.
 */
unsigned int dimension_ba_event::static_type() {
  return (io::events::data_type<io::events::bam, bam::de_dimension_ba_event>::value);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] other Object to copy.
 */
void dimension_ba_event::_internal_copy(dimension_ba_event const& other) {
  ba_id = other.ba_id;
  ba_name = other.ba_name;
  ba_description = other.ba_description;
  sla_month_percent_crit = other.sla_month_percent_crit;
  sla_month_percent_warn = other.sla_month_percent_warn;
  sla_duration_crit = other.sla_duration_crit;
  sla_duration_warn = other.sla_duration_warn;
  return ;
}

/**************************************
*                                     *
*           Static Objects            *
*                                     *
**************************************/

// Mapping.
mapping::entry const dimension_ba_event::entries[] = {
  mapping::entry(
    &bam::dimension_ba_event::ba_id,
    "ba_id",
    mapping::entry::invalid_on_zero),
  mapping::entry(
    &bam::dimension_ba_event::ba_name,
    "ba_name"),
  mapping::entry(
    &bam::dimension_ba_event::ba_description,
    "ba_description"),
  mapping::entry(
    &bam::dimension_ba_event::sla_month_percent_crit,
    "sla_month_percent_crit"),
  mapping::entry(
    &bam::dimension_ba_event::sla_month_percent_warn,
    "sla_month_percent_warn"),
  mapping::entry(
    &bam::dimension_ba_event::sla_duration_crit,
    "sla_month_duration_crit"),
  mapping::entry(
    &bam::dimension_ba_event::sla_duration_warn,
    "sla_month_duration_warn"),
  mapping::entry()
};

// Operations.
static io::data* new_dimension_ba_event() {
  return (new dimension_ba_event);
}
io::event_info::event_operations const dimension_ba_event::operations = {
  &new_dimension_ba_event
};
