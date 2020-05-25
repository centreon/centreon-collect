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

#include <iostream>
#include <memory>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
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

  bool GetNbrHost(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrHost(&context, e, response);
	
	if (!status.ok()) {
	  std::cout << "GetNbrHost rpc engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrContact(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrContact(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrContact rpc engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrService(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrService(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrService rpc engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrServiceGroup(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrServiceGroup(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrServiceGroup rpc engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrContactGroup(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrContactGroup(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrContactGroup rpc engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrHostGroup(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrHostGroup(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrHostGroup rpc engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrServiceDependencies(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrServiceDependencies(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrServiceDependencies engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool GetNbrHostDependencies(GenericValue* response) {
    const ::google::protobuf::Empty e;
	grpc::ClientContext context;

	grpc::Status status = _stub->GetNbrHostDependencies(&context, e, response);

	if (!status.ok()) {
	  std::cout << "GetNbrHostDependencies engine failed" << std::endl;
	  return false;
	}

	return true;
  }

  bool ProcessServiceCheckResult(Check const& sc) {
    grpc::ClientContext context;
    CommandSuccess response;
    grpc::Status status = _stub->ProcessServiceCheckResult(&context, sc, &response);
    if (!status.ok()) {
      std::cout << "ProcessServiceCheckResult failed." << std::endl;
      return false;
    }
    return true;
  }

  bool ProcessHostCheckResult(Check const& hc) {
    grpc::ClientContext context;
    CommandSuccess response;
    grpc::Status status = _stub->ProcessHostCheckResult(&context, hc, &response);
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
  }
  else if (strcmp(argv[1], "GetStats") == 0) {
    Stats stats;
    status = client.GetStats(&stats) ? 0 : 2;
    std::cout << "GetStats: " << stats.DebugString();
  }
  else if (strcmp(argv[1], "ProcessServiceCheckResult") == 0) {
    Check sc;
    sc.set_host_name(argv[2]);
    sc.set_svc_desc(argv[3]);
    sc.set_code(std::stol(argv[4]));
    sc.set_output("Test external command");
    status = client.ProcessServiceCheckResult(sc) ? 0 : 3;
    std::cout << "ProcessServiceCheckResult: " << status << std::endl;
  }
  else if (strcmp(argv[1], "ProcessHostCheckResult") == 0) {
    Check hc;
    hc.set_host_name(argv[2]);
    hc.set_code(std::stol(argv[3]));
    hc.set_output("Test external command");
    status = client.ProcessHostCheckResult(hc) ? 0 : 4;
    std::cout << "ProcessHostCheckResult: " << status << std::endl;
  }
  else if (strcmp(argv[1], "NewThresholdsFile") == 0) {
    ThresholdsFile tf;
    tf.set_filename(argv[2]);
    status = client.NewThresholdsFile(tf) ? 0 : 5;
    std::cout << "NewThresholdsFile: " << status << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrHost") == 0) {
	GenericValue response;
	status = client.GetNbrHost(&response) ? 0 : 1;
	std::cout << "GetNbrHost from client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrContact") == 0) {
	GenericValue response;
	status = client.GetNbrContact(&response) ? 0 : 1;
	std::cout << "GetNbrHost from client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrService") == 0) {
	GenericValue response;
	status = client.GetNbrService(&response) ? 0 : 1;
	std::cout << "GetNbrService from client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrServiceGroup") == 0) {
	GenericValue response;
	status = client.GetNbrServiceGroup(&response) ? 0 : 1;
	std::cout << "GetNbrServiceGroup from client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrContactGroup") == 0) {
	GenericValue response;
	status = client.GetNbrContactGroup(&response) ? 0 : 1;
	std::cout << "GetNbrContactGroup from client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrHostGroup") == 0) {
	GenericValue response;
	status = client.GetNbrHostGroup(&response) ? 0 : 1;
	std::cout << "GetNbrHostGroup from client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrServiceDependencies") == 0) {
	GenericValue response;
	status = client.GetNbrServiceDependencies(&response) ? 0 : 1;
	std::cout << "GetNbrServiceDependencies client" << std::endl;
	std::cout << response.value() << std::endl;
  }
  else if (strcmp(argv[1], "GetNbrHostDependencies") == 0) {
	GenericValue response;
	status = client.GetNbrHostDependencies(&response) ? 0 : 1;
	std::cout << "GetNbrHostDependencies client" << std::endl;
	std::cout << response.value() << std::endl;
  }

  exit(status);
}
