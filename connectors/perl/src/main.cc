/**
 * Copyright 2022 Centreon
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

#include <boost/program_options.hpp>

#include "connectors/perl/src/perl_connector.pb.h"

#include <EXTERN.h>
#include <perl.h>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;
namespace po = boost::program_options;

// Should be defined by build tools.
#ifndef CENTREON_CONNECTOR_VERSION
#define CENTREON_CONNECTOR_VERSION "(development version)"
#endif  // !CENTREON_CONNECTOR_VERSION

/**
 *  Program entry point.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Environment.
 *
 *  @return 0 on successful execution.
 */
int main(int argc, char** argv, char** env) {
  PERL_SYS_INIT3(&argc, &argv, &env);

  try {
    // Command line parsing.
    // Process all command line arguments.
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
      ("help,h", "Print help and exit")
      ("code,c", po::value<std::string>(), "Argument is some Perl code that will be executed by the embedded interpreter.")
      ("debug,d","If this flag is specified, print all logs messages.")
      ("version,v","Print software version and exit.")
      ("log-file,l", po::value<std::string>(),"Specifies the log file (default: stderr).")
      ("test-file,x", po::value<std::string>(),"Specifies the file used instead of stdin.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
      
    std::string test_file_path;

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    } else if (vm.count("version")) {
      std::cout << "Centreon Perl Connector " << CENTREON_CONNECTOR_VERSION
                << std::endl;
      return EXIT_SUCCESS;
    }
    if (vm.count("test-file")) {
      test_file_path = vm["test-file"].as<std::string>();
    }
    // Set logging object.
    if (vm.count("log-file")) {
      std::string filename = vm["log-file"].as<std::string>();
      log::instance().switch_to_file(filename);
    } else
      log::instance().switch_to_stdout();

    if (vm.count("debug")) {
      log::instance().set_level(spdlog::level::trace);
    } else {
      log::instance().set_level(spdlog::level::info);
    }
    log::instance().add_pid_to_log();
    log::core()->info("Centreon Perl Connector {} starting",
                      CENTREON_CONNECTOR_VERSION);

    shared_io_context io_context(std::make_shared<asio::io_context>());
    // checks::shared_signal_set signal_handler(std::make_shared<asio::signal_set>(
    //     *io_context, SIGTERM, SIGINT, SIGPIPE));

    // signal_handler->async_wait(
    //     [io_context](const boost::system::error_code&, int signal_number) {
    //       if (signal_number == SIGPIPE) {
    //         log::core()->info("SIGPIPE received");
    //         return;
    //       }
    //       log::core()->info("termination request received {}", signal_number);
    //       io_context->stop();
    //     });

    // // Load Embedded Perl.
    // embedded_perl::load(argc, argv, env,
    //                     (vm.count("code")
    //                          ? vm["code"].as<std::string>().c_str()
    //                          : nullptr));

    // Program policy.
    // Program policy.
    policy::create(io_context, vm.count("code")
                              ? vm["code"].as<std::string>():"",test_file_path);

    io_context->run();

  } catch (const std::exception& e) {
    std::cerr << "fail to start connector" << e.what() << std::endl;
  }

  // Deinitializations.
  //embedded_perl::unload();

  log::core()->info("bye");

  PERL_SYS_TERM();
  return EXIT_SUCCESS;
}
