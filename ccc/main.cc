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
  if (!channel) {
    std::cerr << "There is no gRPC broker/engine server available at " << url << std::endl;
    exit(3);
  }

  if (channel->GetState(true) == GRPC_CHANNEL_IDLE) {
    std::cerr << "127.0.0.1:" << port << " seems inactive." << std::endl;
    exit(4);
  }

  auto engine_c = std::make_unique<EngineRPCClient>(channel);
  auto broker_c = std::make_unique<BrokerRPCClient>(channel);
  return 0;
}
