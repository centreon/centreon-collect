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
#include "jwt.hh"
#include <string_view>
#include "base64.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using com::centreon::common::rapidjson_helper;
using com::centreon::exceptions::msg_fmt;

namespace com::centreon::common::crypto {

static constexpr std::string_view _header_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "jwt header",
    "type": "object",
    "properties": {
        "alg": {
            "description": "algorithm",
            "type": "string"
        },
        "typ": {
            "description": "type",
            "type": "string"
        }
    },
    "required": [
        "typ"
    ]
}
)");

static constexpr std::string_view _payload_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "jwt payload",
    "type": "object",
    "properties": {
        "iss": {
            "description": "issuer",
            "type": "string"
        },
        "exp": {
            "description": "expiration time",
            "type": "integer"
        },
        "iat": {
            "description": "issued at",
            "type": "integer"
        }
    },
    "required": [
        "iss",
        "exp",
        "iat"
    ]
}
)");

jwt::jwt(const std::string& token)
    : _token(token),
      _exp(std::chrono::system_clock::time_point::min()),
      _iat(std::chrono::system_clock::time_point::min()) {
  // Remove the "Bearer " prefix if present
  if (_token.empty()) {
    throw msg_fmt("empty jwt token");
  }
  if (_token.rfind("Bearer ", 0) == 0) {
    _token = _token.substr(7);
  }
  std::string_view process_token(_token);
  // Split the token into its three parts (header, payload, signature) if error
  // throw an exception
  size_t pos = 0, start = 0, partIndex = 0;

  if ((pos = process_token.find('.', start)) != std::string_view::npos) {
    partIndex++;
    _header = process_token.substr(start, pos - start);
    start = pos + 1;
  }
  if ((pos = process_token.find('.', start)) != std::string_view::npos) {
    partIndex++;
    _payload = process_token.substr(start, pos - start);
    start = pos + 1;
  }
  if (partIndex == 2) {
    _signature = process_token.substr(start);
  } else {
    throw msg_fmt("Invalid token format");
  }

  // Decode the payload
  _header = base64_decode(_header);
  // Decode the payload
  _payload = base64_decode(_payload);

  // Check the validity of the json (header, payload)

  rapidjson::Document doc = rapidjson_helper::read_from_string(_header);
  rapidjson_helper header_json(doc);

  static common::json_validator validator_header(_header_schema);
  try {
    header_json.validate(validator_header);
  } catch (const std::exception& e) {
    throw msg_fmt("forbidden values in jwt header: {}", e.what());
  }

  doc = rapidjson_helper::read_from_string(_payload);
  rapidjson_helper payload_json(doc);

  static common::json_validator validator_payload(_payload_schema);
  try {
    payload_json.validate(validator_payload);
  } catch (const std::exception& e) {
    throw msg_fmt("forbidden values in jwt payload: {}", e.what());
  }

  // check the experation date used
  if (payload_json.has_member("exp")) {
    _exp_str = std::to_string(payload_json.get_uint64_t("exp"));
    _exp = std::chrono::system_clock::time_point(
        std::chrono::seconds(payload_json.get_uint64_t("exp")));
  }
  if (payload_json.has_member("iat")) {
    _iat = std::chrono::system_clock::time_point(
        std::chrono::seconds(payload_json.get_uint64_t("iat")));
  }
}
}  // namespace com::centreon::common::crypto
