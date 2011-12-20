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

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "com/centreon/connector/ssh/commander.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
commander::commander() {}

/**
 *  Destructor.
 */
commander::~commander() throw () {
  unreg();
}

/**
 *  Close callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::close(handle& h) {
  if (&h == &_so)
    throw (basic_error() << "received close request on output");
  else {
    logging::error(logging::high) << "received close request on input";
    logging::info(logging::high) << "sending termination request";
    kill(getpid(), SIGTERM);
  }
  return ;
}

/**
 *  Error callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::error(handle& h) {
  if (&h == &_so)
    throw (basic_error() << "received error on output");
  else {
    logging::error(logging::high) << "received error on input";
    logging::info(logging::high) << "sending termination request";
    kill(getpid(), SIGTERM);
  }
  return ;
}

/**
 *  Read callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::read(handle& h) {
  // XXX
}

/**
 *  Register commander with multiplexer.
 */
void commander::reg() {
  unreg();
  multiplexer::instance().handle_manager::add(&_si, this);
  multiplexer::instance().handle_manager::add(&_so, this);
  return ;
}

/**
 *  Unregister commander with multiplexer.
 *
 *  @param[in] all Set to true to remove both input and output. Set to
 *                 false to remove only input.
 */
void commander::unreg(bool all) {
  if (all)
    multiplexer::instance().handle_manager::remove(this);
  else
    multiplexer::instance().handle_manager::remove(&_si);
  return ;
}

/**
 *  Do we want to monitor handle for reading ?
 *
 *  @param[in] h Handle.
 */
bool commander::want_read(handle& h) {
  return (&h == &_si);
}

/**
 *  Do we want to monitor handle for writing ?
 *
 *  @param[in] h Handle.
 */
bool commander::want_write(handle& h) {
  // XXX
}

/**
 *  Write callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::write(handle& h) {
  // XXX
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] c Unused.
 */
commander::commander(commander const& c)
  : com::centreon::handle_listener() {
  (void)c;
  assert(!"commander cannot be copied");
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] c Unused.
 *
 *  @return This object.
 */
commander& commander::operator=(commander const& c) {
  (void)c;
  assert(!"commander cannot be copied");
  abort();
  return (*this);
}
