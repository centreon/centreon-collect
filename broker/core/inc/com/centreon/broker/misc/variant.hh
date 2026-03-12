/*
 * Copyright 2019 Centreon
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

#ifndef CCB_MISC_VARIANT_HH_
#define CCB_MISC_VARIANT_HH_

#include <cstdint>
#include <string>
#include <variant>

namespace com::centreon::broker::misc {

/**
 * @brief Type-safe union replacing the old hand-rolled variant class.
 *
 * Holds one of: monostate (empty), bool, int32_t, uint32_t, int64_t,
 * uint64_t, double, or std::string. Constructors are provided implicitly
 * by std::variant's converting constructor.
 */
using variant = std::variant<std::monostate,
                             bool,
                             int32_t,
                             uint32_t,
                             int64_t,
                             uint64_t,
                             double,
                             std::string>;

}  // namespace com::centreon::broker::misc

#endif /* !CCB_MISC_VARIANT_HH */
