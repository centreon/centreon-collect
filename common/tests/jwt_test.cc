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

#include <gtest/gtest.h>

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/jwt.hh"

using namespace com::centreon::common::crypto;
using namespace std::chrono;
using com::centreon::exceptions::msg_fmt;

// Test: Constructor
// Purpose: Validate initialization of jwt object with a valid token and check
// header, payload, signature and timestamps.
TEST(TestJWT, Constructor) {
  const std::string metadata =
      "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJleHAiOjE3NDI5OTE0NTgsImlhdCI6MTc0Mjk4Mzg1OCwiaXNzIjoiY2VudHJlb24ifQ."
      "6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ";
  jwt jwt(metadata);
  ASSERT_EQ(jwt.get_header(), "{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
  std::cout << "payload: " << jwt.get_payload() << std::endl;
  ASSERT_EQ(jwt.get_payload(),
            "{\"exp\":1742991458,\"iat\":1742983858,\"iss\":\"centreon\"}");
  ASSERT_EQ(jwt.get_signature(), "6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ");

  ASSERT_EQ(
      jwt.get_iat().time_since_epoch().count(),
      system_clock::time_point(seconds(1742983858)).time_since_epoch().count());
  ASSERT_EQ(jwt.get_exp(), system_clock::time_point(seconds(1742991458)));
}

// Test: ConstructorEmpty
// Purpose: Ensure that constructing a JWT with an empty token throws a msg_fmt
// exception with message "empty jwt token".
TEST(TestJWT, ConstructorEmpty) {
  try {
    // This should throw msg_fmt("empty jwt token")
    jwt j("");

    // If we get here, no exception was thrown:
    FAIL() << "Expected msg_fmt with message but no exception was thrown.";
  } catch (const msg_fmt& ex) {
    // Verify that the exception's message is correct
    EXPECT_STREQ("empty jwt token", ex.what());
  } catch (...) {
    FAIL() << "Expected msg_fmt but got some other exception type.";
  }
}

// Test: ConstructorInvalidFormat
// Purpose: Validate that a JWT token with an invalid format (missing dot)
// triggers a msg_fmt exception.
TEST(TestJWT, ConstructorInvalidFormat) {
  try {
    jwt j(
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJleHAiOjE3NDI5OTE0NTgsImlhdCI6MTc0Mjk4Mzg1OCwiaXNzIjoiY2VudHJlb24ifQ"
        "6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ");  // missing dot

    // If we get here, no exception was thrown:
    FAIL() << "Expected msg_fmt with message but no exception was thrown.";
  } catch (const msg_fmt& ex) {
    // Verify that the exception's message is correct
    EXPECT_STREQ("Invalid token format", ex.what());
  } catch (...) {
    FAIL() << "Expected msg_fmt but got some other exception type.";
  }
}

// Test: ConstructorInvalidbase64
// Purpose: Verify that a JWT token with an invalid Base64 segment throws a
// msg_fmt exception with a specific error message.
TEST(TestJWT, ConstructorInvalidbase64) {
  try {
    // This should throw msg_fmt("Invalid base64 format")
    const std::string metadata =
        "6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ."
        "eyJleHAiOjE3NDI5OTE0NTgsImlhdCI6MTc0Mjk4Mzg1OCwiaXNzIjoiY2VudHJlb24ifQ"
        "."
        "6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ";
    jwt j(metadata);

    // If we get here, no exception was thrown:
    FAIL() << "Expected msg_fmt with message but no exception was thrown.";
  } catch (const msg_fmt& ex) {
    // Verify that the exception's message is correct
    EXPECT_STREQ(
        "This string '6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ' contains "
        "characters not legal in a base64 encoded string.",
        ex.what());
  } catch (...) {
    FAIL() << "Expected msg_fmt but got some other exception type.";
  }
}

// Test: ConstructorNoJson
// Purpose: Check that a JWT token with a non-JSON header throws an exception
// indicating the JSON is not correct.
TEST(TestJWT, ConstructorNoJson) {
  try {
    const std::string metadata =
        "aGVsbG8gd29ybGQgZnJvbSBjZW50cmVvbiBjb2xsZWN0."
        "eyJleHAiOjE3NDI5OTE0NTgsImlhdCI6MTc0Mjk4Mzg1OCwiaXNzIjoiY2VudHJlb24ifQ"
        "."
        "6_m-qL0qdY8-p7bHdinYAlPHPdEwNsfJ--9BUvxgCVQ";
    jwt j(metadata);  // header is not a json

    // If we get here, no exception was thrown:
    FAIL() << "Expected msg_fmt with message but no exception was thrown.";
  } catch (const msg_fmt& ex) {
    // Verify that the exception's message is correct
    EXPECT_STREQ("Error: json is not correct: Invalid value. at offset 0",
                 ex.what());
  } catch (...) {
    FAIL() << "Expected msg_fmt but got some other exception type.";
  }
}

// Test: ConstructorInvalidJsonHeader
// Purpose: Ensure that a JWT token with structurally invalid JSON in the header
// throws the appropriate exception.
TEST(TestJWT, ConstructorInvalidJsonHeader) {
  try {
    const std::string metadata =
        "eyJhbGciOiJIUzI1NiJ9."
        "eyJleHAiOjE3NDMwNjk2OTEsImlhdCI6MTc0MzA2MjA5MSwiaXNzIjoiY2VudHJlb24ifQ"
        ".E-Fd5fKKpuPMBDkFh0yEXO3CDDWekZ0HlqmbMHkzIcA";
    jwt j(metadata);

    // If we get here, no exception was thrown:
    FAIL() << "Expected msg_fmt with message but no exception was thrown.";
  } catch (const msg_fmt& ex) {
    // Verify that the exception's message is correct
    EXPECT_TRUE(
        std::string(ex.what()).find("forbidden values in jwt header: document "
                                    "doesn't respect this schema:") == 0);
  } catch (...) {
    FAIL() << "Expected msg_fmt but got some other exception type.";
  }
}

// Test: ConstructorInvalidJsonPayload
// Purpose: Verify that a JWT token with invalid JSON in the payload part raises
// an exception with an appropriate message.
TEST(TestJWT, ConstructorInvalidJsonPayload) {
  try {
    const std::string metadata =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
        ".E-Fd5fKKpuPMBDkFh0yEXO3CDDWekZ0HlqmbMHkzIcA";
    jwt j(metadata);  // header is not a json

    // If we get here, no exception was thrown:
    FAIL() << "Expected msg_fmt with message but no exception was thrown.";
  } catch (const msg_fmt& ex) {
    EXPECT_TRUE(
        std::string(ex.what()).find("forbidden values in jwt payload: document "
                                    "doesn't respect this schema:") == 0);
  } catch (...) {
    FAIL() << "Expected msg_fmt but got some other exception type.";
  }
}
