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

#include <absl/strings/ascii.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <absl/strings/string_view.h>
#include <getopt.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <grpcpp/generic/generic_stub.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include "client.hh"

using namespace nlohmann;
using namespace com::centreon::ccc;

static struct option long_options[] = {
    {"version", no_argument, 0, 'v'},    {"help", no_argument, 0, 'h'},
    {"port", required_argument, 0, 'p'}, {"list", no_argument, 0, 'l'},
    {"exec", required_argument, 0, 'e'}, {0, 0, 0, 0}};
int main(int argc, char** argv) {
  int option_index = 0;
  int opt;
  int port = 0;

  bool list = false;
  std::string full_cmd;

  while ((opt = getopt_long(argc, argv, "vhp:le:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
      case 'v':
        std::cout << "ccc " << CENTREON_CONNECTOR_VERSION << "\n";
        break;
      case 'h':
        std::cout << "Use: ccc [OPTION...]\n"
                     "'ccc' uses centreon-broker or centreon-engine gRPC api "
                     "to communicate with them\n"
                     "\nExamples:\n"
                     "  ccc -p 51000 --list      # Lists available functions "
                     "from gRPC interface at port 51000\n";
        break;
      case 'p':
        if (!absl::SimpleAtoi(optarg, &port)) {
          std::cerr << "The option -p expects a port number (ie a positive "
                       "integer)\n";
          exit(1);
        }
        break;
      case 'l':
        list = true;
        break;
      case 'e':
        full_cmd = optarg;
        break;
      default:
        std::cerr << "Unrecognized argument '" << opt << "'" << std::endl;
        exit(3);
    }
  }

  if (port == 0) {
    std::cerr << "You must specify a port for the connection to the gRPC server"
              << std::endl;
    exit(2);
  }
  std::string url{absl::StrFormat("127.0.0.1:%d", port)};
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel(url, grpc::InsecureChannelCredentials());

  try {
    client clt(channel);
    if (list) {
      auto methods{clt.methods()};

      for (auto& m : methods)
        std::cout << " * " << m << std::endl;
    } else if (!full_cmd.empty()) {
      auto ret{clt.call(full_cmd)};
      std::cout << ret << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(1);
  }

  //  std::unique_ptr<grpc::GenericStub> stub =
  //  std::make_unique<grpc::GenericStub>(channel); grpc::CompletionQueue cq;
  //  auto context = std::make_unique<grpc::ClientContext>();
  //
  //  com::centreon::broker::Version v_broker;
  //  com::centreon::engine::Version v_engine;
  //
  //  std::string service_str{"com.centreon.broker.Broker"};
  //  const ::google::protobuf::Empty e;
  //  grpc::ByteBuffer request_buf;
  //  bool own_buffer = false;
  //  grpc::Status status = grpc::GenericSerialize<grpc::ProtoBufferWriter,
  //    google::protobuf::Message>(e, &request_buf, &own_buffer);
  //  auto resp = stub->PrepareUnaryCall(context.get(),
  //  absl::StrFormat("/%s/GetVersion", service_str), request_buf, &cq);
  //  resp->StartCall();
  //  grpc::ByteBuffer resp_buf;
  //  resp->Finish(&resp_buf, &status, reinterpret_cast<void*>(1));
  //
  //  const google::protobuf::DescriptorPool* p =
  //  google::protobuf::DescriptorPool::generated_pool(); const
  //  google::protobuf::ServiceDescriptor* service_descriptor =
  //  p->FindServiceByName(service_str); const
  //  google::protobuf::MethodDescriptor* method =
  //  service_descriptor->FindMethodByName("GetVersion");
  ////  const google::protobuf::Descriptor* input_desc = method->input_type();
  //  const google::protobuf::Descriptor* output_desc = method->output_type();
  ////
  ////  std::cout << "input name: " << input_desc->name() << std::endl;
  //  std::cout << "output name: " << output_desc->name() << std::endl;
  ////
  //  google::protobuf::DynamicMessageFactory factory;
  ////  google::protobuf::Message* input_message =
  /// factory.GetPrototype(input_desc)->New();
  //
  //  void *tag;
  //  bool ok = false;
  //  cq.Next(&tag, &ok);
  //  std::cout << "Execution ok ? " << ok << std::endl;
  //  grpc::ProtoBufferReader reader(&resp_buf);
  //  //com::centreon::broker::Version version;
  //
  //  google::protobuf::Message* output_message =
  //  factory.GetPrototype(output_desc)->New();
  //
  //  output_message->ParseFromZeroCopyStream(&reader);
  //  std::string v_str;
  //  if (google::protobuf::TextFormat::PrintToString(*output_message, &v_str))
  //    if (!v_str.empty())
  //      std::cout << "Result: " << v_str << std::endl;

  ///////////////////////////////////////

  //  auto stub_e = std::make_unique<Engine::Stub>(channel);
  //
  //  com::centreon::broker::Version version_b;
  //  const ::google::protobuf::Empty e;
  //  auto context = std::make_unique<grpc::ClientContext>();
  //  grpc::Status status = stub_e->GetVersion(context.get(), e, &version_e);
  //
  //  std::unique_ptr<Broker::Stub> stub_b;
  //  if (!status.ok()) {
  //    context = std::make_unique<grpc::ClientContext>();
  //    stub_e.reset();
  //    channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
  //    stub_b = std::make_unique<Broker::Stub>(channel);
  //    status = stub_b->GetVersion(context.get(), e, &version_b);
  //    if (!status.ok()) {
  //      std::cerr << "Broker GetVersion rpc failed." << std::endl;
  //    }
  //  }
  //
  //  std::string name;
  //
  //  if (stub_e) {
  //    name = "com.centreon.engine.Engine";
  //  }
  //  else if (stub_b) {
  //    name = "com.centreon.broker.Broker";
  //  }
  //  else {
  //    std::cerr << "No connection established." << std::endl;
  //    exit(3);
  //  }
  //
  //  if (list) {
  //    const google::protobuf::DescriptorPool* p =
  //    google::protobuf::DescriptorPool::generated_pool(); const
  //    google::protobuf::ServiceDescriptor* service_descriptor =
  //    p->FindServiceByName(name); size_t size =
  //    service_descriptor->method_count(); for (uint32_t i = 0; i < size; i++)
  //    {
  //      const google::protobuf::MethodDescriptor* method =
  //      service_descriptor->method(i); std::cout << "* " << method->name() <<
  //      std::endl; const google::protobuf::Descriptor* message =
  //      method->input_type(); std::cout << "  - input: " << message->name() <<
  //      std::endl; for (int j = 0; j < message->field_count(); j++) {
  //        auto f = message->field(j);
  //        switch (f->type()) {
  //      case google::protobuf::FieldDescriptor::TYPE_BOOL:
  //        std::cout << "    " << f->name() << ": boolean" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
  //        std::cout << "    " << f->name() << ": double" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_INT32:
  //        std::cout << "    " << f->name() << ": int32" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_UINT32:
  //        std::cout << "    " << f->name() << ": uint32" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_INT64:
  //        std::cout << "    " << f->name() << ": int64" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_UINT64:
  //        std::cout << "    " << f->name() << ": uint64" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_ENUM:
  //        std::cout << "    " << f->name() << ": enum" << std::endl;
  //        break;
  //      case google::protobuf::FieldDescriptor::TYPE_STRING:
  //        std::cout << "    " << f->name() << ": string" << std::endl;
  //        break;
  //      default:
  //        std::cout << "    " << f->name() << ": unknown" << std::endl;
  //        break;
  //        }
  //      }
  //      std::cout << std::endl;
  //    }
  //  }
  //  else if (!full_cmd.empty()) {
  //    //caller my_caller(channel);
  //    size_t pos = full_cmd.find("{");
  //    absl::string_view cmd, args;
  //    if (pos == std::string::npos) {
  //      cmd = full_cmd;
  //      std::cout << "command: " << cmd << std::endl;
  //
  //      //stub_b->PrepareCall(context.get(), cmd);
  //    }
  //    else {
  //      cmd = absl::string_view(full_cmd.c_str(), pos);
  //      cmd =
  //      absl::StripLeadingAsciiWhitespace(absl::StripTrailingAsciiWhitespace(cmd));
  //      std::cout << "command: " << cmd << std::endl;
  //      args = absl::string_view(full_cmd.c_str() + pos, full_cmd.size() -
  //      pos); std::cout << "args: " << args << std::endl; try {
  //        json json_doc = json::parse(args);
  //      } catch (const json::parse_error& e) {
  //        std::cerr << "Error while parsing the command arguments '" << args
  //          << "': " << e.what() << std::endl;
  //        exit(4);
  //      }
  //    }
  //  }

  return 0;
}
