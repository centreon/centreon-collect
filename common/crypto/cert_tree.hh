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

#ifndef CCC_CRYPTO_CERT_TREE_HH
#define CCC_CRYPTO_CERT_TREE_HH

namespace com::centreon::common::crypto {

/**
 * @brief type used to add field to certificate NAME
 * first element of pair should be a ssl NID value such as NID_commonName,
 * NID_countryName, NID_organizationName...
 *
 */
using name_entries = std::vector<std::pair<int /*nid*/, std::string /*value*/>>;

name_entries cn_to_name_entries(const std::string_view& cn);

class cert_tree {
  const X509* _ca;
  const EVP_PKEY* _ca_priv_key;

 public:
  /**
   * @brief the only goal of this struct is to have two constructors() with the
   * same signature, one with file paths, one with file contents
   *
   */
  struct load_from_str {};

  cert_tree(const X509* ca, const EVP_PKEY* ca_priv_key)
      : _ca(ca), _ca_priv_key(ca_priv_key) {}

  cert_tree(const std::string_view& ca_path,
            const std::string_view& priv_key_path,
            const std::string_view& key_password)
      : _ca(load_cert_from_file(ca_path)),
        _ca_priv_key(load_key_from_file(priv_key_path, key_password)) {}

  cert_tree(const std::string_view& ca_content,
            const std::string_view& priv_key_content,
            const std::string_view& key_password,
            const load_from_str&)
      : _ca(load_cert_from_string(ca_content)),
        _ca_priv_key(load_key_from_string(priv_key_content, key_password)) {}

  time_t get_ca_peremption() { return get_peremption(_ca); }

  const X509* get_ca() const { return _ca; }

  std::pair<X509* /*cert*/, EVP_PKEY* /*priv_key*/> generate_cert_key_pair(
      const name_entries& name_fields,
      unsigned minute_cert_ttl);

  static X509* load_cert_from_file(const std::string_view& path);

  static EVP_PKEY* load_key_from_file(const std::string_view& path,
                                      const std::string_view& password);

  static X509* load_cert_from_string(const std::string_view& content);

  static EVP_PKEY* load_key_from_string(const std::string_view& content,
                                        const std::string_view& password);

  static std::string cert_sha(X509* cert);

  static void cert_to_file(const X509* cert, const std::string_view& path);
  static void key_to_file(const EVP_PKEY* key,
                          const std::string_view& path,
                          const std::string_view& password);
  static std::string cert_to_string(const X509* cert);
  static std::string key_to_string(const EVP_PKEY* key);

  static std::pair<X509* /*cert*/, EVP_PKEY* /*priv_key*/>
  generate_self_signed_ca_key_pair(const name_entries& name_fields,
                                   unsigned minute_peremption);

  static EVP_PKEY* generate_ec_key();

  static X509* generate_cert(const EVP_PKEY* pkey,
                             const name_entries& name_fields,
                             unsigned minute_cert_ttl,
                             unsigned version,
                             const EVP_PKEY* ca_key,
                             const X509* ca_cert);

  static bool is_self_signed(const X509* cert);
  static bool is_self_signed(const std::string_view& cert_content);

  static name_entries get_name_fields(const X509* cert);

  static void set_name_fields(X509* cert, const name_entries& name_fields);

  static time_t get_peremption(const X509* cert);
};

}  // namespace com::centreon::common::crypto

#endif
