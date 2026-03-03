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
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "base64.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::common::crypto {
/**
 * @brief exception with ssl error string
 *
 */
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

namespace {
static void copy_subject_alt_name_extensions(const X509* source, X509* target) {
  if (!source || !target) {
    return;
  }

  for (int ext_index = X509_get_ext_by_NID(source, NID_subject_alt_name, -1);
       ext_index >= 0; ext_index = X509_get_ext_by_NID(
                           source, NID_subject_alt_name, ext_index)) {
    X509_EXTENSION* source_ext = X509_get_ext(source, ext_index);
    if (!source_ext) {
      throw com::centreon::exceptions::msg_fmt(
          "unable to read source subjectAltName extension");
    }

    X509_EXTENSION* copied_ext = X509_EXTENSION_dup(source_ext);
    if (!copied_ext) {
      throw com::centreon::exceptions::msg_fmt(
          "unable to duplicate source subjectAltName extension");
    }

    const int add_status = X509_add_ext(target, copied_ext, -1);
    X509_EXTENSION_free(copied_ext);
    if (add_status != 1) {
      throw com::centreon::exceptions::msg_fmt(
          "unable to add subjectAltName extension to generated certificate");
    }
  }
}
}  // namespace

/**
 * @brief load a certificate from pem format file
 *
 * @param path file path
 * @return X509* certificate to free
 * @throw ssl_exception, msg_fmt
 */
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

/**
 * @brief load a key (encrypted or not) from a pem format file
 * @note If you try to load a not encrypted key file with a password, it works
 * despite
 * @param path file path
 * @param key_password password used to encrypt file (empty if no encryption)
 * @return EVP_PKEY*
 * @throw ssl_exception, msg_fmt
 */
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

/**
 * @brief load a certificate from pem format string
 *
 * @param content file content
 * @return X509* certificate to free
 * @throw ssl_exception
 */
X509* cert_tree::load_cert_from_string(const std::string_view& content) {
  BIO* bio = BIO_new_mem_buf(content.data(), content.length());
  X509* ret = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (!ret) {
    throw ssl_exception("fail to load certificate: ");
  }
  return ret;
}

/**
 * @brief load a key (encrypted or not) from a pem format string
 *
 * @param content file content
 * @param key_password password used to encrypt file (empty if no encryption)
 * @return EVP_PKEY*
 * @throw ssl_exception
 */
EVP_PKEY* cert_tree::load_key_from_string(const std::string_view& content,
                                          const std::string_view& password) {
  BIO* bio = BIO_new_mem_buf(content.data(), content.length());
  EVP_PKEY* ret = PEM_read_bio_PrivateKey(
      bio, nullptr, nullptr,
      password.empty() ? nullptr : (void*)password.data());
  BIO_free(bio);
  if (!ret) {
    throw ssl_exception("fail to load private key: ");
  }
  return ret;
}

/**
 * @brief calculate hash of a certificate
 *
 * @param cert
 * @return std::string base64 hash
 * @throw ssl_exception
 */
std::string cert_tree::cert_sha(X509* cert) {
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  if (!X509_digest(cert, EVP_sha256(), md, &md_len)) {
    throw ssl_exception("fail to calculate footprint of certificate: ");
  }

  return base64_encode(
      std::string_view(reinterpret_cast<const char*>(md), md_len));
}

/**
 * @brief save a certificate to a file
 *
 * @param cert
 * @param path
 * @throw ssl_exception, msg_fmt
 */
void cert_tree::cert_to_file(const X509* cert, const std::string_view& path) {
  FILE* fp = fopen(path.data(), "w");
  if (!fp) {
    throw exceptions::msg_fmt("fail to open certificate file {}: {}", path,
                              strerror(errno));
  }
  int ret = PEM_write_X509(fp, const_cast<X509*>(cert));
  if (!ret) {
    fclose(fp);
    ::remove(path.data());
    using namespace std::literals;
    throw ssl_exception("fail to write cerficate "s + path.data() + " :");
  }
  fclose(fp);
}

/**
 * @brief save key to a file
 *
 * @param key
 * @param path file path
 * @param password if empty, no encryption of the key
 * @throw ssl_exception, msg_fmt
 */
void cert_tree::key_to_file(const EVP_PKEY* key,
                            const std::string_view& path,
                            const std::string_view& password) {
  FILE* fp = fopen(path.data(), "w");
  if (!fp) {
    throw exceptions::msg_fmt("fail to open key file {}: {}", path,
                              strerror(errno));
  }

  int ret;
  if (!password.empty()) {
    ret =
        PEM_write_PrivateKey(fp, const_cast<EVP_PKEY*>(key), EVP_aes_256_cbc(),
                             nullptr, 0, nullptr, (void*)password.data());
  } else {
    ret = PEM_write_PrivateKey(fp, const_cast<EVP_PKEY*>(key), nullptr, nullptr,
                               0, nullptr, nullptr);
  }
  if (!ret) {
    fclose(fp);
    ::remove(path.data());
    using namespace std::literals;
    throw ssl_exception("fail to write cerficate "s + path.data() + " :");
  }
  fclose(fp);
  ::chmod(path.data(), 0600);
}

/**
 * @brief generate a prime256v1 key
 *
 * @return EVP_PKEY*
 * @throw ssl_exception
 */
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

/**
 * @brief generate a certificate
 * if ca_cert is provided, returned cert is certified by ca, else is self signed
 * @param pkey key of certificate
 * @param cn CN recorded in certificate
 * @param minute_cert_ttl time to live
 * @param version version usually 1, 0 = v1, 1 = v2, 2 = v3
 * @param ca_key key of the  ca
 * @param ca_cert ca
 * @param use_ca_cert_cn if true, cn param is ignored and generated certificate
 * will have same cn as ca_cert
 * @return X509*
 */
X509* cert_tree::generate_cert(const EVP_PKEY* pkey,
                               const name_entries& name_fields,
                               unsigned minute_cert_ttl,
                               unsigned version,
                               const EVP_PKEY* ca_key,
                               const X509* ca_cert,
                               const X509* san_source_cert) {
  X509* x509 = X509_new();
  X509_NAME* name;

  X509_set_version(x509, version);  // 0 = v1, 1 = v2, 2 = v3
  ASN1_INTEGER_set(X509_get_serialNumber(x509), (long)time(NULL));
  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), minute_cert_ttl * 60);
  X509_set_pubkey(x509, const_cast<EVP_PKEY*>(pkey));

  // subject
  name = X509_get_subject_name(x509);

  set_name_fields(x509, name_fields);

  // Issuer
  if (ca_cert) {
    X509_set_issuer_name(x509, X509_get_subject_name(ca_cert));
  } else {
    X509_set_issuer_name(x509, name);  // auto-signé
    // Extension : Basic Constraints = CA:TRUE
    X509V3_CTX ctx;
    X509V3_set_ctx(&ctx, x509, x509, NULL, NULL, 0);
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints,
                                              "critical,CA:TRUE");
    X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);
  }
  copy_subject_alt_name_extensions(san_source_cert, x509);

  // Signature
  if (!X509_sign(x509, const_cast<EVP_PKEY*>(ca_key ? ca_key : pkey),
                 EVP_sha256())) {
    throw ssl_exception("fail to sign certificate:");
  }

  return x509;
}

/**
 * @brief generate a self signed cert with his prime256v1 key
 *
 * @param cn
 * @param minute_cert_ttl tie to leave
 * @return std::pair<X509* , EVP_PKEY* >
 */
std::pair<X509* /*cert*/, EVP_PKEY* /*priv_key*/>
cert_tree::generate_self_signed_ca_key_pair(const name_entries& name_fields,
                                            unsigned minute_cert_ttl) {
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> ca_key(generate_ec_key(),
                                                             EVP_PKEY_free);

  X509* ca = generate_cert(ca_key.get(), name_fields, minute_cert_ttl, 1 /*v2*/,
                           nullptr, nullptr);
  return std::make_pair(ca, ca_key.release());
}

/**
 * @brief generate a cert with his prime256v1 key certified by _ca
 *
 * @param cn cn of generated certificate
 * @param minute_cert_ttl
 * @return std::pair<X509* , EVP_PKEY* >
 */
std::pair<X509* /*cert*/, EVP_PKEY* /*priv_key*/>
cert_tree::generate_cert_key_pair(const name_entries& name_fields,
                                  unsigned minute_cert_ttl,
                                  const X509* san_source_cert) {
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> priv_key(
      generate_ec_key(), EVP_PKEY_free);

  X509* cert = generate_cert(priv_key.get(), name_fields, minute_cert_ttl,
                             1 /*v2*/, _ca_priv_key, _ca, san_source_cert);
  return std::make_pair(cert, priv_key.release());
}

/**
 * @brief test if cert is a self signed certificate
 *
 * @param cert
 * @return true cert is self signed
 * @return false
 */
bool cert_tree::is_self_signed(const X509* cert) {
  // 1️⃣ Subject == Issuer ?
  if (X509_NAME_cmp(X509_get_subject_name(cert), X509_get_issuer_name(cert)) !=
      0)
    return false;

  // 2️⃣ self signed with his pub key ?
  EVP_PKEY* pkey = X509_get_pubkey(const_cast<X509*>(cert));
  if (!pkey)
    return false;

  int ret = X509_verify(const_cast<X509*>(cert), pkey);
  EVP_PKEY_free(pkey);

  return ret == 1;
}

/**
 * @brief test if cert is a self signed certificate
 *
 * @param cert_content
 * @return true cert is self signed
 * @return false
 */
bool cert_tree::is_self_signed(const std::string_view& cert_content) {
  X509* cert = load_cert_from_string(cert_content);
  if (!cert) {
    return false;
  }
  bool ret = is_self_signed(cert);
  X509_free(cert);
  return ret;
}

/**
 * @brief output cert content to a string
 *
 * @param cert
 * @return std::string
 */
std::string cert_tree::cert_to_string(const X509* cert) {
  BIO* bio = BIO_new(BIO_s_mem());

  PEM_write_bio_X509(bio, const_cast<X509*>(cert));

  BUF_MEM* mem;
  BIO_get_mem_ptr(bio, &mem);

  std::string pem(mem->data, mem->length);

  BIO_free(bio);
  return pem;
}

/**
 * @brief output a key to a string
 *
 * @param key
 * @return std::string
 */
std::string cert_tree::key_to_string(const EVP_PKEY* key) {
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio)
    return {};

  PEM_write_bio_PrivateKey(bio, const_cast<EVP_PKEY*>(key),
                           nullptr,  // no encryption
                           nullptr, 0, nullptr, nullptr);

  BUF_MEM* mem;
  BIO_get_mem_ptr(bio, &mem);

  std::string pem(mem->data, mem->length);

  BIO_free(bio);
  return pem;
}

/**
 * @brief get name fields of certificate
 *
 * @param cert
 * @return name_entries
 * @throw invalid_argument cert has no cn
 */
name_entries cert_tree::get_name_fields(const X509* cert) {
  const X509_NAME* subject = X509_get_subject_name(cert);
  int field_count = X509_NAME_entry_count(subject);

  name_entries ret;

  for (int field_index = 0; field_index < field_count; ++field_index) {
    const X509_NAME_ENTRY* field = X509_NAME_get_entry(subject, field_index);
    const ASN1_OBJECT* obj = X509_NAME_ENTRY_get_object(field);
    int nid = OBJ_obj2nid(obj);
    const ASN1_STRING* data = X509_NAME_ENTRY_get_data(field);
    unsigned char* utf8 = NULL;
    int len = ASN1_STRING_to_UTF8(&utf8, data);
    if (len >= 0) {
      ret.emplace_back(
          std::make_pair(nid, reinterpret_cast<const char*>(utf8)));
      OPENSSL_free(utf8);
    }
  }
  return ret;
}

/**
 * @brief set fields of name of a certificate
 *
 * @param cert out: certificate where to put name_fields content
 * @param name_fields in: fields to set in cert
 */
void cert_tree::set_name_fields(X509* cert, const name_entries& name_fields) {
  X509_NAME* subject = X509_get_subject_name(cert);
  for (const auto& field : name_fields) {
    X509_NAME_add_entry_by_NID(
        subject, field.first, MBSTRING_UTF8,
        reinterpret_cast<const unsigned char*>(field.second.c_str()),
        field.second.size(), -1, 0);
  }
}

/**
 * @brief get peremption certificate
 *
 * @param cert
 * @return time_t
 * @throw invalid_argument cert not after is an invalid date time
 */
time_t cert_tree::get_peremption(const X509* cert) {
  // conversion
  struct tm tm;
  const ASN1_TIME* perempt = X509_get0_notAfter(cert);
  if (ASN1_TIME_to_tm(perempt, &tm) != 1) {
    throw std::invalid_argument("not after certificate field is invalid");
  }

#if defined(_WIN32)
  return _mkgmtime(&tm);  // Windows
#else
  return timegm(&tm);  // POSIX
#endif
}

/**
 * @brief simple helper to use when we have only to add CN to name
 *
 * @param cn
 * @return name_entries
 */
name_entries com::centreon::common::crypto::cn_to_name_entries(
    const std::string_view& cn) {
  return {{NID_commonName, std::string(cn)}};
}
