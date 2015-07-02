/*
** Copyright 2015 Merethis
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

#include "com/centreon/broker/dumper/db_dump.hh"
#include "com/centreon/broker/dumper/internal.hh"
#include "com/centreon/broker/io/events.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::dumper;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
db_dump::db_dump() : commit(false), full(false) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
db_dump::db_dump(db_dump const& other) : io::data(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
db_dump::~db_dump() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
db_dump& db_dump::operator=(db_dump const& other) {
  if (this != &other) {
    io::data::operator=(other);
    _internal_copy(other);
  }
  return (*this);
}

/**
 *  Get event type.
 *
 *  @return Event type.
 */
unsigned int db_dump::type() const {
  return (static_type());
}

/**
 *  Get event class type.
 *
 *  @return Event class type.
 */
unsigned int db_dump::static_type() {
  return (io::events::data_type<io::events::dumper, dumper::de_db_dump>::value);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] other  Object to copy.
 */
void db_dump::_internal_copy(db_dump const& other) {
  commit = other.commit;
  full = other.full;
  return ;
}
