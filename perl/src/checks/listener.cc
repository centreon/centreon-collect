/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/perl/checks/listener.hh"

using namespace com::centreon::connector::perl::checks;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
listener::listener() {}

/**
 *  Copy constructor.
 *
 *  @param[in] l Unused.
 */
listener::listener(listener const& l) {
  (void)l;
}

/**
 *  Destructor.
 */
listener::~listener() {}

/**
 *  Assignment operator.
 *
 *  @param[in] l Unused.
 *
 *  @return This object.
 */
listener& listener::operator=(listener const& l) {
  (void)l;
  return (*this);
}
