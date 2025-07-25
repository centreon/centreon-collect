/**
 * Copyright 2009-2013,2018-2024 Centreon
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

#include <cerrno>
#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>
#include "bbdo/common.pb.h"

#include <absl/synchronization/mutex.h>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/numbers.h>

#include <boost/asio.hpp>

namespace asio = boost::asio;
using namespace com::centreon;

// with this define boost::interprocess doesn't need Boost.DataTime
#define BOOST_DATE_TIME_NO_LIB 1
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include "com/centreon/broker/brokerrpc.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/misc/diagnostic.hh"
#include "com/centreon/common/pool.hh"
#include "common/crypto/aes256.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using log_v2 = common::log_v2::log_v2;

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

std::shared_ptr<asio::io_context> g_io_context =
    std::make_shared<asio::io_context>();

std::shared_ptr<boost::asio::signal_set> signals =
    std::make_shared<boost::asio::signal_set>(*g_io_context, SIGHUP, SIGTERM);

std::shared_ptr<com::centreon::common::crypto::aes256> credentials_decrypt;

constexpr std::string_view _engine_context_path =
    "/etc/centreon-engine/engine-context.json";

// Main config file.
static std::vector<std::string> gl_mainconfigfiles;
static config::state gl_state;
static std::atomic_bool gl_term{false};

static struct option long_options[] = {{"pool_size", required_argument, 0, 's'},
                                       {"check", no_argument, 0, 'c'},
                                       {"diagnose", no_argument, 0, 'D'},
                                       {"version", no_argument, 0, 'v'},
                                       {"help", no_argument, 0, 'h'},
                                       {0, 0, 0, 0}};

/**
 * @brief handle signals SIGHUP and SIGTERM
 *
 * @param err
 * @param signal_number
 */
static void signal_handler(const std::shared_ptr<spdlog::logger>& core_logger,
                           const boost::system::error_code& err,
                           int signal_number) {
  if (signal_number == SIGTERM) {
    SPDLOG_LOGGER_INFO(core_logger, "main: SIGTERM received by process {}",
                       getpid());
    gl_term = true;
    return;  // no restart a signals->async_wait
  } else if (signal_number == SIGHUP) {
    // Log message.
    SPDLOG_LOGGER_INFO(core_logger, "main: configuration update requested");

    try {
      // Parse configuration file.
      config::parser parsr;
      config::state conf{parsr.parse(gl_mainconfigfiles.front())};
      auto& log_conf = conf.mut_log_conf();
      log_conf.allow_only_atomic_changes(true);
      try {
        log_v2::instance().apply(log_conf);
        /* We update the logger, since the conf has been applied */
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(core_logger, "problem while reloading cbd: {}",
                            e.what());
        SPDLOG_LOGGER_ERROR(core_logger, "problem while reloading cbd: {}",
                            e.what());
      }

      try {
        if (std::filesystem::is_regular_file(_engine_context_path) &&
            std::filesystem::file_size(_engine_context_path) > 0) {
          std::unique_ptr<com::centreon::common::crypto::aes256> new_file =
              std::make_unique<com::centreon::common::crypto::aes256>(
                  _engine_context_path);
          // we test validity of keys
          std::string encrypted = new_file->encrypt("test encrypt");
          if (new_file->decrypt(encrypted) == "test encrypt") {
            credentials_decrypt = std::move(new_file);
          } else {
            throw std::invalid_argument(
                "this keys are unable to crypt and decrypt a sentence");
          }
        }
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(
            core_logger,
            "credentials_encryption is set but we can not read {}: {}",
            _engine_context_path, e.what());
      }

      try {
        // Apply resulting configuration.
        config::applier::state::instance().apply(conf);

        gl_state = conf;
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(
            core_logger,
            "main: configuration update could not succeed, reloading previous "
            "configuration: {}",
            e.what());
        config::applier::state::instance().apply(gl_state);
      } catch (...) {
        SPDLOG_LOGGER_ERROR(
            core_logger,
            "main: configuration update could not succeed, reloading previous "
            "configuration");
        config::applier::state::instance().apply(gl_state);
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_INFO(core_logger, "main: configuration update failed: {}",
                         e.what());
    } catch (...) {
      SPDLOG_LOGGER_INFO(
          core_logger, "main: configuration update failed: unknown exception");
    }
  } else {
    SPDLOG_LOGGER_ERROR(core_logger,
                        "main: unimplemented signal handler for signal: {}",
                        signal_number);
  }
  signals->async_wait(
      [save_signalset = signals, core_logger](
          const boost::system::error_code& err, int signal_number) {
        signal_handler(core_logger, err, signal_number);
      });
}

/**
 *  @brief Program entry point.
 *
 *  main() is the first function called when the program starts.
 *
 *  @param[in] argc Number of arguments received on the command line.
 *  @param[in] argv Arguments received on the command line, stored in an
 *                  array.
 *
 *  @return 0 on normal termination, any other value on failure.
 */
int main(int argc, char* argv[]) {
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
  absl::EnableMutexInvariantDebugging(true);
  // Initialization.
  int opt, option_index = 0, n_thread = 0;
  std::string broker_name{"unknown"};
  uint16_t default_port{51000};
  std::string default_listen_address{"localhost"};

  log_v2::load("cbd");
  auto core_logger = log_v2::instance().get(log_v2::CORE);
  com::centreon::common::pool::load(g_io_context, core_logger);

  // Set signal handler.
  signals->async_wait(
      [save_signalset = signals, core_logger](
          const boost::system::error_code& err, int signal_number) {
        signal_handler(core_logger, err, signal_number);
      });

  // Return value.
  int retval(0);

  try {
    // Check the command line.
    bool check{false};
    bool diagnose{false};
    bool help{false};
    bool version{false};

    while ((opt = getopt_long(argc, argv, "s:cDvh", long_options,
                              &option_index)) != -1) {
      switch (opt) {
        case 's':
          if (!absl::SimpleAtoi(optarg, &n_thread)) {
            throw msg_fmt("The option -s expects a positive integer");
          }
          break;
        case 'c':
          check = true;
          break;
        case 'D':
          diagnose = true;
          break;
        case 'h':
          help = true;
          break;
        case 'v':
          version = true;
          break;
        default:
          throw msg_fmt(
              "Enter allowed options : [-s <poolsize>] [-c] [-D] [-h] [-v]");
          break;
      }
    }
    if (optind < argc)
      while (optind < argc)
        gl_mainconfigfiles.push_back(argv[optind++]);

    // Check parameters requirements.
    if (diagnose) {
      if (gl_mainconfigfiles.empty()) {
        SPDLOG_LOGGER_ERROR(
            core_logger,
            "diagnostic: no configuration file provided: DIAGNOSTIC FILE MIGHT "
            "NOT BE USEFUL");
      }
      misc::diagnostic diag;
      diag.generate(gl_mainconfigfiles);
    } else if (help) {
      SPDLOG_LOGGER_INFO(
          core_logger,
          "USAGE: {} [-s <poolsize>] [-c] [-D] [-h] [-v] [<configfile>]",
          argv[0]);

      SPDLOG_LOGGER_INFO(core_logger,
                         "  '-s<poolsize>'  Set poolsize threads.");
      SPDLOG_LOGGER_INFO(core_logger, "  '-c'  Check configuration file.");
      SPDLOG_LOGGER_INFO(core_logger, "  '-D'  Generate a diagnostic file.");
      SPDLOG_LOGGER_INFO(core_logger, "  '-h'  Print this help.");
      SPDLOG_LOGGER_INFO(core_logger, "  '-v'  Print Centreon Broker version.");
      SPDLOG_LOGGER_INFO(core_logger,
                         "Centreon Broker " CENTREON_BROKER_VERSION);
      SPDLOG_LOGGER_INFO(core_logger,
                         "Copyright 2009-" CENTREON_CURRENT_YEAR " Centreon");
      SPDLOG_LOGGER_INFO(
          core_logger,
          "License ASL 2.0 <http://www.apache.org/licenses/LICENSE-2.0>");
      retval = 0;
    } else if (version) {
      SPDLOG_LOGGER_INFO(core_logger, "Centreon Broker {}",
                         CENTREON_BROKER_VERSION);
      retval = 0;
    } else if (gl_mainconfigfiles.empty()) {
      SPDLOG_LOGGER_ERROR(
          core_logger,
          "USAGE: {} [-s <poolsize>] [-c] [-D] [-h] [-v] [<configfile>]\n\n",
          argv[0]);
      return 1;
    } else {
      SPDLOG_LOGGER_INFO(core_logger, "Centreon Broker {}",
                         CENTREON_BROKER_VERSION);
      SPDLOG_LOGGER_INFO(core_logger, "Copyright 2009-2021 Centreon");
      SPDLOG_LOGGER_INFO(
          core_logger,
          "License ASL 2.0 <http://www.apache.org/licenses/LICENSE-2.0>");

      // Reset locale.
      setlocale(LC_NUMERIC, "C");

      {
        // Parse configuration file.
        config::parser parsr;
        config::state conf{parsr.parse(gl_mainconfigfiles.front())};
        auto& log_conf = conf.log_conf();
        /* It is important to apply the log conf before broker threads start.
         * Otherwise we will have issues with concurrent accesses. */
        try {
          log_v2::instance().apply(log_conf);
        } catch (const std::exception& e) {
          SPDLOG_LOGGER_ERROR(core_logger, "{}", e.what());
        }

        SPDLOG_LOGGER_INFO(core_logger, "main: process {} pid:{} begin",
                           argv[0], getpid());

        if (n_thread > 0 && n_thread < 100)
          conf.pool_size(n_thread);
        config::applier::init(common::BROKER, conf);

        // Apply resulting configuration totally or partially.
        config::applier::state::instance().apply(conf, !check);
        broker_name = conf.broker_name();
        gl_state = conf;
      }

      if (!gl_state.listen_address().empty())
        default_listen_address = gl_state.listen_address();

      if (gl_state.rpc_port() == 0)
        default_port += gl_state.broker_id();
      else
        default_port = gl_state.rpc_port();
      std::unique_ptr<brokerrpc, std::function<void(brokerrpc*)> > rpc(
          new brokerrpc(default_listen_address, default_port, broker_name),
          [](brokerrpc* rpc) {
            rpc->shutdown();
            delete rpc;
          });

      // Launch event loop.
      retval = EXIT_SUCCESS;
      if (!check) {
        while (!gl_term) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        SPDLOG_LOGGER_INFO(core_logger,
                           "main: termination request received by process {}",
                           getpid());
      }
      //  Unload endpoints.
      config::applier::deinit();
      cache::global_cache::unload();
    }
  }
  // Standard exception.
  catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(core_logger, "Error during cbd exit: {}", e.what());
    retval = EXIT_FAILURE;
  }
  // Unknown exception.
  catch (...) {
    SPDLOG_LOGGER_ERROR(core_logger, "Error general during cbd exit");
    retval = EXIT_FAILURE;
  }

  SPDLOG_LOGGER_INFO(core_logger, "main: process {} pid:{} end exit_code:{}",
                     argv[0], getpid(), retval);
  g_io_context->stop();
  com::centreon::common::pool::unload();
  log_v2::unload();
  return retval;
}
