/**
 * Copyright 2011-2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCE_MACROS_RESOLVE_HH
#define CCE_MACROS_RESOLVE_HH

#include "engine/macros/macro_context.hh"
#include "engine/macros/descriptors.hh"

namespace com::centreon::engine::macros {

std::string value(macro_context& mac,
                  std::string_view macro_name,
                  bool strip_illegal);
std::string table_value(macro_context& mac,
                        const descriptor& desc,
                        std::string_view arg1,
                        std::string_view arg2);

}  // namespace com::centreon::engine::macros

#endif  // !CCE_MACROS_RESOLVE_HH
