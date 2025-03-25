/**
 * Copyright 2022-2024 Centreon
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

#ifndef _CCC_CLIENT_HH
#define _CCC_CLIENT_HH
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/generic_stub.h>
#include <list>

namespace com {
namespace centreon {
namespace ccc {
constexpr const char color_green[] = "\u001b[32;1m";
constexpr const char color_blue[] = "\u001b[34;1m";
constexpr const char color_reset[] = "\u001b[0m";
constexpr const char color_red[] = "\u001b[31;1m";
constexpr const char color_yellow[] = "\u001b[33;1m";
constexpr const char color_message[] = "\u001b[32;1m";
constexpr const char color_method[] = "\u001b[34;1m";
constexpr const char color_error[] = "\u001b[31;1m";

template <const char* C>
const char* color(bool enabled) {
  if (enabled)
    return C;
  else
    return "";
}

class client {
  enum type { CCC_NONE, CCC_BROKER, CCC_ENGINE };
  std::unique_ptr<grpc::GenericStub> _stub;
  type _server;
  bool _color_enabled;
  bool _always_print_primitive_fields;
  grpc::CompletionQueue _cq;

 public:
  client(std::shared_ptr<grpc::Channel> channel,
         bool color_enabled = true,
         bool _always_print_primitive_fields = false);
  std::list<std::string> methods() const;
  std::string call(const std::string& cmd, const std::string& args);
  std::string info_method(const std::string& cmd) const;
};
}  // namespace ccc
}  // namespace centreon
}  // namespace com
#endif
