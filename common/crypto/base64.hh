/**
 * Copyright 2024 Centreon
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
#ifndef CCC_CRYPTO_BASE64_HH
#define CCC_CRYPTO_BASE64_HH
#include <string>

namespace com::centreon::common::crypto {

std::string base64_encode(const std::string_view& str);
std::string base64_decode(const std::string_view& ascdata);

}  // namespace com::centreon::common::crypto

#endif /* !CCC_CRYPTO_BASE64_HH */
