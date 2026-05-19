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

#include "com/centreon/connector/perl/config.hh"
#include "connectors/perl/src/perl_connector.pb.h"

#include <EXTERN.h>
#include <perl.h>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

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
    config conf(argc, argv);

    // Set logging object.
    if (!conf.log_file_path().empty()) {
      log::instance().switch_to_file(conf.log_file_path());
    } else
      log::instance().switch_to_stdout();

    log::instance().set_level(conf.log_level());
    log::instance().add_pid_to_log();
    log::core()->info("Centreon Perl Connector {} starting",
                      CENTREON_CONNECTOR_VERSION);

    if (conf.need_to_stop()) {
      return 0;
    }

    shared_io_context io_context(std::make_shared<asio::io_context>());
    sigignore(SIGPIPE);
    // Program policy.
    policy::create(io_context, log::core(), conf);

    io_context->run();

  } catch (const std::exception& e) {
    std::cerr << "fail to start connector" << e.what() << std::endl;
  }

  log::core()->info("bye");

  PERL_SYS_TERM();
  return EXIT_SUCCESS;
}
