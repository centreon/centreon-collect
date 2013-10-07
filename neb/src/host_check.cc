/*
** Copyright 2009-2013 Merethis
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

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/host_check.hh"
#include "com/centreon/broker/neb/internal.hh"

using namespace com::centreon::broker::neb;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
host_check::host_check() {}

/**
 *  Copy constructor.
 *
 *  @param[in] hc Object to copy.
 */
host_check::host_check(host_check const& hc) : check(hc) {}

/**
 *  Destructor.
 */
host_check::~host_check() {}

/**
 *  Assignment operator.
 *
 *  @param[in] hc Object to copy.
 *
 *  @return This object.
 */
host_check& host_check::operator=(host_check const& hc) {
  check::operator=(hc);
  return (*this);
}

/**
 *  Get the type of this event.
 *
 *  @return The event type.
 */
unsigned int host_check::type() const {
  return (io::events::data_type<io::events::neb, neb::de_host_check>::value);
}
