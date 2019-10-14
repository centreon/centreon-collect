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

#include "com/centreon/handle_listener.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 */
handle_listener::handle_listener() {}

/**
 *  Default destructor.
 */
handle_listener::~handle_listener() throw() {}

/**
 *  Read action on a specific handle.
 *
 *  @param[in] h  The handle notify by a read.
 */
void handle_listener::read(handle& h) {
  // Basic implementation, discard all data.
  char buf[4096];
  h.read(buf, sizeof(buf));
}

/**
 *  Define if the listener was notify for the read action.
 *
 *  @param[in] h  The handle to define to be read.
 *
 *  @return Always return false.
 */
bool handle_listener::want_read(handle& h) {
  (void)h;
  return (false);
}

/**
 *  Define if the listener was notify for the write action.
 *
 *  @param[in] h  The handle to define to be write.
 *
 *  @return Always return false.
 */
bool handle_listener::want_write(handle& h) {
  (void)h;
  return (false);
}

/**
 *  Write action on a specific handle.
 *
 *  @param[in] h  The handle notify by a write.
 */
void handle_listener::write(handle& h) {
  (void)h;
  // Basic implementation, do nothing.
}
