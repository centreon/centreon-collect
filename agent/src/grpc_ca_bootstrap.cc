/**
 * Copyright 2026 Centreon
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

#include "grpc_ca_bootstrap.hh"
#include "spdlog/spdlog.h"

#if defined(__linux__)
#include <pwd.h>
#elif defined(_WIN32)
#include <wincrypt.h>
#include <windows.h>
#endif

#include <openssl/x509.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "agent.grpc.pb.h"
#include "com/centreon/common/rapidjson_helper.hh"
#include "common/crypto/cert_tree.hh"

#include <filesystem>

namespace com::centreon::agent {

#if defined(_WIN32)
/**
 * @brief Convert a Win32 error code to a readable string.
 */
static std::string win_error_message(DWORD error_code) {
  LPSTR raw_message = nullptr;
  const DWORD format_result = ::FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error_code, 0, reinterpret_cast<LPSTR>(&raw_message), 0,
      nullptr);
  if (!format_result || !raw_message) {
    return "Unknown Win32 error";
  }

  std::string message(raw_message);
  ::LocalFree(raw_message);

  while (!message.empty() &&
         (message.back() == '\r' || message.back() == '\n')) {
    message.pop_back();
  }
  return message;
}

/**
 * @brief Persist CA certificate path in Windows registry.
 */
static bool persist_ca_path_in_registry(
    const std::string& registry_key,
    const std::string& ca_path,
    const std::shared_ptr<spdlog::logger>& logger) {
  HKEY h_key = nullptr;
  const LSTATUS create_status =
      ::RegCreateKeyExA(HKEY_LOCAL_MACHINE, registry_key.c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &h_key, nullptr);
  if (create_status != ERROR_SUCCESS) {
    SPDLOG_LOGGER_ERROR(logger, "Unable to open/create registry key {}: {}",
                        registry_key,
                        win_error_message(static_cast<DWORD>(create_status)));
    return false;
  }

  const DWORD value_size =
      static_cast<DWORD>(ca_path.size() + 1);  // include null terminator
  const LSTATUS set_status = ::RegSetValueExA(
      h_key, "ca_certificate", 0, REG_SZ,
      reinterpret_cast<const BYTE*>(ca_path.c_str()), value_size);
  ::RegCloseKey(h_key);

  if (set_status != ERROR_SUCCESS) {
    SPDLOG_LOGGER_ERROR(
        logger, "Unable to write ca_certificate in registry {}: {}",
        registry_key, win_error_message(static_cast<DWORD>(set_status)));
    return false;
  }

  SPDLOG_LOGGER_INFO(logger, "Updated registry {} with ca_certificate={}",
                     registry_key, ca_path);
  return true;
}

/**
 * @brief Import PEM CA certificate into Windows Local Machine certificate
 * store.
 */
static bool add_ca_to_windows_store(
    const std::string& ca_pem,
    const std::shared_ptr<spdlog::logger>& logger) {
  DWORD der_size = 0;
  if (!::CryptStringToBinaryA(ca_pem.c_str(), static_cast<DWORD>(ca_pem.size()),
                              CRYPT_STRING_ANY, nullptr, &der_size, nullptr,
                              nullptr)) {
    const DWORD error_code = ::GetLastError();
    SPDLOG_LOGGER_ERROR(logger, "Unable to decode PEM certificate: {}",
                        win_error_message(error_code));
    return false;
  }

  std::vector<BYTE> der(der_size);
  if (!::CryptStringToBinaryA(ca_pem.c_str(), static_cast<DWORD>(ca_pem.size()),
                              CRYPT_STRING_ANY, der.data(), &der_size, nullptr,
                              nullptr)) {
    const DWORD error_code = ::GetLastError();
    SPDLOG_LOGGER_ERROR(logger, "Unable to decode PEM certificate bytes: {}",
                        win_error_message(error_code));
    return false;
  }

  PCCERT_CONTEXT cert_context = ::CertCreateCertificateContext(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, der.data(), der_size);
  if (!cert_context) {
    const DWORD error_code = ::GetLastError();
    SPDLOG_LOGGER_ERROR(logger,
                        "Unable to create cert context from decoded CA: {}",
                        win_error_message(error_code));
    return false;
  }

  HCERTSTORE cert_store = ::CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                          CERT_SYSTEM_STORE_LOCAL_MACHINE |
                                              CERT_STORE_OPEN_EXISTING_FLAG |
                                              CERT_STORE_MAXIMUM_ALLOWED_FLAG,
                                          L"ROOT");

  if (!cert_store) {
    const DWORD error_code = ::GetLastError();
    SPDLOG_LOGGER_ERROR(logger,
                        "Unable to open Windows Local Machine ROOT store: {}",
                        win_error_message(error_code));
    ::CertFreeCertificateContext(cert_context);
    return false;
  }

  const BOOL add_status = ::CertAddCertificateContextToStore(
      cert_store, cert_context, CERT_STORE_ADD_REPLACE_EXISTING, nullptr);

  if (!add_status) {
    const DWORD error_code = ::GetLastError();
    SPDLOG_LOGGER_ERROR(logger,
                        "Unable to add CA certificate to Windows store: {}",
                        win_error_message(error_code));
  } else {
    SPDLOG_LOGGER_INFO(
        logger,
        "CA certificate added to Windows Local Machine ROOT certificate store");
  }

  ::CertCloseStore(cert_store, 0);
  ::CertFreeCertificateContext(cert_context);
  return add_status == TRUE;
}

#endif

#if defined(__linux__)
/**
 * @brief Ensure the persisted CA file is owned by the agent service account.
 *
 * @return true when ownership already matches or was successfully updated.
 */
static bool set_owner_to_agent_user(
    const std::filesystem::path& file_path,
    const std::shared_ptr<spdlog::logger>& logger) {
  const auto k_agent_user = "centreon-monitoring-agent";
  struct passwd* user = ::getpwnam(k_agent_user);
  if (!user) {
    SPDLOG_LOGGER_ERROR(logger, "Unable to resolve user {}", k_agent_user);
    return false;
  }

  struct stat st {};
  if (::stat(file_path.c_str(), &st) != 0) {
    SPDLOG_LOGGER_ERROR(logger,
                        "Unable to stat CA file {} before ownership update",
                        file_path.string());
    return false;
  }

  if (st.st_uid == user->pw_uid && st.st_gid == user->pw_gid) {
    return true;
  }

  if (::chown(file_path.c_str(), user->pw_uid, user->pw_gid) != 0) {
    SPDLOG_LOGGER_ERROR(logger, "Unable to set CA file owner to {} for {}",
                        k_agent_user, file_path.string());
    return false;
  }

  SPDLOG_LOGGER_INFO(logger, "Set CA file owner to {}", k_agent_user);
  return true;
}
#endif

/**
 * @brief Write the CA file path into the JSON configuration document.
 *
 * Updates `ca` or `ca_certificate` when present, otherwise adds
 * `ca_certificate`.
 */
static void set_ca_path_in_config_document(rapidjson::Document& doc,
                                           const std::string& ca_path) {
  auto& allocator = doc.GetAllocator();

  // Reuse a single update path for both accepted config keys.
  auto set_ca_path_member = [&](const char* member_name) {
    if (doc.HasMember(member_name)) {
      doc[member_name].SetString(
          ca_path.c_str(), static_cast<rapidjson::SizeType>(ca_path.size()),
          allocator);
      return true;
    }
    return false;
  };

  if (set_ca_path_member("ca") || set_ca_path_member("ca_certificate")) {
    return;
  }

  rapidjson::Value key("ca_certificate", allocator);
  rapidjson::Value value;
  value.SetString(ca_path.c_str(),
                  static_cast<rapidjson::SizeType>(ca_path.size()), allocator);
  doc.AddMember(key, value, allocator);
}

/**
 * @brief Persist the CA path to the JSON config file.
 */
static bool persist_ca_path_in_config_file(
    const std::string& config_path,
    const std::string& ca_path,
    const std::shared_ptr<spdlog::logger>& logger) {
  rapidjson::Document doc =
      com::centreon::common::rapidjson_helper::read_from_file(config_path);
  if (!doc.IsObject()) {
    SPDLOG_LOGGER_ERROR(logger, "configuration root is not an object in {}",
                        config_path);
    return false;
  }

  set_ca_path_in_config_document(doc, ca_path);

  rapidjson::StringBuffer json_buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(json_buffer);
  doc.Accept(writer);

  std::ofstream config_output(config_path, std::ios::trunc);
  if (!config_output.is_open()) {
    SPDLOG_LOGGER_ERROR(logger, "unable to open config file {} for writing",
                        config_path);
    return false;
  }
  config_output << json_buffer.GetString() << '\n';
  return true;
}

/**
 * @brief Resolve platform default target file for a bootstrapped CA.
 */
static std::filesystem::path get_default_bootstrap_ca_path(
    const std::shared_ptr<spdlog::logger>& logger [[maybe_unused]]) {
#if defined(_WIN32)
  char module_path[MAX_PATH] = {};
  DWORD module_path_len = ::GetModuleFileNameA(nullptr, module_path, MAX_PATH);
  if (module_path_len > 0 && module_path_len < MAX_PATH) {
    return std::filesystem::path(module_path).parent_path() / "cma-ca.pem";
  }

  const DWORD error_code = ::GetLastError();
  SPDLOG_LOGGER_WARN(
      logger,
      "Unable to resolve executable directory ({}), defaulting to install path",
      win_error_message(error_code));
  return std::filesystem::path(
             "C:\\Program Files\\Centreon\\CentreonMonitoringAgent") /
         "cma-ca.pem";
#else
  return std::filesystem::path("/etc/centreon-monitoring-agent/cma-ca.crt");
#endif
}

/**
 * @brief Try direct platform persistence that may avoid file/config updates.
 *
 * On Windows, importing into certificate store is attempted first. If it
 * succeeds, file/config persistence is skipped.
 */
static bool try_platform_direct_ca_persist(
    const std::string& ca_pem [[maybe_unused]],
    const std::string& config_reference [[maybe_unused]],
    bool& done_without_file,
    const std::shared_ptr<spdlog::logger>& logger [[maybe_unused]]) {
#if defined(_WIN32)
  done_without_file = add_ca_to_windows_store(ca_pem, logger);
  if (!done_without_file) {
    SPDLOG_LOGGER_WARN(
        logger,
        "Unable to persist CA in Windows Local Machine certificate store; "
        "fallback to file and registry persistence");
  }
  return true;
#else
  done_without_file = false;
  return true;
#endif
}

/**
 * @brief Apply platform-specific side effects after CA file write.
 */
static bool apply_platform_ca_post_file_persist(
    const std::filesystem::path& ca_path [[maybe_unused]],
    const std::shared_ptr<spdlog::logger>& logger [[maybe_unused]]) {
#if defined(__linux__)
  return set_owner_to_agent_user(ca_path, logger);
#else
  return true;
#endif
}

/**
 * @brief Persist CA path reference in platform config backend.
 */
static bool persist_platform_ca_path_reference(
    const std::string& config_reference,
    const std::string& ca_path,
    const std::shared_ptr<spdlog::logger>& logger) {
#if defined(_WIN32)
  return persist_ca_path_in_registry(config_reference, ca_path, logger);
#else
  return persist_ca_path_in_config_file(config_reference, ca_path, logger);
#endif
}

/**
 * @brief Read a whole file into memory.
 *
 * @return file content, or an empty string when reading fails.
 */
std::string read_file_content(const std::string& file_path,
                              const std::shared_ptr<spdlog::logger>& logger) {
  if (file_path.empty()) {
    return {};
  }
  try {
    std::ifstream file(file_path);
    if (!file.is_open()) {
      SPDLOG_LOGGER_ERROR(logger, "fail to open {}", file_path);
      return {};
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to read {}: {}", file_path, e.what());
  }
  return {};
}

/**
 * @brief Build the workflow object with runtime config and logger dependencies.
 */
ca_bootstrap_workflow::ca_bootstrap_workflow(
    const config& conf,
    std::string config_path,
    std::shared_ptr<com::centreon::common::grpc::grpc_config> grpc_conf,
    std::shared_ptr<spdlog::logger> logger)
    : _conf(conf),
      _config_path(std::move(config_path)),
      _grpc_conf(std::move(grpc_conf)),
      _logger(std::move(logger)) {}

/**
 * @brief Execute the CA bootstrap flow when fingerprint mode is enabled.
 */
void ca_bootstrap_workflow::run() {
  if (!is_bootstrap_needed()) {
    return;
  }

  // First try to connect with the provided configuration. If it works, no need
  // to do anything.
  if (probe_full_tls_connection()) {
    SPDLOG_LOGGER_INFO(_logger,
                       "Full TLS connection succeeded with existing "
                       "configuration");
    return;
  }

  SPDLOG_LOGGER_WARN(_logger,
                     "Full TLS connection to {} failed and fingerprint is "
                     "configured; trying "
                     "TLS_SKIP_VERIFY_CA CA bootstrap",
                     _conf.get_endpoint());

  std::optional<std::string> ca_pem = fetch_ca_with_tls_skip_verify();
  if (!ca_pem || !validate_ca_fingerprint(*ca_pem)) {
    return;
  }

  _grpc_conf->set_ca(*ca_pem);
  SPDLOG_LOGGER_CRITICAL(
      _logger,
      "[SECURITY] CA bootstrap succeeded from fingerprint for endpoint {}",
      _conf.get_endpoint());
  if (!persist_ca_to_file_and_config(*ca_pem)) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "CA bootstrap succeeded but persistence failed for endpoint {}; "
        "TLS will work this session only",
        _conf.get_endpoint());
  }
}

/**
 * @brief Tell whether CA bootstrap should run for the current configuration.
 */
bool ca_bootstrap_workflow::is_bootstrap_needed() const {
  return !_conf.use_reverse_connection() && !_conf.get_ca_fingerprint().empty();
}

/**
 * @brief Probe endpoint connectivity using full TLS with current settings.
 */
bool ca_bootstrap_workflow::probe_full_tls_connection() const {
  try {
    bootstrap_grpc_client client(_grpc_conf, _logger);
    auto channel = client.get_channel();
    if (!channel) {
      SPDLOG_LOGGER_WARN(_logger, "TLS probe failed for {}: null channel",
                         _grpc_conf->get_hostport());
      return false;
    }

    const bool connected = channel->WaitForConnected(
        std::chrono::system_clock::now() + std::chrono::seconds(5));
    channel.reset();
    return connected;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_WARN(_logger, "TLS probe failed for {}: {}",
                       _grpc_conf->get_hostport(), e.what());
    return false;
  }
}

/**
 * @brief Fetch the remote CA certificate over TLS_SKIP_VERIFY_CA mode.
 *
 * @return remote CA PEM when retrieval succeeds, `std::nullopt` otherwise.
 */
std::optional<std::string>
ca_bootstrap_workflow::fetch_ca_with_tls_skip_verify() const {
  auto temp_grpc_conf =
      std::make_shared<com::centreon::common::grpc::grpc_config>(
          _conf.get_endpoint(),
          com::centreon::common::grpc::grpc_config::TLS_SKIP_VERIFY_CA,
          read_file_content(_conf.get_public_cert_file(), _logger),
          read_file_content(_conf.get_private_key_file(), _logger), "" /* ca */,
          _conf.get_ca_name(), true, 30,
          _conf.get_second_max_reconnect_backoff(),
          _conf.get_max_message_length(), _conf.get_token(),
          _conf.get_trusted_tokens(), _conf.get_ca_fingerprint());

  bootstrap_grpc_client client(temp_grpc_conf, _logger);
  auto stub = CACertificateService::NewStub(client.get_channel());

  CACertificateRequest request;
  CACertificateResponse response;
  ::grpc::ClientContext context;

  const std::string& fingerprint = _conf.get_ca_fingerprint();
  if (!fingerprint.empty()) {
    context.AddMetadata("x-token", fingerprint);
  }

  const ::grpc::Status status =
      stub->GetCACertificate(&context, request, &response);
  if (!status.ok()) {
    SPDLOG_LOGGER_WARN(
        _logger, "CA bootstrap via TLS_SKIP_VERIFY_CA failed for {}: {} ({})",
        _conf.get_endpoint(), status.error_message(),
        static_cast<int>(status.error_code()));
    return std::nullopt;
  }

  if (response.certificate_pem().empty()) {
    SPDLOG_LOGGER_WARN(_logger, "CA bootstrap returned an empty certificate");
    return std::nullopt;
  }

  return response.certificate_pem();
}

/**
 * @brief Validate the fetched CA certificate against configured fingerprint.
 */
bool ca_bootstrap_workflow::validate_ca_fingerprint(
    const std::string& ca_pem) const {
  try {
    std::unique_ptr<X509, decltype(&X509_free)> cert(
        com::centreon::common::crypto::cert_tree::load_cert_from_string(ca_pem),
        &X509_free);
    if (!cert) {
      SPDLOG_LOGGER_ERROR(_logger, "Unable to load cert from string");
      return false;
    }
    const std::string fingerprint =
        com::centreon::common::crypto::cert_tree::cert_sha(cert.get());

    if (_conf.get_ca_fingerprint() != fingerprint) {
      SPDLOG_LOGGER_ERROR(_logger, "Retrieved CA fingerprint mismatch ");
      return false;
    }

    return true;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        _logger, "Unable to validate retrieved CA fingerprint: {}", e.what());
    return false;
  }
}

/**
 * @brief Persist fetched CA to platform trust/config backends.
 *
 * On Windows, certificate store persistence is attempted first. If it works,
 * file and registry updates are skipped.
 */
bool ca_bootstrap_workflow::persist_ca_to_file_and_config(
    const std::string& ca_pem) const {
  try {
    bool done_without_file = false;
    try_platform_direct_ca_persist(ca_pem, _config_path, done_without_file,
                                   _logger);
    if (done_without_file) {
      SPDLOG_LOGGER_INFO(
          _logger,
          "Persisted CA certificate in platform trust store; skipping file "
          "and config update");
      return true;
    }

    const std::filesystem::path ca_path =
        get_default_bootstrap_ca_path(_logger);
    const std::filesystem::path parent = ca_path.parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent);
    }

    {
      std::ofstream ca_output(ca_path, std::ios::trunc);
      if (!ca_output.is_open()) {
        throw std::runtime_error("unable to open CA file for writing");
      }
      ca_output << ca_pem;
      if (ca_pem.empty() || ca_pem.back() != '\n') {
        ca_output << '\n';
      }
    }

    if (!apply_platform_ca_post_file_persist(ca_path, _logger)) {
      SPDLOG_LOGGER_WARN(_logger, "unable to set the owner for the cert");
    }

    const std::string ca_path_string = ca_path.string();
    if (!persist_platform_ca_path_reference(_config_path, ca_path_string,
                                            _logger)) {
      return false;
    }

    SPDLOG_LOGGER_INFO(_logger, "Persisted CA certificate to {} and updated {}",
                       ca_path_string, _config_path);
    return true;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        _logger, "Failed to persist retrieved CA certificate to config {}: {}",
        _config_path, e.what());
    return false;
  }
}

/**
 * @brief Entrypoint helper that runs CA bootstrap when needed.
 */
void bootstrap_ca_from_fingerprint_if_needed(
    const config& conf,
    const std::string& config_path,
    const std::shared_ptr<com::centreon::common::grpc::grpc_config>& grpc_conf,
    const std::shared_ptr<spdlog::logger>& logger) {
  ca_bootstrap_workflow(conf, config_path, grpc_conf, logger).run();
}

}  // namespace com::centreon::agent
