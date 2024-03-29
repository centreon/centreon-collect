/**
* Copyright 2021 Centreon
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

#include "bbdo/bbdo/stop.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bbdo;

/**
 *  Default constructor.
 */
stop::stop() : io::data(stop::static_type()) {}

// Mapping.
mapping::entry const stop::entries[]{mapping::entry()};

// Operations.
static io::data* new_stop() {
  return new stop;
}
io::event_info::event_operations const stop::operations = {&new_stop, nullptr,
                                                           nullptr};
