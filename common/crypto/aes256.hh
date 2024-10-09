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

#ifndef CCC_VAULT_AES256_HH
#define CCC_VAULT_AES256_HH
#include <string>

namespace com::centreon::common::crypto {

class aes256 {
  const std::string _first_key;
  const std::string _second_key;

  std::string _app_secret();

 public:
  aes256(const std::string& first_key, const std::string& second_key);
  aes256(const aes256&) = delete;
  aes256& operator=(const aes256&) = delete;
  std::string decrypt(const std::string& input);
  std::string encrypt(const std::string& input);
  void set_env_file(const std::string& env_file);
};
}  // namespace com::centreon::common::crypto

#endif /* !CCC_VAULT_AES256_HH */
