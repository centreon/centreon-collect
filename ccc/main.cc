/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <getopt.h>
#include <fstream>
#include <iostream>

#include <absl/strings/ascii.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <absl/strings/string_view.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/json/json.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <grpcpp/generic/generic_stub.h>
#include <nlohmann/json.hpp>

#include "client.hh"
#include "common/engine_conf/state.pb.h"

using namespace nlohmann;
using namespace com::centreon::ccc;

static struct option long_options[] = {{"version", no_argument, 0, 'v'},
                                       {"help", no_argument, 0, 'h'},
                                       {"port", required_argument, 0, 'p'},
                                       {"list", no_argument, 0, 'l'},
                                       {"nocolor", no_argument, 0, 'n'},
                                       {"full", no_argument, 0, 'F'},
                                       {0, 0, 0, 0}};

static void usage(bool color_enabled) {
  std::cout
      << color<color_method>(color_enabled)
      << "Use: " << color<color_reset>(color_enabled)
      << "ccc [OPTIONS...] [COMMANDS]\n"
         "'ccc' uses centreon-broker or centreon-engine gRPC api "
         "to communicate with them\n"
         "\n"
      << color<color_method>(color_enabled)
      << "Options:" << color<color_reset>(color_enabled)
      << "\n"
         "  -v, --version\n"
         "    Displays the version of ccc.\n"
         "  -h, --help [COMMAND]\n"
         "    Displays a general help or a help message on the command.\n"
         "  -p, --port <NUMBER>\n"
         "    Specifies the gRPC server port to connect to.\n"
         "  -l, --list\n"
         "    Displays the available methods.\n"
         "  -n, --nocolor\n"
         "    Outputs are displayed with the current color.\n"
         "  -F, --full\n"
         "    Forces display of fields containing default values.\n"
         "  -P, --proto <file path>:[category]\n"
         "    decode .prot file and display it in json format.\n"
         "    category can be empty for all or: engine_params_only, commands, "
         "connectors , contacts, contactgroups, hostdependencies, "
         "hostescalations, hostgroups, hosts, servicedependencies, "
         "serviceescalations, servicegroups, services, "
         "anomalydetections, timeperiods, severities, tags, user.\n"
         "\n"
      << color<color_method>(color_enabled)
      << "Examples:" << color<color_reset>(color_enabled)
      << "\n"
         "  ccc -p 51001 --list       # Lists available functions "
         "from gRPC interface at port 51000\n"
         "  ccc -p 51001 GetVersion{} # Calls the GetVersion method.\n";
}

struct content_printer {
  ::google::protobuf::json::PrintOptions print_options;

  void print(
      const com::centreon::engine::configuration::State& engine_state) const {
    std::string output;
    (void)::google::protobuf::json::MessageToJsonString(engine_state, &output,
                                                        print_options);
    std::cout << output << std::endl;
  }

  template <class mess_type>
  void print(
      const ::google::protobuf::RepeatedPtrField<mess_type>& to_print) const {
    std::cout << '[';
    if (print_options.add_whitespace)
      std::cout << std::endl;

    std::string elem_output;
    char sep = 0;
    for (const auto& elem : to_print) {
      if (sep) {
        std::cout << sep;
        if (print_options.add_whitespace)
          std::cout << std::endl;
      }

      elem_output.clear();
      (void)::google::protobuf::json::MessageToJsonString(elem, &elem_output,
                                                          print_options);
      std::cout << elem_output;
      sep = ',';
    }
    std::cout << ']';
    if (print_options.add_whitespace)
      std::cout << std::endl;
  }

  void print(
      const ::google::protobuf::Map<std::string, std::string>& to_print) const {
    std::cout << '{';
    if (print_options.add_whitespace)
      std::cout << std::endl;
    bool first = true;
    for (const auto& [key, value] : to_print) {
      if (!first) {
        std::cout << ',';
        if (print_options.add_whitespace)
          std::cout << std::endl;
      }
      if (print_options.add_whitespace)
        std::cout << "  ";
      std::cout << '"' << key << "\":\"" << value << '"';
      first = false;
    }
    if (print_options.add_whitespace)
      std::cout << std::endl;
    std::cout << '}';
  }
};

using engine_state_filter =
    std::function<void(com::centreon::engine::configuration::State&,
                       content_printer& printer)>;

#define PRINT_FIELD(field)                                               \
  {                                                                      \
    #field, [](com::centreon::engine::configuration::State& all,         \
               content_printer& printer) { printer.print(all.field()); } \
  }

std::array<std::pair<std::string_view, engine_state_filter>, 19>
    engine_state_filters = {
        {{"all", [](com::centreon::engine::configuration::State& all,
                    content_printer& printer) { printer.print(all); }},
         {"engine_params_only",
          [](com::centreon::engine::configuration::State& all,
             content_printer& printer) {
            all.mutable_commands()->Clear();
            all.mutable_connectors()->Clear();
            all.mutable_contacts()->Clear();
            all.mutable_contactgroups()->Clear();
            all.mutable_hostdependencies()->Clear();
            all.mutable_hostescalations()->Clear();
            all.mutable_hostgroups()->Clear();
            all.mutable_hosts()->Clear();
            all.mutable_servicedependencies()->Clear();
            all.mutable_serviceescalations()->Clear();
            all.mutable_servicegroups()->Clear();
            all.mutable_services()->Clear();
            all.mutable_anomalydetections()->Clear();
            all.mutable_timeperiods()->Clear();
            all.mutable_severities()->Clear();
            all.mutable_tags()->Clear();
            all.mutable_user()->clear();
            printer.print(all);
          }},
         PRINT_FIELD(commands),
         PRINT_FIELD(connectors),
         PRINT_FIELD(contacts),
         PRINT_FIELD(contactgroups),
         PRINT_FIELD(hostdependencies),
         PRINT_FIELD(hostescalations),
         PRINT_FIELD(hostgroups),
         PRINT_FIELD(hosts),
         PRINT_FIELD(servicedependencies),
         PRINT_FIELD(serviceescalations),
         PRINT_FIELD(servicegroups),
         PRINT_FIELD(services),
         PRINT_FIELD(anomalydetections),
         PRINT_FIELD(timeperiods),
         PRINT_FIELD(severities),
         PRINT_FIELD(tags),
         PRINT_FIELD(user)}};

bool decode_prot_file(const char* file_path_with_filter,
                      bool prettier_json,
                      bool always_print_primitive_fields) {
  std::string file_path(file_path_with_filter);
  std::string filter;
  if (auto sep_pos = file_path.find(':'); sep_pos != std::string::npos) {
    filter = file_path.substr(sep_pos + 1);
    file_path.resize(sep_pos);
  }
  std::ifstream f(file_path);
  if (!f) {
    std::cerr << "Can't open file " << file_path << " : " << strerror(errno)
              << std::endl;
    return false;
  }

  content_printer printer;
  printer.print_options.always_print_fields_with_no_presence =
      always_print_primitive_fields;
  if (prettier_json) {
    printer.print_options.add_whitespace = true;
  }
  // first we try to decode engine state object
  com::centreon::engine::configuration::State engine_state;
  if (engine_state.ParseFromIstream(&f)) {
    if (filter.empty()) {
      filter = "all";
    }

    auto filter_search = std::find_if(
        engine_state_filters.begin(), engine_state_filters.end(),
        [&filter](const auto& to_test) { return to_test.first == filter; });

    if (filter_search == engine_state_filters.end()) {
      std::cerr << "unknown filter:" << filter << std::endl;
      std::cerr << "allowed _filters are:";
      for (const auto& [filt, _] : engine_state_filters) {
        std::cerr << filt << ' ';
      }
      std::cerr << std::endl;
      return false;
    }
    filter_search->second(engine_state, printer);
    return true;
  }
  std::cerr << "fail to decode proto file: " << file_path << std::endl;
  return false;
}

int main(int argc, char** argv) {
  int option_index = 0;
  int opt;
  int port = 0;

  bool list = false;
  bool help = false;
  bool color_enabled = true;
  bool always_print_primitive_fields = false;
  const char* prot_file_to_decode = nullptr;

  while ((opt = getopt_long(argc, argv, "vhnp:P:lF", long_options,
                            &option_index)) != -1) {
    switch (opt) {
      case 'v':
        std::cout << "ccc " << CENTREON_CONNECTOR_VERSION << "\n";
        exit(0);
        break;
      case 'h':
        help = true;
        break;
      case 'n':
        color_enabled = false;
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
      case 'F':
        always_print_primitive_fields = true;
        break;
      case 'P':
        prot_file_to_decode = optarg;
        break;
      default:
        std::cerr << "Unrecognized argument '" << opt << "'" << std::endl;
        exit(3);
    }
  }

  if (help && optind == argc) {
    usage(color_enabled);
    exit(0);
  }

  if (prot_file_to_decode) {
    exit(decode_prot_file(prot_file_to_decode, true,
                          always_print_primitive_fields) != true);
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
    client clt(channel, color_enabled, always_print_primitive_fields);
    if (help) {
      std::string message{clt.info_method(argv[optind])};
      std::cout << "Input message for this function:\n" << message << std::endl;
      exit(0);
    } else if (list) {
      if (optind < argc) {
        std::cerr << "\n"
                  << color<color_error>(color_enabled)
                  << "Error: " << color<color_reset>(color_enabled)
                  << "The list argument expects no command.\n"
                  << std::endl;
        usage(color_enabled);
        exit(4);
      }
      auto methods{clt.methods()};

      for (auto& m : methods)
        std::cout << " * " << m << std::endl;
    } else {
      for (int i = optind; i < argc; i++) {
        std::string_view full_cmd{argv[i]};
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
    std::cerr << color<color_error>(color_enabled)
              << "Error: " << color<color_reset>(color_enabled) << e.what()
              << std::endl;
    exit(1);
  }

  return 0;
}
