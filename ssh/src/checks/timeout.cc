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
//timeout::timeout(timeout const& t) : com::centreon::task(t) {
//  _internal_copy(t);
//}

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
//timeout& timeout::operator=(timeout const& t) {
//  if (this != &t) {
//    com::centreon::task::operator=(t);
//    _internal_copy(t);
//  }
//  return (*this);
//}

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
void timeout::_internal_copy(timeout const& t) {
  _check = t._check;
  return ;
}
