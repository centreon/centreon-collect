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

#ifndef CCB_GRPC_ACCEPTOR_HH
#define CCB_GRPC_ACCEPTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "grpc_config.hh"

CCB_BEGIN()

namespace grpc {

class server;

class acceptor : public io::endpoint {
  std::shared_ptr<server> _grpc_instance;

 public:
  acceptor(const grpc_config::pointer& conf);
  ~acceptor();

  acceptor(const acceptor&) = delete;
  acceptor& operator=(const acceptor&) = delete;

  std::shared_ptr<io::stream> open() override;
  std::shared_ptr<io::stream> open(const system_clock::time_point& dead_line);
  std::shared_ptr<io::stream> open(const system_clock::duration& delay) {
    return open(system_clock::now() + delay);
  }
  bool is_ready() const override;
};
};  // namespace grpc
CCB_END()

#endif  // !CCB_GRPC_ACCEPTOR_HH
