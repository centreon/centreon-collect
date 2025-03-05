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
#ifndef CCC_VAULT_VAULT_ACCESS_HH
#define CCC_VAULT_VAULT_ACCESS_HH
#include "com/centreon/common/http/http_client.hh"
#include "common/crypto/aes256.hh"

using com::centreon::common::crypto::aes256;
using com::centreon::common::http::client;

namespace com::centreon::common::vault {
class vault_access {
  /* The url and port to access to the Vault. */
  std::string _url;
  uint16_t _port;
  std::shared_ptr<spdlog::logger> _logger;

  std::string _root_path;

  /* The AES256 encrypt/decrypt tool to access the vault. */
  std::unique_ptr<aes256> _aes_encryptor;

  /* First key needed to use _aes_encryptor. */
  std::string _app_secret;
  /* Second key needed to use _aes_encryptor. */
  std::string _salt;

  /* The main credentials to access the Vault. */
  std::string _role_id;
  std::string _secret_id;

  /* The http client to the vault */
  std::shared_ptr<client> _client;

  /* The token to ask for a password */
  std::string _token;

  void _decrypt_role_and_secret();
  void _set_vault_informations(const std::string& vault_file);
  void _set_env_informations(const std::string& env_file);

 public:
  vault_access(const std::string& env_file,
               const std::string& vault_file,
               bool verify_peer,
               const std::shared_ptr<spdlog::logger>& logger);
  std::string decrypt(const std::string& encrypted);
};
}  // namespace com::centreon::common::vault
#endif /* !CCC_VAULT_VAULT_ACCESS_HH */
