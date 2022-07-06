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

static struct option long_options[] = {{"version", no_argument, 0, 'v'},
                                       {"help", no_argument, 0, 'h'},
                                       {"port", required_argument, 0, 'p'},
                                       {"list", no_argument, 0, 'l'},
                                       {0, 0, 0, 0}};

static void usage() {
  std::cout << color_method << "Use: " << color_reset
            << "ccc [OPTIONS...] [COMMANDS]\n"
               "'ccc' uses centreon-broker or centreon-engine gRPC api "
               "to communicate with them\n"
               "\nExamples:\n"
               "  ccc -p 51001 --list       # Lists available functions "
               "from gRPC interface at port 51000\n"
               "  ccc -p 51001 GetVersion{} # Calls the GetVersion method.\n";
}

int main(int argc, char** argv) {
  int option_index = 0;
  int opt;
  int port = 0;

  bool list = false;
  bool help = false;

  while ((opt = getopt_long(argc, argv, "vhp:l", long_options,
                            &option_index)) != -1) {
    switch (opt) {
      case 'v':
        std::cout << "ccc " << CENTREON_CONNECTOR_VERSION << "\n";
        break;
      case 'h':
        help = true;
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
      default:
        std::cerr << "Unrecognized argument '" << opt << "'" << std::endl;
        exit(3);
    }
  }

  if (help && optind == argc) {
    usage();
    exit(0);
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
    if (help) {
      std::string message{clt.info_method(argv[optind])};
      std::cout << "Input message for this function:\n" << message << std::endl;
      exit(0);
    } else if (list) {
      if (optind < argc) {
        std::cerr << "\n"
                  << color_error << "Error: " << color_reset
                  << "The list argument expects no command.\n"
                  << std::endl;
        usage();
        exit(4);
      }
      auto methods{clt.methods()};

      for (auto& m : methods)
        std::cout << " * " << m << std::endl;
    } else {
      for (int i = optind; i < argc; i++) {
        absl::string_view full_cmd{argv[i]};
        size_t first = full_cmd.find_first_not_of(" \t");
        size_t last = full_cmd.find_first_of(" \t\n{(", first);
        std::string cmd;
        std::string args;
        if (last == std::string::npos)
          cmd = std::string(full_cmd);
        else {
          cmd = std::string(full_cmd.substr(first, last));
          args = std::string(full_cmd.substr(last));
        }
        std::string res = clt.call(cmd, args);
        std::cout << res << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << color_error << "Error: " << color_reset << e.what()
              << std::endl;
    exit(1);
  }

  return 0;
}
