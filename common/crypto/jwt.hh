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

#ifndef CCC_CRYPTO_JWT_HH
#define CCC_CRYPTO_JWT_HH
#include <chrono>
#include <string>

namespace com::centreon::common::crypto {

class jwt {
  std::string _header;
  std::string _payload;
  std::string _signature;

  std::string _token;

  std::chrono::system_clock::time_point _exp;
  std::chrono::system_clock::time_point _iat;

  std::string _exp_str;

 public:
  jwt(const std::string& token);
  jwt(const jwt&) = delete;
  jwt& operator=(const jwt&) = delete;

  const std::string& get_header() const { return _header; }
  const std::string& get_payload() const { return _payload; }
  const std::string& get_signature() const { return _signature; }
  const std::string& get_string() const { return _token; }

  std::chrono::system_clock::time_point get_exp() const { return _exp; }
  std::chrono::system_clock::time_point get_iat() const { return _iat; }
  const std::string& get_exp_str() const { return _exp_str; }
};
}  // namespace com::centreon::common::crypto

#endif /* !CCC_CRYPTO_JWT_HH */
