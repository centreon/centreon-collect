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

int export_lib_version = 42;
char const* export_lib_name = "shared_testing_library";

/**
 *  Addition function.
 *
 *  @param[in] i1  first integer.
 *  @param[in] i2  second integer.
 *
 *  @return i1 + i2.
 */
extern "C" int add(int i1, int i2) {
  return (i1 + i2);
}
