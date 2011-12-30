/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/checks/timeout.hh"

using namespace com::centreon::connector::ssh::checks;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] chk Check that will be notified if timeout occurs.
 */
timeout::timeout(check* chk) : _check(chk) {}

/**
 *  Copy constructor.
 *
 *  @param[in] t Object to copy.
 */
timeout::timeout(timeout const& t) : com::centreon::task(t) {
  _copy(t);
}

/**
 *  Destructor.
 */
timeout::~timeout() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] t Object to copy.
 *
 *  @return This object.
 */
timeout& timeout::operator=(timeout const& t) {
  if (this != &t) {
    com::centreon::task::operator=(t);
    _copy(t);
  }
  return (*this);
}

/**
 *  Get the check object.
 *
 *  @return Check object.
 */
check* timeout::get_check() const throw () {
  return (_check);
}

/**
 *  Notify check of timeout.
 */
void timeout::run() {
  if (_check)
    _check->on_timeout();
  return ;
}

/**
 *  Set target check.
 *
 *  @param[in] chk Target check.
 */
void timeout::set_check(check* chk) throw () {
  _check = chk;
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] t Object to copy.
 */
void timeout::_copy(timeout const& t) {
  _check = t._check;
  return ;
}
