#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <iostream>
#include <getopt.h>
#include "broker_client.hh"
#include "engine_client.hh"

static struct option long_options[] = {{"version", no_argument, 0, 'v'},
                                       {"help", no_argument, 0, 'h'},
                                       {"port", no_argument, 0, 'p'},
                                       {"list", no_argument, 0, 'l'},
                                       {0, 0, 0, 0}};
int main(int argc, char** argv) {
  int option_index = 0;
  int opt;
  int port = 0;

  bool list = false;

  while ((opt = getopt_long(argc, argv, "vhp:l", long_options, &option_index)) != -1) {
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
    }
  }

  if (port == 0) {
    std::cerr << "You must specify a port for the connection to the gRPC server" << std::endl;
    exit(2);
  }
  std::string url{absl::StrFormat("127.0.0.1:%d", port)};
  auto channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
  auto stub_e = std::make_unique<Engine::Stub>(channel);

  com::centreon::engine::Version version_e;
  com::centreon::broker::Version version_b;
  const ::google::protobuf::Empty e;
  auto context = std::make_unique<grpc::ClientContext>();
  grpc::Status status = stub_e->GetVersion(context.get(), e, &version_e);

  std::unique_ptr<Broker::Stub> stub_b;
  if (!status.ok()) {
    context = std::make_unique<grpc::ClientContext>();
    stub_e.reset();
    channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
    stub_b = std::make_unique<Broker::Stub>(channel);
    status = stub_b->GetVersion(context.get(), e, &version_b);
    if (!status.ok()) {
      std::cerr << "Broker GetVersion rpc failed." << std::endl;
    }
  }

  std::string name;

  if (stub_e) {
    name = "com.centreon.engine.Engine";
  }

  if (stub_b) {
    name = "com.centreon.broker.Broker";
  }

  if (list) {
    const google::protobuf::DescriptorPool* p = google::protobuf::DescriptorPool::generated_pool();
    const google::protobuf::ServiceDescriptor* serviceDescriptor = p->FindServiceByName(name);
    size_t size = serviceDescriptor->method_count();
    for (uint32_t i = 0; i < size; i++) {
      const google::protobuf::MethodDescriptor* method = serviceDescriptor->method(i);
      std::cout << "* " << method->name() << std::endl;
      const google::protobuf::Descriptor* message = method->input_type();
      std::cout << "  - input: " << message->name() << std::endl;
      for (int j = 0; j < message->field_count(); j++) {
        auto f = message->field(j);
        switch (f->type()) {
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
        std::cout << "    " << f->name() << ": boolean" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
        std::cout << "    " << f->name() << ": double" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
        std::cout << "    " << f->name() << ": int32" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
        std::cout << "    " << f->name() << ": uint32" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
        std::cout << "    " << f->name() << ": int64" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
        std::cout << "    " << f->name() << ": uint64" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
        std::cout << "    " << f->name() << ": enum" << std::endl;
        break;
      case google::protobuf::FieldDescriptor::TYPE_STRING:
        std::cout << "    " << f->name() << ": string" << std::endl;
        break;
      default:
        std::cout << "    " << f->name() << ": unknown" << std::endl;
        break;
        }
      }
      std::cout << std::endl;
    }
  }

  return 0;
}
