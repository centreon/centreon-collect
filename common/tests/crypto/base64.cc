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

#include <gtest/gtest.h>

#include "common/crypto/base64.hh"

using namespace com::centreon::common::crypto;

TEST(StringBase64, Encode) {
  ASSERT_EQ(base64_encode("A first little attempt."),
            "QSBmaXJzdCBsaXR0bGUgYXR0ZW1wdC4=");
  ASSERT_EQ(base64_encode("A"), "QQ==");
  ASSERT_EQ(base64_encode("AB"), "QUI=");
  ASSERT_EQ(base64_encode("ABC"), "QUJD");
}

TEST(StringBase64, Decode) {
  ASSERT_EQ(
      base64_decode(base64_encode("A first little attempt.")),
      "A first little attempt.");
  ASSERT_EQ(
      base64_decode(base64_encode(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")),
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
  ASSERT_EQ(base64_decode(base64_encode("a")), "a");
  ASSERT_EQ(base64_decode(base64_encode("ab")), "ab");
  ASSERT_EQ(base64_decode(base64_encode("abc")), "abc");
  std::string str("告'警'数\\量");
  ASSERT_EQ(base64_decode(base64_encode(str)), str);
}
