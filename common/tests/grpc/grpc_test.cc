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
#include <spdlog/spdlog.h>

#include "common/tests/grpc/grpc_test.grpc.pb.h"

#include "com/centreon/common/grpc/grpc_client.hh"
#include "com/centreon/common/grpc/grpc_server.hh"
#include "grpc/grpc_test.pb.h"

extern std::shared_ptr<asio::io_context> g_io_context;

class grpc_test : public ::testing::Test {
 protected:
  static std::string _key;
  static std::string _crt;

 public:
  static void SetUpTestSuite() {
    EVP_PKEY* pkey = EVP_PKEY_new();
    BIGNUM* e = BN_new();
    BN_set_word(e, RSA_F4);  // 65537
    RSA* rsa = RSA_new();
    assert(RSA_generate_key_ex(rsa, 2048, e, NULL));
    EVP_PKEY_assign_RSA(pkey, rsa);  // Transfère la propriété à pkey
    BN_free(e);

    // Créer un certificat X.509 auto-signé
    X509* cert = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365);  // 1 an
    X509_set_pubkey(cert, pkey);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"FR",
                               -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               (unsigned char*)"InMemoryCorp", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (unsigned char*)"localhost", -1, -1, 0);
    X509_set_issuer_name(cert, name);

    assert(X509_sign(cert, pkey, EVP_sha256()));

    // Sauvegarder dans des buffers mémoire (BIOs)
    BIO* cert_bio = BIO_new(BIO_s_mem());
    BIO* key_bio = BIO_new(BIO_s_mem());

    assert(PEM_write_bio_X509(cert_bio, cert));

    assert(PEM_write_bio_PrivateKey(key_bio, pkey, NULL, NULL, 0, NULL, NULL));

    // Extraire les données en mémoire
    BUF_MEM* cert_buf;
    BUF_MEM* key_buf;
    BIO_get_mem_ptr(cert_bio, &cert_buf);
    BIO_get_mem_ptr(key_bio, &key_buf);

    _crt.assign(cert_buf->data, cert_buf->length);
    _key.assign(key_buf->data, key_buf->length);

    BIO_free(cert_bio);
    BIO_free(key_bio);
    EVP_PKEY_free(pkey);
    X509_free(cert);

    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
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
