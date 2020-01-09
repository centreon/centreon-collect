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

#include "com/centreon/connector/perl/checks/timeout.hh"
#include "com/centreon/connector/perl/checks/check.hh"

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
 *  Get the check object.
 *
 *  @return Check object.
 */
check* timeout::get_check() const throw() {
  return _check;
}

/**
 *  Is this a final timeout ?
 *
 *  @return true if the timeout is final.
 */
bool timeout::is_final() const throw() {
  return _final;
}

/**
 *  Notify check of timeout.
 */
void timeout::run() {
  if (_check)
    _check->on_timeout(_final);
}

/**
 *  Set target check.
 *
 *  @param[in] chk Target check.
 */
void timeout::set_check(check* chk) throw() {
  _check = chk;
}

/**
 *  Set whether this timeout is final.
 *
 *  @param[in] final New final parameter.
 */
void timeout::set_final(bool final) throw() {
  _final = final;
}
