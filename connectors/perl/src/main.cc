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

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/connector/perl/options.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;

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
      return EXIT_SUCCESS;
    } else if (opts.get_argument("version").get_is_set()) {
      std::cout << "Centreon Perl Connector " << CENTREON_CONNECTOR_VERSION
                << std::endl;
      return EXIT_SUCCESS;
    }
    if (opts.get_argument("test-file").get_is_set()) {
      test_file_path = opts.get_argument("test-file").get_value();
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
      log::instance().add_pid_to_log();
      log::core()->info("Centreon Perl Connector {} starting",
                        CENTREON_CONNECTOR_VERSION);

      shared_io_context io_context(std::make_shared<asio::io_context>());
      checks::shared_signal_set signal_handler(
          std::make_shared<asio::signal_set>(*io_context, SIGTERM, SIGINT,
                                             SIGPIPE));

      signal_handler->async_wait(
          [io_context](const boost::system::error_code&, int signal_number) {
            if (signal_number == SIGPIPE) {
              log::core()->info("SIGPIPE received");
              return;
            }
            log::core()->info("termination request received {}", signal_number);
            io_context->stop();
          });

      // Load Embedded Perl.
      embedded_perl::load(argc, argv, env,
                          (opts.get_argument("code").get_is_set()
                               ? opts.get_argument("code").get_value().c_str()
                               : nullptr));

      // Program policy.
      // Program policy.
      policy::create(io_context, test_file_path);

      io_context->run();
    }
  } catch (std::exception const& e) {
    log::core()->error(e.what());
  }

  // Deinitializations.
  embedded_perl::unload();

  log::core()->info("bye");

  PERL_SYS_TERM();
  return EXIT_SUCCESS;
}
