/**
 * Copyright 2025 Centreon
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

#ifndef CCB_NEB_BBDO2_TO_BBDO3_HH
#define CCB_NEB_BBDO2_TO_BBDO3_HH

#include "com/centreon/broker/io/data.hh"

namespace com::centreon::broker::neb {
std::shared_ptr<io::data> bbdo2_to_bbdo3(const std::shared_ptr<io::data>& d);
}

#endif
