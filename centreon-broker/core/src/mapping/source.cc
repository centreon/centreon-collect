/*
** Copyright 2011,2015 Merethis

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

#include "com/centreon/broker/mapping/source.hh"

using namespace com::centreon::broker::mapping;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
source::source() {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
source::source(source const& other) {
  (void)other;
}

/**
 *  Destructor.
 */
source::~source() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
source& source::operator=(source const& other) {
  (void)other;
  return (*this);
}
