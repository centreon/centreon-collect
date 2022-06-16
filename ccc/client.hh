/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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
class client {
  enum type { CCC_NONE, CCC_BROKER, CCC_ENGINE };
  std::unique_ptr<grpc::GenericStub> _stub;
  type _server;
  grpc::CompletionQueue _cq;
  std::unique_ptr<grpc::ClientContext> _context;

 public:
  client(std::shared_ptr<grpc::Channel> channel);
  std::list<std::string> methods() const;
};
}  // namespace ccc
}  // namespace centreon
}  // namespace com
#endif
