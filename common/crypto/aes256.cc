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
#include <absl/container/fixed_array.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "aes256.hh"
#include "base64.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::common::crypto {

/**
 * @brief The aes256 constructor. This class is used to encrypt and decrypt
 * data using the AES256 algorithm. Two keys are provided to the constructor.
 * The first key is used to encrypt the data and the second key is used to
 * generate a hash of the encrypted data. This hash is used to check The
 * integrity of the data.
 *
 * @param first_key The first key used to encrypt the data.
 * @param second_key The second key used to generate the hash of the encrypted.
 */
aes256::aes256(const std::string& first_key, const std::string& second_key)
    : _first_key{base64_decode(first_key)},
      _second_key(base64_decode(second_key)) {
  if (_first_key.size() != 32)
    throw exceptions::msg_fmt(
        "the key for aes256 must have a size of 256 bits and not {}",
        _first_key.size() * 8);
  assert(!_second_key.empty());
}

/**
 * @brief Encrypt the input string using the AES256 algorithm.
 *
 * @param input The string to encrypt.
 *
 * @return The encrypted string.
 */
std::string aes256::encrypt(const std::string& input) {
  const int iv_length = EVP_CIPHER_iv_length(EVP_aes_256_cbc());

  uint32_t crypted_size =
      input.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc());

  std::string result;
  /* Here is the result just before the base64 encoding. It is composed of:
   * * the iv
   * * the hash of size 64
   * * the crypted vector
   * We reserve result to be suffisantly big to contain them. crypted_size
   * is an estimated size, always bigger than the real result.
   */
  result.resize(iv_length + 64 + crypted_size);

  std::string_view iv(result.data(), iv_length);
  std::string_view hmac(result.data() + iv_length, 64);
  std::string_view crypted_vector(result.data() + iv_length + 64, crypted_size);

  RAND_bytes((unsigned char*)iv.data(), iv_length);

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                          (unsigned char*)_first_key.data(),
                          (unsigned char*)iv.data())) {
    EVP_CIPHER_CTX_free(ctx);
    throw exceptions::msg_fmt("Encryption initialization failed");
  }

  int output_length;
  if (!EVP_EncryptUpdate(ctx, (unsigned char*)crypted_vector.data(),
                         &output_length, (unsigned char*)input.data(),
                         input.size())) {
    EVP_CIPHER_CTX_free(ctx);
    throw exceptions::msg_fmt("Encryption failed");
  }
  int len = output_length;

  if (!EVP_EncryptFinal_ex(
          ctx, (unsigned char*)crypted_vector.data() + output_length, &len)) {
    uint64_t err = ERR_get_error();
    absl::FixedArray<char, 1024> mess(0);
    ERR_error_string_n(err, mess.data(), 1023);
    EVP_CIPHER_CTX_free(ctx);
    throw exceptions::msg_fmt("Encryption finalization failed: {}",
                              mess.data());
  }
  assert(output_length + len <= (int)crypted_size);
  crypted_size = output_length + len;
  result.resize(iv_length + 64 + crypted_size);
  crypted_vector = crypted_vector.substr(0, crypted_size);
  EVP_CIPHER_CTX_free(ctx);

  // iv is already at the beginning of result.
  // We put the hmac at the second place
  uint32_t output_len;
  if (!HMAC(EVP_sha3_512(), _second_key.data(), _second_key.length(),
            (unsigned char*)crypted_vector.data(), crypted_vector.size(),
            (unsigned char*)hmac.data(), &output_len))
    throw exceptions::msg_fmt(
        "Error during the message authentication code computation");
  assert(output_len == 64);

  return base64_encode(result);
}

/**
 * @brief Decrypt the input string using the AES256 algorithm.
 *
 * @param input The string to decrypt.
 *
 * @return The decrypted string.
 */
std::string aes256::decrypt(const std::string& input) {
  std::string mix = base64_decode(input);

  const int iv_length = EVP_CIPHER_iv_length(EVP_aes_256_cbc());
  if (iv_length <= 0) {
    throw exceptions::msg_fmt("Error when retrieving the cipher length");
  }

  if (mix.size() <= static_cast<size_t>(iv_length) + 64)
    throw exceptions::msg_fmt("The content is not AES256 encrypted");

  std::string_view iv(mix.data(), iv_length);
  std::string_view hash(mix.data() + iv_length, 64);
  std::string_view encrypted_first_part(mix.data() + iv_length + 64,
                                        mix.size() - iv_length - 64);

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  int len = 0;
  int plaintext_len = 0;

  std::string data;
  data.resize(encrypted_first_part.size() +
              EVP_CIPHER_block_size(EVP_aes_256_cbc()));

  if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                          (unsigned char*)_first_key.data(),
                          (unsigned char*)iv.data())) {
    EVP_CIPHER_CTX_free(ctx);
    throw exceptions::msg_fmt("Decryption initialization failed");
  }

  if (!EVP_DecryptUpdate(ctx, (unsigned char*)data.data(), &len,
                         (unsigned char*)encrypted_first_part.data(),
                         encrypted_first_part.size())) {
    EVP_CIPHER_CTX_free(ctx);
    throw exceptions::msg_fmt("Decryption failed");
  }
  plaintext_len = len;

  if (!EVP_DecryptFinal_ex(ctx, (unsigned char*)data.data() + len, &len)) {
    uint64_t err = ERR_get_error();
    absl::FixedArray<char, 1024> mess(0);
    ERR_error_string_n(err, mess.data(), 1023);
    EVP_CIPHER_CTX_free(ctx);
    throw exceptions::msg_fmt("Decryption finalization failed: {}",
                              mess.data());
  }
  plaintext_len += len;

  data.resize(plaintext_len);
  EVP_CIPHER_CTX_free(ctx);

  if (!data.empty()) {
    std::string second_encrypted_new;
    second_encrypted_new.resize(SHA512_DIGEST_LENGTH);
    uint32_t second_encrypted_length;
    if (!HMAC(EVP_sha3_512(), _second_key.data(), _second_key.length(),
              (unsigned char*)encrypted_first_part.data(),
              encrypted_first_part.length(),
              (unsigned char*)second_encrypted_new.data(),
              &second_encrypted_length))
      throw exceptions::msg_fmt(
          "Error during the message authentication code computation");

    assert(second_encrypted_length == 64);
    if (hash == second_encrypted_new)
      return data;
  }

  return std::string();
}

}  // namespace com::centreon::common::crypto
