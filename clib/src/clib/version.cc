/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/clib/version.hh"

using namespace com::centreon::clib;

/**
 *  Get version major.
 *
 *  @return Centreon Clib version major.
 */
unsigned int version::get_major() throw() {
  return (major);
}

/**
 *  Get version minor.
 *
 *  @return Centreon Clib version minor.
 */
unsigned int version::get_minor() throw() {
  return (minor);
}

/**
 *  Get version patch.
 *
 *  @return Centreon Clib version patch.
 */
unsigned int version::get_patch() throw() {
  return (patch);
}

/**
 *  Get version string.
 *
 *  @return Centreon Clib version as string.
 */
char const* version::get_string() throw() {
  return (version::string);
}
