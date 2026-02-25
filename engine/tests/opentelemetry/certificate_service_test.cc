/**
 * Copyright 2026 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <gtest/gtest.h>

#include <openssl/x509v3.h>
#include "agent.grpc.pb.h"
#include "com/centreon/engine/modules/opentelemetry/certificate_service.hh"
#include "common/crypto/cert_tree.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;
using namespace com::centreon;
// Sample CA certificate for testing (self-signed)
const char* TEST_CA_CERT = R"(-----BEGIN CERTIFICATE-----
MIIDgzCCAmugAwIBAgIUTIxSeVMzS+1wejZJ3twXIpJSrGEwDQYJKoZIhvcNAQEL
BQAwUDELMAkGA1UEBhMCRlIxDTALBgNVBAgMBFRlc3QxDTALBgNVBAcMBFRlc3Qx
ETAPBgNVBAoMCENlbnRyZW9uMRAwDgYDVQQDDAdUZXN0IENBMCAXDTI2MDIyNTIz
NTYxM1oYDzIxMjYwMjAxMjM1NjEzWjBQMQswCQYDVQQGEwJGUjENMAsGA1UECAwE
VGVzdDENMAsGA1UEBwwEVGVzdDERMA8GA1UECgwIQ2VudHJlb24xEDAOBgNVBAMM
B1Rlc3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCvKrsplv/m
FMvMsRpRsasX3rt1NMxgmqgrM14+9Vw1nbI9PQeBq/5rA3n/NiWFIUEZSH/45t9f
GK3coK6Y9XtPdB4cJqR0/pvTuFbADXs6TR3GaaLPt/S9gRXPEftw9QjjtjJDQQP+
iYKAUtLz+DmOGzi3XKvfC6WpokOx7psTbOLqllCt5jFlLatJVoR1Wt0lyXDSWVFz
YyLqvtFeD6tCcHXIrJBlRjat4cN6W5ZyYpUEcwJJmMu179sgfHS0vncpTEQcS2D/
Av03b4l0ZX3ZIC7PITBy7rcfxp1dAs3duaqj8iR49jpZl4rk9u1hb5ZDnoivZ7KF
pMHIhJTTYoQ/AgMBAAGjUzBRMB0GA1UdDgQWBBTey/ZUxIHca62Sl3hVFqqeUeS1
fTAfBgNVHSMEGDAWgBTey/ZUxIHca62Sl3hVFqqeUeS1fTAPBgNVHRMBAf8EBTAD
AQH/MA0GCSqGSIb3DQEBCwUAA4IBAQB1QmgYsgcrhKkGZ8ybp5kIS9QOJc7PuDGM
iXanaPqoS5RwzwC+ddCDHfqJ4zrch0DBWEyT+p8TAQB4TltNvmDgAR8wGo8dt9um
ROThYfUoe3iliwbqs2YmghZ/Ir2zuBmORXdQpphvXmuoJADLgnzIFV6nx3aER0gK
Vjycffvh5MQRw06EI92TH48hvp4KOMWglBCwOCs8JAn3KgItrrboDYugE0QxJRQY
k92tlZI9pFcj/kh2/cMYNLOz2+rzzAVHVLRFahtxpn16hwcxrsy9E0j7b+lZKPOZ
u63GnH//c1m+jQyTACtX4ApUN6h+Qx6e6u3CBmEOsEyB/RAKi4Bg
-----END CERTIFICATE-----)";

class certificate_service_test : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;
  std::shared_ptr<certificate_service> _service_impl;

  std::unique_ptr<grpc::Server> _server;
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<com::centreon::agent::CACertificateService::Stub> _stub;

  std::string _fingerprint_prefix;

  void SetUp() override {
    _logger = spdlog::default_logger();

    X509* cert = common::crypto::cert_tree::load_cert_from_string(TEST_CA_CERT);

    _fingerprint_prefix =
        common::crypto::cert_tree::cert_sha(cert).substr(0, 6);

    _service_impl = certificate_service::load(_logger, TEST_CA_CERT);

    grpc::ServerBuilder builder;
    builder.RegisterService(_service_impl.get());

    _server = builder.BuildAndStart();
    _channel = _server->InProcessChannel(grpc::ChannelArguments());

    _stub = com::centreon::agent::CACertificateService::NewStub(_channel);
  }

  void TearDown() override {
    _server->Shutdown();
    _service_impl.reset();
  }
};

TEST_F(certificate_service_test, GetCACertificateWithValidToken) {
  grpc::ClientContext context;
  context.AddMetadata("x-token", _fingerprint_prefix);

  com::centreon::agent::CACertificateRequest request;
  com::centreon::agent::CACertificateResponse response;

  grpc::Status status = _stub->GetCACertificate(&context, request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::OK);
  EXPECT_EQ(response.certificate_pem(), TEST_CA_CERT);
  SPDLOG_LOGGER_INFO(_logger, "received CA certificate:\n{}",
                     response.certificate_pem());
}

// Test CA certificate retrieval without token (should fail)
TEST_F(certificate_service_test, GetCACertificateWithoutToken) {
  grpc::ClientContext context;

  com::centreon::agent::CACertificateRequest request;
  com::centreon::agent::CACertificateResponse response;

  grpc::Status status = _stub->GetCACertificate(&context, request, &response);

  EXPECT_EQ(status.error_code(), ::grpc::StatusCode::UNAUTHENTICATED);
  EXPECT_EQ(status.error_message(), "Missing authentication token");
  EXPECT_TRUE(response.certificate_pem().empty());
}

// Test CA certificate retrieval with invalid token (should fail)
TEST_F(certificate_service_test, GetCACertificateWithInvalidToken) {
  grpc::ClientContext context;
  context.AddMetadata("x-token", "invalid_token_999");

  com::centreon::agent::CACertificateRequest request;
  com::centreon::agent::CACertificateResponse response;

  grpc::Status status = _stub->GetCACertificate(&context, request, &response);

  EXPECT_EQ(status.error_code(), ::grpc::StatusCode::PERMISSION_DENIED);
  EXPECT_EQ(status.error_message(), "Invalid authentication token");
  EXPECT_TRUE(response.certificate_pem().empty());
}

// Test with empty token
TEST_F(certificate_service_test, GetCACertificateWithEmptyToken) {
  grpc::ClientContext context;
  context.AddMetadata("x-token", "");  // Empty token

  com::centreon::agent::CACertificateRequest request;
  com::centreon::agent::CACertificateResponse response;

  grpc::Status status = _stub->GetCACertificate(&context, request, &response);

  EXPECT_EQ(status.error_code(), ::grpc::StatusCode::PERMISSION_DENIED);
  EXPECT_EQ(status.error_message(), "Invalid authentication token");
  EXPECT_TRUE(response.certificate_pem().empty());
}
