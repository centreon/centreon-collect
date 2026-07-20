/**
 * Copyright 2025 Centreon
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

#include <gtest/gtest.h>

#include "com/centreon/common/hex_dump.hh"
#include "common/crypto/aes256.hh"
#include "common/crypto/base64.hh"

using namespace com::centreon::common;
using namespace com::centreon::common::crypto;

/**
 * @brief aes256 implementation must follow what is done in php.
 *
 * By default, aes256 needs a 32 bytes key. If the key has a different size,
 * the php implementation resizes it to 32 bytes by padding it with
 * zeroes. This test checks that the aes256 implementation behaves the same way.
 *
 * First, we encrypt a string with a key that is smaller than 32 bytes.
 * Then, we create a new aes256 object with a key that is padded to 32
 * bytes with zeroes. Finally, we decrypt the encrypted string with the new
 * aes256 object and check that the decrypted string is the same as the
 * original string.
 */
TEST(StringAES256, SmallFirstKey) {
  std::string first_key("A little test");
  std::string second_key("The salt to use");
  std::string first_key_b64 = base64_encode(first_key);
  std::string second_key_b64 = base64_encode(second_key);
  aes256 aes(first_key_b64, second_key_b64);
  std::string result = aes.encrypt("To encrypt");
  std::string decrypted = aes.decrypt(result);
  ASSERT_NE(result, "To encrypt");
  ASSERT_EQ(decrypted, "To encrypt");

  std::string first_key_completed;
  first_key_completed.resize(32, 0);
  ASSERT_EQ(first_key_completed.size(), 32);
  std::copy(first_key.begin(), first_key.end(), first_key_completed.begin());
  std::cout << "First key completed: "
            << debug_buf(first_key_completed.data(), first_key_completed.size(),
                         32)
            << std::endl;
  aes256 aes_completed(base64_encode(first_key_completed), second_key_b64);
  std::string new_decrypted = aes_completed.decrypt(result);
  ASSERT_EQ(new_decrypted, "To encrypt");
}

/**
 * @brief aes256 implementation must follow what is done in php.
 *
 * This test checks that the aes256 implementation behaves the same way as
 * the php implementation when the first key is progressively increased.
 * The second key is fixed.
 *
 * We start with a first key of size 1 and progressively increase it until
 * it reaches the size of the string template (bigger than 32 bytes). For each
 * size, we create a new aes256 object with the first key and a fixed second
 * key. We then encrypt a string and decrypt it with the same aes256 object.
 * Finally, we check that the decrypted string is the same as the original
 * string.
 */
TEST(StringAES256, ProgressiveFirstKey) {
  std::string_view temp(
      "A little key that is long enough... Please, please, use it!");
  std::string to_encrypt("The quick brown fox jumps over the lazy dog");
  for (size_t i = 1; i < temp.size(); ++i) {
    std::string_view first_key(temp.data(), i);
    std::cout << "First key (" << i << "): "
              << debug_buf(first_key.data(), first_key.size(), temp.size())
              << std::endl;
    std::string second_key("The salt to use");
    std::string first_key_b64 = base64_encode(first_key);
    std::string second_key_b64 = base64_encode(second_key);
    aes256 aes(first_key_b64, second_key_b64);
    std::string result = aes.encrypt(to_encrypt);
    std::string decrypted = aes.decrypt(result);
    ASSERT_EQ(decrypted, to_encrypt);
  }
}

/**
 * @brief aes256 implementation must follow what is done in php.
 *
 * This test checks that the aes256 implementation behaves the same way as
 * the php implementation when the first key is bigger than 32 bytes.
 * The second key is fixed.
 *
 * We start with a first key of size 32 and progressively increase it until
 * it reaches the size of the string template (bigger than 32 bytes). For each
 * size, we create a new aes256 object with the first key and a fixed second
 * key. We then encrypt a string with the new aes256 object and decrypt it with
 * the initial aes256 one. Finally, we check that the decrypted string is the
 * same as the original string.
 */
TEST(StringAES256, BigFirstKey) {
  std::string_view temp(
      "A little key that is long enough... Please, please, use it!");
  std::string to_encrypt("The quick brown fox jumps over the lazy dog");
  std::string initial_key;
  std::unique_ptr<aes256> initial_aes;

  for (size_t i = 32; i < temp.size(); ++i) {
    std::string_view first_key(temp.data(), i);
    std::cout << "First key (" << i << "): "
              << debug_buf(first_key.data(), first_key.size(), temp.size())
              << std::endl;
    std::string second_key("The salt to use");
    std::string first_key_b64 = base64_encode(first_key);
    std::string second_key_b64 = base64_encode(second_key);
    if (i == 32) {
      initial_key = first_key_b64;
      initial_aes = std::make_unique<aes256>(first_key_b64, second_key_b64);
    }

    aes256 aes(first_key_b64, second_key_b64);
    std::string result = aes.encrypt(to_encrypt);
    /* As the first key is truncated to 32 bytes, we can use the initial aes
     * object to decrypt the result. */
    std::string decrypted = initial_aes->decrypt(result);
    ASSERT_EQ(decrypted, to_encrypt);
  }
}
