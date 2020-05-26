/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
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
#include <iostream>
#include <memory>
#include "engine.grpc.pb.h"

using namespace com::centreon::engine;

class EngineRPCClient {
  std::unique_ptr<Engine::Stub> _stub;

 public:
  EngineRPCClient(std::shared_ptr<grpc::Channel> channel)
      : _stub(Engine::NewStub(channel)) {}

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

  bool GetStats(Stats* stats) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;
    grpc::Status status = _stub->GetStats(&context, e, stats);
    if (!status.ok()) {
      std::cout << "GetStats rpc failed." << std::endl;
      return false;
    }
    return true;
  }

  bool GetHost(std::string* req, EngineHost* response) {
    HostIdentifier request;
    request.set_allocated_host_name(req);
	grpc::ClientContext context;

    grpc::Status status = _stub->GetHost(&context, request, response);

    if (!status.ok()) {
      std::cout << "GetHostsCount rpc engine failed" << std::endl;
      return false;
    }


	  return true;
  }

  bool GetHostsCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status = _stub->GetHostsCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetHostsCount rpc engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetContactsCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status = _stub->GetContactsCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetContactsCount rpc engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetServicesCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status = _stub->GetServicesCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetServicesCount rpc engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetServiceGroupsCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status = _stub->GetServiceGroupsCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetServiceGroupsCount rpc engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetContactGroupsCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status = _stub->GetContactGroupsCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetContactGroupsCount rpc engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetHostGroupsCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status = _stub->GetHostGroupsCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetHostGroupsCount rpc engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetServiceDependenciesCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status =
        _stub->GetServiceDependenciesCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetServiceDependenciesCount engine failed" << std::endl;
      return false;
    }

    return true;
  }

  bool GetHostDependenciesCount(GenericValue* response) {
    const ::google::protobuf::Empty e;
    grpc::ClientContext context;

    grpc::Status status =
        _stub->GetHostDependenciesCount(&context, e, response);

    if (!status.ok()) {
      std::cout << "GetHostDependenciesCount engine failed" << std::endl;
      return false;
    }

    return true;
 }

  bool ProcessServiceCheckResult(Check const& sc) {
    grpc::ClientContext context;
    CommandSuccess response;
    grpc::Status status =
        _stub->ProcessServiceCheckResult(&context, sc, &response);
    if (!status.ok()) {
      std::cout << "ProcessServiceCheckResult failed." << std::endl;
      return false;
    }
    return true;
  }

  bool ProcessHostCheckResult(Check const& hc) {
    grpc::ClientContext context;
    CommandSuccess response;
    grpc::Status status =
        _stub->ProcessHostCheckResult(&context, hc, &response);
    if (!status.ok()) {
      std::cout << "ProcessHostCheckResult failed." << std::endl;
      return false;
    }
    return true;
  }

  bool NewThresholdsFile(const ThresholdsFile& tf) {
    grpc::ClientContext context;
    CommandSuccess response;
    grpc::Status status = _stub->NewThresholdsFile(&context, tf, &response);
    if (!status.ok()) {
      std::cout << "NewThresholdsFile failed." << std::endl;
      return false;
    }
    return true;
  }
};

int main(int argc, char** argv) {
  int32_t status;
  EngineRPCClient client(grpc::CreateChannel(
      "127.0.0.1:40001", grpc::InsecureChannelCredentials()));

  if (argc < 2) {
    std::cout << "ERROR: this client must be called with a command..."
              << std::endl;
    exit(1);
  }

  if (strcmp(argv[1], "GetVersion") == 0) {
    Version version;
    status = client.GetVersion(&version) ? 0 : 1;
    std::cout << "GetVersion: " << version.DebugString();
  } else if (strcmp(argv[1], "GetStats") == 0) {
    Stats stats;
    status = client.GetStats(&stats) ? 0 : 2;
    std::cout << "GetStats: " << stats.DebugString();
  } else if (strcmp(argv[1], "ProcessServiceCheckResult") == 0) {
    Check sc;
    sc.set_host_name(argv[2]);
    sc.set_svc_desc(argv[3]);
    sc.set_code(std::stol(argv[4]));
    sc.set_output("Test external command");
    status = client.ProcessServiceCheckResult(sc) ? 0 : 3;
    std::cout << "ProcessServiceCheckResult: " << status << std::endl;
  } else if (strcmp(argv[1], "ProcessHostCheckResult") == 0) {
    Check hc;
    hc.set_host_name(argv[2]);
    hc.set_code(std::stol(argv[3]));
    hc.set_output("Test external command");
    status = client.ProcessHostCheckResult(hc) ? 0 : 4;
    std::cout << "ProcessHostCheckResult: " << status << std::endl;
  } else if (strcmp(argv[1], "NewThresholdsFile") == 0) {
    ThresholdsFile tf;
    tf.set_filename(argv[2]);
    status = client.NewThresholdsFile(tf) ? 0 : 5;
    std::cout << "NewThresholdsFile: " << status << std::endl;
  } else if (strcmp(argv[1], "GetHostsCount") == 0) {
    GenericValue response;
    status = client.GetHostsCount(&response) ? 0 : 1;
    std::cout << "GetHostsCount from client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetContactsCount") == 0) {
    GenericValue response;
    status = client.GetContactsCount(&response) ? 0 : 1;
    std::cout << "GetContactsCount from client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetServicesCount") == 0) {
    GenericValue response;
    status = client.GetServicesCount(&response) ? 0 : 1;
    std::cout << "GetServicesCount from client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetServiceGroupsCount") == 0) {
    GenericValue response;
    status = client.GetServiceGroupsCount(&response) ? 0 : 1;
    std::cout << "GetServiceGroupsCount from client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetContactGroupsCount") == 0) {
    GenericValue response;
    status = client.GetContactGroupsCount(&response) ? 0 : 1;
    std::cout << "GetContactGroupsCount from client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetHostGroupsCount") == 0) {
    GenericValue response;
    status = client.GetHostGroupsCount(&response) ? 0 : 1;
    std::cout << "GetHostGroupsCount from client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetServiceDependenciesCount") == 0) {
    GenericValue response;
    status = client.GetServiceDependenciesCount(&response) ? 0 : 1;
    std::cout << "GetServiceDependenciesCount client" << std::endl;
    std::cout << response.value() << std::endl;
  } else if (strcmp(argv[1], "GetHostDependenciesCount") == 0) {
    GenericValue response;
    status = client.GetHostDependenciesCount(&response) ? 0 : 1;
    std::cout << "GetHostDependenciesCount client" << std::endl;
    std::cout << response.value() << std::endl;
  }
  exit(status);
}
