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

#include <iostream>
#include <stdio.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_listener.hh"
#include "com/centreon/handle_manager.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  @class listener
 *  @brief litle implementation of handle listener to test the
 *         handle manager.
 */
class     listener : public handle_listener {
public:
          listener() {}
          ~listener() throw () {}
  void    error(handle& h) { (void)h; }
};

/**
 *  Try to insert a null pointer handle argument.
 *
 *  @return True on sucess, otherwise false.
 */
static bool null_handle() {
  try {
    handle_manager hm;
    listener l;
    hm.add(NULL, &l);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Try to insert a null pointer listener argument.
 *
 *  @return True on sucess, otherwise false.
 */
static bool null_listener() {
  try {
    handle_manager hm;
    io::file_stream fs;
    hm.add(&fs, NULL);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check basic insert into handle manager.
 *
 *  @return True on sucess, otherwise false.
 */
static bool basic_add() {
  try {
    handle_manager hm;

    io::file_stream fs(stdin);
    listener l;
    hm.add(&fs, &l);
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);

}

/**
 *  Check multiple insert into handle manager.
 *
 *  @return True on sucess, otherwise false.
 */
static bool double_add() {
  try {
    handle_manager hm;

    io::file_stream fs(stdin);
    listener l;
    hm.add(&fs, &l);
    try {
      hm.add(&fs, &l);
    }
    catch (std::exception const& e) {
      (void)e;
      return (true);
    }
  }
  catch (std::exception const& e) {
    (void)e;
  }
  return (false);
}

/**
 *  Check the handle manager add.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!null_handle())
      throw (basic_error() << "invalid handler is set");
    if (!null_listener())
      throw (basic_error() << "invalid listener is set");
    if (!basic_add())
      throw (basic_error() << "add failed");
    if (!double_add())
      throw (basic_error() << "double add failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
