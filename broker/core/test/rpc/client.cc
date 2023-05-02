/*
 * Copyright 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include "../src/broker.grpc.pb.h"

using namespace com::centreon::broker;

class BrokerRPCClient {
  std::unique_ptr<Broker::Stub> _stub;

 public:
  BrokerRPCClient(std::shared_ptr<grpc::Channel> channel)
      : _stub(Broker::NewStub(channel)) {}

  bool GetVersion(Version* version) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;
    grpc::Status status = _stub->GetVersion(&context, e, version);
    if (!status.ok()) {
      std::cout << "GetVersion rpc failed." << std::endl;
      return false;
    }
    return true;
  }

  bool GetSqlManagerStats(SqlManagerStats* response) {
    const SqlConnection e;
    grpc::ClientContext context;
    grpc::Status status = _stub->GetSqlManagerStats(&context, e, response);
    if (!status.ok()) {
      std::cout << "GetSqlManager rpc failed." << std::endl;
      return false;
    }
    return true;
  }

  bool GetConflictManagerStats(ConflictManagerStats* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;
    grpc::Status status = _stub->GetConflictManagerStats(&context, e, response);
    if (!status.ok()) {
      std::cout << "GetConflictManager rpc failed." << std::endl;
      return false;
    }
    return true;
  }

  bool GetMuxerStats(MuxerStats* response, const std::string& name) {
    GenericString s;
    s.set_str_arg(name);
    grpc::ClientContext context;
    grpc::Status status = _stub->GetMuxerStats(&context, s, response);
    if (!status.ok()) {
      std::cout << "GetMuxerStats rpc failed." << std::endl;
      return false;
    }
    return true;
  }
};

int main(int argc, char** argv) {
  int32_t status = 0;
  BrokerRPCClient client(grpc::CreateChannel(
      "127.0.0.1:40000", grpc::InsecureChannelCredentials()));

  if (argc < 2) {
    std::cout << "ERROR: this client must be called with a command..."
              << std::endl;
    exit(1);
  }

  else if (strcmp(argv[1], "GetVersion") == 0) {
    Version version;
    status = client.GetVersion(&version) ? 0 : 1;
    std::cout << "GetVersion: " << version.DebugString();
  }

  else if (strcmp(argv[1], "GetSqlManagerStatsValue") == 0) {
    uint32_t sz = atoi(argv[2]);
    SqlManagerStats response;
    status = client.GetSqlManagerStats(&response) ? 0 : 1;
    for (uint32_t i = 0; i < sz; ++i) {
      std::cout << response.connections().at(i).waiting_tasks() << std::endl;
    }
  } else if (strcmp(argv[1], "GetConflictManagerStats") == 0) {
    ConflictManagerStats response;
    status = client.GetConflictManagerStats(&response) ? 0 : 1;
    std::cout << "events_handled: " << response.events_handled() << std::endl;
  } else if (strcmp(argv[1], "GetMuxerStats") == 0) {
    status = 0;
    for (int16_t cpt = 2; cpt < argc; cpt++) {
      MuxerStats response;
      status += client.GetMuxerStats(&response, argv[cpt]) ? 0 : 1;
      std::cout << "name: " << argv[cpt]
                << ", queue_file: " << response.queue_file().name()
                << ", unacknowledged_events: "
                << response.unacknowledged_events() << std::endl;
    }
  } else
    std::cout << "warning : not implemented" << std::endl;

  exit(status);
}
