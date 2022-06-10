#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <iostream>
#include <getopt.h>
#include "broker_client.hh"
#include "engine_client.hh"

static struct option long_options[] = {{"version", no_argument, 0, 'v'},
                                       {"help", no_argument, 0, 'h'},
                                       {"port", no_argument, 0, 'p'},
                                       {0, 0, 0, 0}};
int main(int argc, char** argv) {
  int option_index = 0;
  int opt;
  int port = 0;

  while ((opt = getopt_long(argc, argv, "vhp:", long_options, &option_index)) != -1) {
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
    }
  }

  if (port == 0) {
    std::cerr << "You must specify a port for the connection to the gRPC server" << std::endl;
    exit(2);
  }
  std::string url{absl::StrFormat("127.0.0.1:%d", port)};
  auto channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
  auto stub_e = std::make_unique <Engine::Stub>(channel);

  com::centreon::engine::Version version_e;
  com::centreon::broker::Version version_b;
  const ::google::protobuf::Empty e;
  grpc::ClientContext context;
  grpc::Status status = stub_e->GetVersion(&context, e, &version_e);
  if (!status.ok()) {
    std::cerr << "Engine GetVersion rpc failed." << std::endl;
  }

  channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
  auto stub_b = std::make_unique <Broker::Stub>(channel);
  status = stub_b->GetVersion(&context, e, &version_b);
  if (!status.ok()) {
    std::cerr << "Broker GetVersion rpc failed." << std::endl;
  }
//  auto broker_c = std::make_unique<BrokerRPCClient>(channel);
//  if (channel->GetState(true) == GRPC_CHANNEL_IDLE) {
//    std::cerr << "127.0.0.1:" << port << " broker seems inactive." << std::endl;
//    exit(4);
//  }

  return 0;
}
