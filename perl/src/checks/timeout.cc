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

#include "com/centreon/connector/perl/checks/check.hh"
#include "com/centreon/connector/perl/checks/timeout.hh"

using namespace com::centreon::connector::perl::checks;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] chk   Check that will be notified if timeout occurs.
 *  @param[in] final Is this the final timeout ?
 */
timeout::timeout(check* chk, bool final) : _check(chk), _final(final) {}

/**
 *  Copy constructor.
 *
 *  @param[in] t Object to copy.
 */
timeout::timeout(timeout const& t) : com::centreon::task(t) {
  _internal_copy(t);
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
    _internal_copy(t);
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
 *  Is this a final timeout ?
 *
 *  @return true if the timeout is final.
 */
bool timeout::is_final() const throw () {
  return (_final);
}

/**
 *  Notify check of timeout.
 */
void timeout::run() {
  if (_check)
    _check->on_timeout(_final);
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

/**
 *  Set whether this timeout is final.
 *
 *  @param[in] final New final parameter.
 */
void timeout::set_final(bool final) throw () {
  _final = final;
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
void timeout::_internal_copy(timeout const& t) {
  _check = t._check;
  _final = t._final;
  return ;
}
