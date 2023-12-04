/**
* Copyright 2011-2013 Centreon
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

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/ssh/options.hh"
#include "com/centreon/connector/ssh/policy.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::ssh;

// Should be defined by build tools.
#ifndef CENTREON_CONNECTOR_VERSION
#define CENTREON_CONNECTOR_VERSION "(development version)"
#endif  // !CENTREON_CONNECTOR_VERSION

/**
 *  Connector entry point.
 *
 *  @param[in] argc Arguments count.
 *  @param[in] argv Arguments values.
 *
 *  @return 0 on successful execution.
 */
int main(int argc, char* argv[]) {
  try {
    // Command line parsing.
    options opts;
    std::string test_file_path;
    try {
      opts.parse(argc - 1, argv + 1);
    } catch (exceptions::basic const& e) {
      std::cout << e.what() << std::endl << opts.usage() << std::endl;
      return EXIT_FAILURE;
    }
    if (opts.get_argument("help").get_is_set()) {
      std::cout << opts.help() << std::endl;
    } else if (opts.get_argument("version").get_is_set()) {
      std::cout << "Centreon SSH Connector " << CENTREON_CONNECTOR_VERSION
                << std::endl;
    } else {
      // Set logging object.
      if (opts.get_argument("log-file").get_is_set()) {
        std::string filename(opts.get_argument("log-file").get_value());
        log::instance().switch_to_file(filename);
      } else
        log::instance().switch_to_stdout();

      if (opts.get_argument("debug").get_is_set()) {
        log::instance().set_level(spdlog::level::trace);
      } else {
        log::instance().set_level(spdlog::level::info);
      }

      if (opts.get_argument("test-file").get_is_set()) {
        test_file_path = opts.get_argument("test-file").get_value();
      }
      log::core()->info("Centreon SSH Connector {} starting PID={}",
                        CENTREON_CONNECTOR_VERSION, getpid());
      // Initialize libssh2.
      log::core()->debug("initializing libssh2");
      if (libssh2_init(0))
        throw basic_error() << "libssh2 initialization failed";
      {
        char const* version(libssh2_version(LIBSSH2_VERSION_NUM));
        if (!version)
          throw basic_error()
              << "libssh2 version is too old (>= " << LIBSSH2_VERSION
              << " required)";
        log::core()->info("libssh2 version {} successfully loaded", version);
      }

      // Set termination handler.
      log::core()->debug("installing termination handler");

      shared_io_context io_context(std::make_shared<asio::io_context>());
      asio::signal_set signal_handler(*io_context, SIGTERM, SIGINT, SIGPIPE);

      signal_handler.async_wait(
          [io_context](const boost::system::error_code&, int signal_number) {
            if (signal_number == SIGPIPE) {
              log::core()->info("SIGPIPE received");
              return;
            }
            log::core()->info("termination request received");
            io_context->stop();
          });

      // Program policy.
      policy::create(io_context, test_file_path);

      io_context->run();
    }
  } catch (std::exception const& e) {
    log::core()->error("failure {}", e.what());
    return EXIT_FAILURE;
  }

  // Deinitialize libssh2.
  libssh2_exit();

  log::core()->info("bye");

  return EXIT_SUCCESS;
}
