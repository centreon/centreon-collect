/**
 * Copyright 2025 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
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

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <openssl/x509.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <tuple>

#include "common/tests/grpc/grpc_test.grpc.pb.h"

#include "com/centreon/common/grpc/grpc_client.hh"
#include "com/centreon/common/grpc/grpc_server.hh"
#include "grpc/grpc_test.pb.h"

extern std::shared_ptr<asio::io_context> g_io_context;

class grpc_test : public ::testing::Test {
 protected:
  static std::string _key;
  static std::string _crt;

  static void to_disk(const std::string_view& path,
                      const std::string_view& content) {
    ::remove(path.data());
    std::ofstream f(path.data());
    f << content;
  }

  static std::string from_disk(const std::string_view& path) {
    std::ifstream file(path.data());
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    return ss.str();
  }

  static EVP_PKEY* generate_key() {
    EVP_PKEY* key = EVP_PKEY_new();
    BIGNUM* e = BN_new();
    BN_set_word(e, RSA_F4);  // 65537
    RSA* rsa = RSA_new();
    assert(RSA_generate_key_ex(rsa, 4096, e, NULL));
    EVP_PKEY_assign_RSA(key, rsa);  // Transfère la propriété à pkey
    BN_free(e);
    return key;
  }

  static std::string key_to_str(EVP_PKEY* pkey) {
    BIO* key_bio = BIO_new(BIO_s_mem());
    assert(PEM_write_bio_PrivateKey(key_bio, pkey, NULL, NULL, 0, NULL, NULL));
    BUF_MEM* key_buf;
    BIO_get_mem_ptr(key_bio, &key_buf);
    std::string key(key_buf->data, key_buf->length);
    BIO_free(key_bio);
    return key;
  }

  static std::string crt_to_str(X509* cert) {
    BIO* cert_bio = BIO_new(BIO_s_mem());

    assert(PEM_write_bio_X509(cert_bio, cert));

    // Extraire les données en mémoire
    BUF_MEM* cert_buf;
    BIO_get_mem_ptr(cert_bio, &cert_buf);

    std::string crt(cert_buf->data, cert_buf->length);

    BIO_free(cert_bio);
    return crt;
  }

  static std::string csr_to_str(X509_REQ* cert) {
    BIO* cert_bio = BIO_new(BIO_s_mem());

    assert(PEM_write_bio_X509_REQ(cert_bio, cert));

    // Extraire les données en mémoire
    BUF_MEM* cert_buf;
    BIO_get_mem_ptr(cert_bio, &cert_buf);

    std::string crt(cert_buf->data, cert_buf->length);

    BIO_free(cert_bio);
    return crt;
  }

  static X509* generate_cert(EVP_PKEY* pkey,
                             const char* CN,
                             int days,
                             int version,
                             EVP_PKEY* sign_key,
                             X509* issuer_cert) {
    X509* x509 = X509_new();
    X509_NAME* name;

    X509_set_version(x509, version);  // 0 = v1, 1 = v2, 2 = v3
    ASN1_INTEGER_set(X509_get_serialNumber(x509), (long)time(NULL));
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), days * 24 * 3600);
    X509_set_pubkey(x509, pkey);

    // Sujet
    name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"FR",
                               -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               (unsigned char*)"centreon", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)CN, -1,
                               -1, 0);

    // Issuer
    if (issuer_cert) {
      X509_set_issuer_name(x509, X509_get_subject_name(issuer_cert));
    } else {
      X509_set_issuer_name(x509, name);  // auto-signé
    }

    // Signature
    if (!X509_sign(x509, sign_key ? sign_key : pkey, EVP_sha256())) {
      fprintf(stderr, "Erreur signature certificat\n");
      return NULL;
    }

    return x509;
  }

  static std::tuple<std::string, std::string, std::string>
  generate_key_cert_with_ca_v2(unsigned days) {
    EVP_PKEY* ca_key = generate_key();
    X509* ca_cert = generate_cert(ca_key, "RootCA", 365, 1, nullptr, nullptr);
    EVP_PKEY* user_key = generate_key();
    X509* user_crt =
        generate_cert(user_key, "localhost", 1, 1, ca_key, ca_cert);
    std::string sz_ca_crt = crt_to_str(ca_cert);
    std::string sz_user_crt = crt_to_str(user_crt);
    std::string sz_user_key = key_to_str(user_key);
    FILE* f = fopen("ca_key.pem", "wb");
    PEM_write_PrivateKey(f, ca_key, NULL, NULL, 0, NULL, NULL);
    fclose(f);
    f = fopen("ca_cert.pem", "wb");
    PEM_write_X509(f, ca_cert);
    fclose(f);

    f = fopen("user_key.pem", "wb");
    PEM_write_PrivateKey(f, user_key, NULL, NULL, 0, NULL, NULL);
    fclose(f);

    f = fopen("user_cert.pem", "wb");
    PEM_write_X509(f, user_crt);
    fclose(f);

    return std::make_tuple(sz_user_key, sz_user_crt, sz_ca_crt);
  }

  // 📜 Crée un certificat auto-signé pour la CA
  static X509* generate_ca_cert(EVP_PKEY* ca_pkey,
                                const std::string_view& cn,
                                unsigned second_peremption) {
    X509* x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 0);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), second_peremption);
    X509_set_pubkey(x509, ca_pkey);

    // Sujet et émetteur identiques (auto-signé)
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"FR",
                               -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               (unsigned char*)"centreon", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (unsigned char*)cn.data(), -1, -1, 0);
    X509_set_issuer_name(x509, name);

    X509_set_version(x509, 0);

    // Extension : Basic Constraints = CA:TRUE
    X509V3_CTX ctx;
    X509V3_set_ctx(&ctx, x509, x509, NULL, NULL, 0);
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints,
                                              "critical,CA:TRUE");
    X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);

    X509_sign(x509, ca_pkey, EVP_sha256());
    return x509;
  }

  // 🧾 Crée un certificat client signé par la CA
  static X509* generate_client_cert(EVP_PKEY* client_pkey,
                                    X509* ca_cert,
                                    EVP_PKEY* ca_pkey,
                                    unsigned second_peremption) {
    // creation requete certificat(CSR)
    X509_REQ* req = X509_REQ_new();
    X509_REQ_set_version(req, 0);

    // Nom du sujet
    X509_NAME* name = X509_NAME_new();
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"FR",
                               -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               (unsigned char*)"centreon", -1, -1, 0);
    assert(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                      (unsigned char*)"localhost", -1, -1, 0));
    assert(X509_REQ_set_subject_name(req, name));
    X509_NAME_free(name);

    // Clé publique
    assert(X509_REQ_set_pubkey(req, client_pkey));

    // Signer la CSR avec la clé privée
    assert(X509_REQ_sign(req, client_pkey, EVP_sha256()));

    to_disk("test.csr", csr_to_str(req));

    X509* cert = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 0);
    X509_set_version(cert, 0);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), second_peremption);

    // Nom du sujet = celui de la CSR
    X509_set_subject_name(cert, X509_REQ_get_subject_name(req));
    // Nom de l'émetteur = celui de la CA
    X509_set_issuer_name(cert, X509_get_subject_name(ca_cert));
    // Clé publique = celle de la CSR
    EVP_PKEY* req_pubkey = X509_REQ_get_pubkey(req);
    X509_set_pubkey(cert, req_pubkey);
    EVP_PKEY_free(req_pubkey);

    // Extensions basiques (ex: BasicConstraints)
    X509V3_CTX ctx;
    X509V3_set_ctx(&ctx, ca_cert, cert, NULL, NULL, 0);

    X509_EXTENSION* ext;
    ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "CA:FALSE");
    X509_add_ext(cert, ext, -1);
    X509_EXTENSION_free(ext);

    // Signer le certificat avec la clé privée de la CA
    X509_sign(cert, ca_pkey, EVP_sha256());

    to_disk("client.crt", crt_to_str(cert));

    return cert;
  }

 public:
  static void SetUpTestSuite() {
    std::tie(_key, _crt) = generate_key_cert(3600);
  }

  static std::pair<std::string /*key*/, std::string /*cert*/> generate_key_cert(
      unsigned second_peremption) {
    EVP_PKEY* pkey = generate_key();

    X509* cert = generate_ca_cert(pkey, "localhost", second_peremption);

    std::string key = key_to_str(pkey);
    std::string crt = crt_to_str(cert);
    EVP_PKEY_free(pkey);
    X509_free(cert);

    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
    return std::make_pair(key, crt);
  }

  static std::
      tuple<std::string /*key*/, std::string /*cert*/, std::string /*ca_cert*/>
      generate_key_cert_with_ca_v1(unsigned second_peremption) {
    EVP_PKEY* ca_key = generate_key();

    X509* ca_cert =
        generate_ca_cert(ca_key, "root ca", 86400 * 365 * 10);  // 10 years

    to_disk("ca.crt", crt_to_str(ca_cert));

    EVP_PKEY* client_key = generate_key();
    X509* client_cert =
        generate_client_cert(client_key, ca_cert, ca_key, second_peremption);

    std::string key = key_to_str(client_key);
    std::string crt = crt_to_str(client_cert);
    std::string ca_crt = crt_to_str(ca_cert);
    EVP_PKEY_free(ca_key);
    EVP_PKEY_free(client_key);
    X509_free(ca_cert);
    X509_free(client_cert);

    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
    return std::make_tuple(key, crt, ca_crt);
  }

  static std::
      tuple<std::string /*key*/, std::string /*cert*/, std::string /*ca_cert*/>
      load_key_cert_with_ca(unsigned second_peremption) {
    std::string key = from_disk(
        "/data/dev/centreon-collect-bis/broker/grpc/test/grpc_test_keys/"
        "server_1234.key");
    std::string crt = from_disk(
        "/data/dev/centreon-collect-bis/broker/grpc/test/grpc_test_keys/"
        "server_1234.crt");
    std::string ca_crt = from_disk(
        "/data/dev/centreon-collect-bis/broker/grpc/test/grpc_test_keys/"
        "ca_1234.crt");
    return std::make_tuple(key, crt, ca_crt);
  }
};

std::string grpc_test::_key;
std::string grpc_test::_crt;

namespace com::centreon::common::grpc::test {
class server_reactor
    : public ::grpc::ServerBidiReactor<MessageToServer, MessageToClient> {
  MessageToServer _request;
  MessageToClient _response;

 public:
  server_reactor() { StartRead(&_request); }

  void OnReadDone(bool ok) override {
    if (ok) {
      _response.set_int_value(_request.int_value());
      StartWrite(&_response);
    } else {
      Finish(::grpc::Status::OK);
    }
  }

  void OnWriteDone(bool ok) override {
    if (ok) {
      StartRead(&_request);
    }
  }

  // server version
  void OnDone() override { delete this; }
};

class grpc_server : public common::grpc::grpc_server_base,
                    public std::enable_shared_from_this<grpc_server>,
                    public TestService::Service {
 public:
  grpc_server(const std::shared_ptr<common::grpc::grpc_config>& conf)
      : common::grpc::grpc_server_base(conf, spdlog::default_logger()) {}

  void start() {
    ::grpc::Service::MarkMethodCallback(
        0,
        new ::grpc::internal::CallbackBidiHandler<MessageToServer,
                                                  MessageToClient>(
            [me = shared_from_this()](::grpc::CallbackServerContext* context) {
              return me->Export(context);
            }));

    _init(
        [this](::grpc::ServerBuilder& builder) {
          builder.RegisterService(this);
        },
        false);
  }

  ::grpc::ServerBidiReactor<MessageToServer, MessageToClient>* Export(
      ::grpc::CallbackServerContext* context) {
    auto authctx = context->auth_context();

    return new server_reactor();
  }
};

class client_reactor
    : public ::grpc::ClientBidiReactor<MessageToServer, MessageToClient> {
  MessageToServer _request;
  MessageToClient _response;
  uint32_t _received_value ABSL_LOCKS_EXCLUDED(_received_value_m);
  mutable absl::Mutex _received_value_m;
  ::grpc::ClientContext _context;

 public:
  ::grpc::ClientContext& get_context() { return _context; }

  void send_to_server(uint32_t value) {
    _request.set_int_value(value);
    StartWrite(&_request);
  }

  bool wait(uint32_t expected_received_value) {
    absl::MutexLock l(&_received_value_m);

    struct waiter {
      uint32_t expected;
      uint32_t* received;

      bool operator()() const { return expected == *received; }
    };

    waiter wait = {expected_received_value, &_received_value};
    return _received_value_m.AwaitWithTimeout(absl::Condition(&wait),
                                              absl::Seconds(5));
  }

  void OnWriteDone(bool ok) override {
    if (ok) {
      StartRead(&_response);
    }
  }

  void OnReadDone(bool ok) override {
    if (ok) {
      absl::MutexLock l(&_received_value_m);
      SPDLOG_LOGGER_INFO(spdlog::default_logger(), "receive {}",
                         _response.int_value());
      _received_value = _response.int_value();
    }
  }

  void OnDone(const ::grpc::Status&) override { delete this; }

  void shutdown() {
    RemoveHold();
    _context.TryCancel();
  }
};

class grpc_client : public common::grpc::grpc_client_base {
  std::unique_ptr<TestService::Stub> _stub;

  client_reactor* _reactor;

 public:
  grpc_client(const std::shared_ptr<common::grpc::grpc_config>& conf)
      : common::grpc::grpc_client_base(conf, spdlog::default_logger()) {
    _stub = std::move(TestService::NewStub(_channel));
  }

  void start(uint32_t value_to_send) {
    _reactor = new client_reactor;
    auto& context = _reactor->get_context();
    auto auth = context.auth_context();
    _stub->async()->Export(&context, _reactor);
    _reactor->send_to_server(value_to_send);
    _reactor->StartCall();
  }

  bool wait(uint32_t expected_received_value) {
    return _reactor->wait(expected_received_value);
  }

  void shutdown() { _reactor->shutdown(); }
};

}  // namespace com::centreon::common::grpc::test

using namespace com::centreon::common::grpc;

TEST_F(grpc_test, with_cert_verify) {
  std::shared_ptr<grpc_config> server_conf = std::make_shared<grpc_config>(
      "localhost:7894", true, _crt, _key, "", "", false, 60);

  std::shared_ptr<test::grpc_server> server =
      std::make_shared<test::grpc_server>(server_conf);
  server->start();

  std::shared_ptr<grpc_config> client_conf = std::make_shared<grpc_config>(
      "localhost:7894", true, "", "", _crt, "", false, 60);
  std::shared_ptr<test::grpc_client> client =
      std::make_shared<test::grpc_client>(client_conf);

  uint32_t value = rand();
  SPDLOG_INFO("start test with value {}", value);
  client->start(value);
  ASSERT_TRUE(client->wait(value));

  server->shutdown(std::chrono::seconds(2));
}

TEST_F(grpc_test, with_root_cert_verify) {
  std::tuple<std::string, std::string, std::string> key_crt_ca =
      generate_key_cert_with_ca_v1(86400);

  std::shared_ptr<grpc_config> server_conf = std::make_shared<grpc_config>(
      "localhost:7894", true, std::get<1>(key_crt_ca), std::get<0>(key_crt_ca),
      std::get<2>(key_crt_ca), "", false, 60);

  std::shared_ptr<test::grpc_server> server =
      std::make_shared<test::grpc_server>(server_conf);
  server->start();

  std::shared_ptr<grpc_config> client_conf = std::make_shared<grpc_config>(
      "localhost:7894", true, "", "", std::get<2>(key_crt_ca), "", false, 60);
  std::shared_ptr<test::grpc_client> client =
      std::make_shared<test::grpc_client>(client_conf);

  uint32_t value = rand();
  SPDLOG_INFO("start test with value {}", value);
  client->start(value);
  ASSERT_TRUE(client->wait(value));

  server->shutdown(std::chrono::seconds(2));
}

TEST_F(grpc_test, with_cert_verify_but_without_cert) {
  std::shared_ptr<grpc_config> server_conf = std::make_shared<grpc_config>(
      "localhost:7895", true, _crt, _key, "", "", false, 60);

  std::shared_ptr<test::grpc_server> server =
      std::make_shared<test::grpc_server>(server_conf);
  server->start();

  std::shared_ptr<grpc_config> client_conf = std::make_shared<grpc_config>(
      "localhost:7895", true, "", "", "", "", false, 60);
  std::shared_ptr<test::grpc_client> client =
      std::make_shared<test::grpc_client>(client_conf);

  uint32_t value = rand();
  SPDLOG_INFO("start test with value {}", value);
  client->start(value);
  ASSERT_FALSE(client->wait(value));

  server->shutdown(std::chrono::seconds(2));
}

TEST_F(grpc_test, without_cert_verify) {
  std::shared_ptr<grpc_config> server_conf = std::make_shared<grpc_config>(
      "localhost:7896", true, _crt, _key, "", "", false, 60);

  std::shared_ptr<test::grpc_server> server =
      std::make_shared<test::grpc_server>(server_conf);
  server->start();

  std::shared_ptr<grpc_config> client_conf = std::make_shared<grpc_config>(
      "localhost:7896", true, "", "", "", "", false, 60);
  client_conf->set_skip_server_certificate_verification(true);
  std::shared_ptr<test::grpc_client> client =
      std::make_shared<test::grpc_client>(client_conf);

  uint32_t value = rand();
  SPDLOG_INFO("start test with value {}", value);
  client->start(value);
  ASSERT_TRUE(client->wait(value));

  server->shutdown(std::chrono::seconds(2));
}

TEST_F(grpc_test, with_father_cert_verify) {
  std::shared_ptr<grpc_config> server_conf = std::make_shared<grpc_config>(
      "localhost:7894", true, _crt, _key, "", "", false, 60);

  std::shared_ptr<test::grpc_server> server =
      std::make_shared<test::grpc_server>(server_conf);
  server->start();

  std::shared_ptr<grpc_config> client_conf = std::make_shared<grpc_config>(
      "localhost:7894", true, "", "", _crt, "", false, 60);
  std::shared_ptr<test::grpc_client> client =
      std::make_shared<test::grpc_client>(client_conf);

  uint32_t value = rand();
  SPDLOG_INFO("start test with value {}", value);
  client->start(value);
  ASSERT_TRUE(client->wait(value));

  server->shutdown(std::chrono::seconds(2));
}
