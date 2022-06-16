#include "client.hh"
#include <absl/strings/str_format.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include "broker/core/src/broker.grpc.pb.h"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "engine.grpc.pb.h"

using namespace com::centreon::ccc;

constexpr const char color_method[] = "\u001b[32;1m";
constexpr const char color_message[] = "\u001b[34;1m";
constexpr const char color_reset[] = "\u001b[0m";
client::client(std::shared_ptr<grpc::Channel> channel)
    : _stub{std::make_unique<grpc::GenericStub>(channel)}, _server{CCC_NONE} {
  const ::google::protobuf::Empty e;
  com::centreon::broker::Version broker_v;
  com::centreon::engine::Version engine_v;
  grpc::ByteBuffer request_buf;

  std::array<std::string, 2> service_name{"com.centreon.broker.Broker",
                                          "com.centreon.engine.Engine"};

  for (auto& sn : service_name) {
    _context = std::make_unique<grpc::ClientContext>();

    bool own_buffer = false;
    grpc::Status status = grpc::GenericSerialize<grpc::ProtoBufferWriter,
                                                 google::protobuf::Message>(
        e, &request_buf, &own_buffer);
    auto resp = _stub->PrepareUnaryCall(_context.get(),
                                        absl::StrFormat("/%s/GetVersion", sn),
                                        request_buf, &_cq);
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
          std::cout << "Connected to a Broker "
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
          std::cout << "Connected to an Engine "
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
        "Cannot connect to a Broker/Engine gRPC server");
}

std::list<std::string> client::methods() const {
  std::list<std::string> retval;
  const google::protobuf::DescriptorPool* p =
      google::protobuf::DescriptorPool::generated_pool();
  const google::protobuf::ServiceDescriptor* serviceDescriptor;
  switch (_server) {
    case CCC_BROKER:
      serviceDescriptor = p->FindServiceByName("com.centreon.broker.Broker");
      break;
    case CCC_ENGINE:
      serviceDescriptor = p->FindServiceByName("com.centreon.engine.Engine");
      break;
    default:
      // Should not occur
      assert(1 == 0);
  }
  size_t size = serviceDescriptor->method_count();
  for (uint32_t i = 0; i < size; i++) {
    const google::protobuf::MethodDescriptor* method =
        serviceDescriptor->method(i);
    const google::protobuf::Descriptor* input_message = method->input_type();
    const google::protobuf::Descriptor* output_message = method->output_type();
    retval.emplace_back(absl::StrFormat(
        "%s%s%s(%s%s%s) -> %s%s%s", color_method, method->name(), color_reset,
        color_message, input_message->name(), color_reset, color_message,
        output_message->name(), color_reset));
  }
  return retval;
}
