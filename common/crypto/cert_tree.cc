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

#include "cert_tree.hh"
#include <openssl/pem.h>
#include <sys/stat.h>
#include <cstring>
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::common::crypto {
class ssl_exception : public std::runtime_error {
  static int _append_ssl_error_to_str(const char* str, size_t len, void* out) {
    std::string* sz_out = reinterpret_cast<std::string*>(out);
    sz_out->append(str, len);
    sz_out->push_back('\n');
    return 1;
  }

  static std::string _message_from_error(const std::string_view& message) {
    std::string ret(message);
    ret.push_back(' ');
    ERR_print_errors_cb(_append_ssl_error_to_str, &ret);
    return ret;
  }

 public:
  ssl_exception(const std::string_view& message)
      : std::runtime_error(_message_from_error(message)) {}
};
}  // namespace com::centreon::common::crypto

using namespace com::centreon::common::crypto;

X509* cert_tree::load_cert_from_file(const std::string_view& path) {
  FILE* fp = fopen(path.data(), "r");
  if (!fp) {
    throw exceptions::msg_fmt("fail to read certificate {}: {}", path,
                              strerror(errno));
  }
  X509* ret = PEM_read_X509(fp, nullptr, nullptr, nullptr);
  if (!ret) {
    fclose(fp);
    using namespace std::literals;
    throw ssl_exception("fail to read cerficate "s + path.data() + " :");
  }
  fclose(fp);
  return ret;
}

EVP_PKEY* cert_tree::load_key_from_file(const std::string_view& path,
                                        const std::string_view& key_password) {
  FILE* fp = fopen(path.data(), "r");
  if (!fp) {
    throw exceptions::msg_fmt("fail to read private key {}: {}", path),
        strerror(errno);
  }

  EVP_PKEY* ret = PEM_read_PrivateKey(
      fp, nullptr, nullptr,
      (void*)(key_password.empty() ? nullptr : key_password.data()));
  if (!ret) {
    fclose(fp);
    using namespace std::literals;
    throw ssl_exception("fail to read private key "s + path.data() + " :");
  }
  fclose(fp);
  return ret;
}

void cert_tree::cert_to_file(X509* cert, const std::string_view& path) {
  FILE* fp = fopen(path.data(), "w");
  if (!fp) {
    throw exceptions::msg_fmt("fail to open certificate file {}: {}", path,
                              strerror(errno));
  }
  int ret = PEM_write_X509(fp, cert);
  if (!ret) {
    fclose(fp);
    ::remove(path.data());
    using namespace std::literals;
    throw ssl_exception("fail to write cerficate "s + path.data() + " :");
  }
  fclose(fp);
}

void cert_tree::key_to_file(EVP_PKEY* key,
                            const std::string_view& path,
                            const std::string_view& password) {
  FILE* fp = fopen(path.data(), "w");
  if (!fp) {
    throw exceptions::msg_fmt("fail to open key file {}: {}", path,
                              strerror(errno));
  }
  int ret = PEM_write_PrivateKey(
      fp, key, EVP_aes_256_cbc(), nullptr, 0, nullptr,
      (void*)(password.empty() ? nullptr : password.data()));
  if (!ret) {
    fclose(fp);
    ::remove(path.data());
    using namespace std::literals;
    throw ssl_exception("fail to write cerficate "s + path.data() + " :");
  }
  fclose(fp);
  ::chmod(path.data(), 0600);
}

static void evp_key_ctx_free(EVP_PKEY_CTX* to_delete) {
  if (to_delete)
    EVP_PKEY_CTX_free(to_delete);
}

EVP_PKEY* cert_tree::generate_ec_key() {
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL), EVP_PKEY_CTX_free);

  if (!ctx) {
    throw ssl_exception("fail to allocate key ctx:");
  }

  if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
    throw ssl_exception("fail to init key ctx:");
  }

  if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1) <=
      0) {
    throw ssl_exception("fail to init key ctx:");
  }

  EVP_PKEY* ret = nullptr;

  if (EVP_PKEY_keygen(ctx.get(), &ret) <= 0) {
    throw ssl_exception("fail to generate key:");
  }
  return ret;
}

X509* cert_tree::generate_cert(EVP_PKEY* pkey,
                               const std::string_view& cn,
                               unsigned day_peremption,
                               unsigned version,
                               EVP_PKEY* ca_key,
                               X509* ca_cert) {
  X509* x509 = X509_new();
  X509_NAME* name;

  X509_set_version(x509, version);  // 0 = v1, 1 = v2, 2 = v3
  ASN1_INTEGER_set(X509_get_serialNumber(x509), (long)time(NULL));
  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), day_peremption * 24 * 3600);
  X509_set_pubkey(x509, pkey);

  // subject
  name = X509_get_subject_name(x509);
  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"FR", -1,
                             -1, 0);
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                             (unsigned char*)"centreon", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (unsigned char*)cn.data(), -1, -1, 0);

  // Issuer
  if (ca_cert) {
    X509_set_issuer_name(x509, X509_get_subject_name(ca_cert));
  } else {
    X509_set_issuer_name(x509, name);  // auto-signé
  }

  // Signature
  if (!X509_sign(x509, ca_key ? ca_key : pkey, EVP_sha256())) {
    throw ssl_exception("fail to sign certificate:");
  }

  return x509;
}

std::pair<X509* /*cert*/, EVP_PKEY* /*priv_key*/>
cert_tree::generate_self_signed_ca_key_pair(const std::string_view& cn,
                                            unsigned day_peremption) {
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> ca_key(generate_ec_key(),
                                                             EVP_PKEY_free);

  X509* ca = generate_cert(ca_key.get(), cn, day_peremption, 1 /*v2*/, nullptr,
                           nullptr);
  return std::make_pair(ca, ca_key.release());
}
