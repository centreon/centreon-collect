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

#include "client.hh"
#include <absl/strings/str_format.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <grpcpp/generic/generic_stub.h>
#include "broker/core/src/broker.grpc.pb.h"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "engine.grpc.pb.h"

using namespace com::centreon::ccc;

client::client(std::shared_ptr<grpc::Channel> channel)
    : _stub{std::make_unique<grpc::GenericStub>(channel)}, _server{CCC_NONE} {
  const ::google::protobuf::Empty e;
  com::centreon::broker::Version broker_v;
  com::centreon::engine::Version engine_v;
  grpc::ByteBuffer request_buf;

  std::array<std::string, 2> service_name{"com.centreon.broker.Broker",
                                          "com.centreon.engine.Engine"};

  for (auto& sn : service_name) {
    grpc::ClientContext context;

    bool own_buffer = false;
    grpc::Status status = grpc::GenericSerialize<grpc::ProtoBufferWriter,
                                                 google::protobuf::Message>(
        e, &request_buf, &own_buffer);
    auto resp = _stub->PrepareUnaryCall(
        &context, absl::StrFormat("/%s/GetVersion", sn), request_buf, &_cq);
    resp->StartCall();
    grpc::ByteBuffer resp_buf;
    resp->Finish(&resp_buf, &status, reinterpret_cast<void*>(1));

    google::protobuf::DynamicMessageFactory factory;

    void* tag;
    bool ok = false;
    _cq.Next(&tag, &ok);
    grpc::ProtoBufferReader reader(&resp_buf);

    if (sn == "com.centreon.broker.Broker") {
      if (broker_v.ParseFromZeroCopyStream(&reader)) {
        std::string output_str;
        google::protobuf::TextFormat::PrintToString(broker_v, &output_str);
        if (!output_str.empty()) {
          std::cerr << "Connected to a Centreon Broker "
                    << absl::StrFormat("%02d.%02d.%d gRPC server",
                                       broker_v.major(), broker_v.minor(),
                                       broker_v.patch())
                    << std::endl;
          _server = CCC_BROKER;
          break;
        }
      }
    } else if (sn == "com.centreon.engine.Engine") {
      if (engine_v.ParseFromZeroCopyStream(&reader)) {
        std::string output_str;
        google::protobuf::TextFormat::PrintToString(engine_v, &output_str);
        if (!output_str.empty()) {
          std::cerr << "Connected to a Centreon Engine "
                    << absl::StrFormat("%02d.%02d.%d gRPC server",
                                       engine_v.major(), engine_v.minor(),
                                       engine_v.patch())
                    << std::endl;
          _server = CCC_ENGINE;
          break;
        }
      }
    }
  }
  if (_server == CCC_NONE)
    throw com::centreon::exceptions::msg_fmt(
        "Cannot connect to a Centreon Broker/Engine gRPC server");
}

std::list<std::string> client::methods() const {
  std::list<std::string> retval;
  const google::protobuf::DescriptorPool* p =
      google::protobuf::DescriptorPool::generated_pool();
  const google::protobuf::ServiceDescriptor* service_descriptor;
  switch (_server) {
    case CCC_BROKER:
      service_descriptor = p->FindServiceByName("com.centreon.broker.Broker");
      break;
    case CCC_ENGINE:
      service_descriptor = p->FindServiceByName("com.centreon.engine.Engine");
      break;
    default:
      // Should not occur
      assert(1 == 0);
  }
  size_t size = service_descriptor->method_count();
  for (uint32_t i = 0; i < size; i++) {
    const google::protobuf::MethodDescriptor* method =
        service_descriptor->method(i);
    const google::protobuf::Descriptor* input_message = method->input_type();
    const google::protobuf::Descriptor* output_message = method->output_type();
    retval.emplace_back(absl::StrFormat(
        "%s%s%s(%s%s%s) -> %s%s%s", color_method, method->name(), color_reset,
        color_message, input_message->name(), color_reset, color_message,
        output_message->name(), color_reset));
  }
  return retval;
}

std::string client::call(const std::string& cmd, const std::string& args) {
  //grpc::CompletionQueue cq;
  const google::protobuf::DescriptorPool* p =
      google::protobuf::DescriptorPool::generated_pool();
  const google::protobuf::ServiceDescriptor* service_descriptor;
  std::string cmd_str;
  switch (_server) {
    case CCC_BROKER:
      service_descriptor = p->FindServiceByName("com.centreon.broker.Broker");
      cmd_str = absl::StrFormat("/com.centreon.broker.Broker/%s", cmd);
      break;
    case CCC_ENGINE:
      service_descriptor = p->FindServiceByName("com.centreon.engine.Engine");
      cmd_str = absl::StrFormat("/com.centreon.engine.Engine/%s", cmd);
      break;
    default:
      // Should not occur
      assert(1 == 0);
  }
  auto method = service_descriptor->FindMethodByName(cmd);
  if (method == nullptr)
    throw com::centreon::exceptions::msg_fmt("The command '{}' doesn't exist",
                                             cmd_str);

  const google::protobuf::Descriptor* input_desc = method->input_type();
  google::protobuf::DynamicMessageFactory factory;
  google::protobuf::Message* input_message =
      factory.GetPrototype(input_desc)->New();
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = false;
  options.case_insensitive_enum_parsing = true;
  google::protobuf::util::Status status =
      google::protobuf::util::JsonStringToMessage(args, input_message, options);

  if (status.code() != google::protobuf::util::status_internal::StatusCode::kOk)
	  throw com::centreon::exceptions::msg_fmt("Error during the execution of '{}' method: {}",
			  cmd_str, status.ToString());

  grpc::ByteBuffer request_buf;
  bool own_buffer = false;
  grpc::ClientContext context;
  grpc::Status status_res = grpc::GenericSerialize<grpc::ProtoBufferWriter,
                                                   google::protobuf::Message>(
      *input_message, &request_buf, &own_buffer);
  auto resp = _stub->PrepareUnaryCall(&context, cmd_str, request_buf, &_cq);
  resp->StartCall();
  grpc::ByteBuffer resp_buf;
  resp->Finish(&resp_buf, &status_res, reinterpret_cast<void*>(1));

  void* tag;
  bool ok = false;
  _cq.Next(&tag, &ok);
  grpc::ProtoBufferReader reader(&resp_buf);

  const google::protobuf::Descriptor* output_desc = method->output_type();
  google::protobuf::Message* output_message =
      factory.GetPrototype(output_desc)->New();

  if (output_message->ParseFromZeroCopyStream(&reader)) {
    std::string retval;
    google::protobuf::util::JsonPrintOptions json_options;
    json_options.add_whitespace = true;
    auto status = google::protobuf::util::MessageToJsonString(
        *output_message, &retval, json_options);

    return retval;
  }
  return "";
}
