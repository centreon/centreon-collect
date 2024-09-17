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

#ifndef CCB_MISC_ENCODER_HH
#define CCB_MISC_ENCODER_HH
#include <filesystem>

namespace com::centreon::broker::misc {
class aes256_encoder {
  const std::string _first_key;
  const std::string _second_key;

  std::string _app_secret();

 public:
  aes256_encoder(const std::string& first_key, const std::string& second_key);
  aes256_encoder(const aes256_encoder&) = delete;
  aes256_encoder& operator=(const aes256_encoder&) = delete;
  std::string decrypt(const std::string& input);
  std::string encrypt(const std::string& input);
  void set_env_file(const std::string& env_file);
};
}  // namespace com::centreon::broker::misc

#endif /* !CCB_MISC_ENCODER_HH */
