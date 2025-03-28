/**
 * Copyright 1999-2009 Ethan Galstad
 * Copyright 2009-2010 Nagios Core Development Team and Community Contributors
 * Copyright 2011-2025 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <random>

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>

namespace asio = boost::asio;
namespace po = boost::program_options;

#include <spdlog/fmt/ostr.h>

#include <absl/container/btree_map.h>

#include <boost/circular_buffer.hpp>
#include <boost/container/flat_map.hpp>

#include "com/centreon/common/pool.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/broker/loader.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/logging.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/extended_conf.hh"
#include "com/centreon/engine/diagnostic.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/enginerpc.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging.hh"
#include "com/centreon/engine/logging/broker.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros/misc.hh"
#include "com/centreon/engine/retention/dump.hh"
#include "com/centreon/engine/retention/parser.hh"
#include "com/centreon/engine/retention/state.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/version.hh"
#include "com/centreon/logging/engine.hh"
#include "common/engine_conf/parser.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine;
using com::centreon::broker::neb::cbmod;
using com::centreon::common::log_v2::log_v2;

std::shared_ptr<asio::io_context> g_io_context(
    std::make_shared<asio::io_context>());

/**
 *  Centreon Engine entry point.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main(int argc, char* argv[]) {
  // Get global macros.
  nagios_macros* mac(get_global_macros());

  // Load singletons and global variable.
  log_v2::load("centengine");

  // Initialize the initial configuration state.
  {
    auto cfg = std::make_unique<configuration::State>();
    configuration::state_helper state_hlp(cfg.get());
    pb_indexed_config.set_state(std::move(cfg));
  }
  /* It's time to set the logger. Later, we will have access from multiple
   * threads and we'll only be able to change loggers atomic values. */
  // init pb_config to default values
  {
    auto pb_config = std::make_unique<configuration::State>();
    configuration::state_helper state_hlp(pb_config.get());
    configuration::indexed_state pb_indexed_config(std::move(pb_config));
  }

  init_loggers();
  configuration::applier::logging::instance();
  com::centreon::common::pool::load(g_io_context, runtime_logger);

  logging::broker backend_broker_log;

  int retval = EXIT_FAILURE;
  try {
    // Options.
    bool error = false;
    bool diagnose = false;
    std::string broker_config;
    std::vector<std::string> extended_conf_file;

    // Process all command line arguments.
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
      ("help,h", "Print help message")
      ("version,V", "Print software version and license")
      ("verify-config,v", "Verify all configuration data")
      ("test-scheduling,s", "Show projected/recommended check scheduling and other diagnostic info"
       " based on the current configuration files")
      ("dont-verify-paths,x",
       "Don't check for circular object paths - USE WITH CAUTION!")
      ("diagnose,D", "Generate a diagnostic file")
      ("broker-config,b", po::value<std::string>()->value_name("module_file"),
       "Broker configuration file")
      ("extended-config,c",
       po::value<std::vector<std::string>>()->value_name("config-file"),
       "Extended configuration file")
      ("proto-conf,p", po::value<std::string>()->value_name("proto_dir"),
       "Directory containing the protocol buffer configuration files")
      ("log-file,l", po::value<std::string>()->value_name("log-file"),
       "Full path to the log file name")
      ("config-file,f", po::value<std::string>()->value_name("cfg_file"),
        "Main configuration file");

    // clang-format on
    po::positional_options_description p;
    p.add("config-file", -1);

    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(p).run(),
        vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] << " [options] cfg_file" << std::endl;
      std::cout << desc << std::endl;
      retval = EXIT_SUCCESS;
    } else if (vm.count("version")) {
      std::cout
          << "Centreon Engine " CENTREON_ENGINE_VERSION_STRING
             "\n"
             "\n"
             "Copyright 1999-2009 Ethan Galstad\n"
             "Copyright 2009-2010 Nagios Core Development Team and Community "
             "Contributors\n"
             "Copyright 2011-" CENTREON_CURRENT_YEAR
             " Centreon\n"
             "\n"
             "This program is free software: you can redistribute it and/or\n"
             "modify it under the terms of the GNU General Public License "
             "version 2\n"
             "as published by the Free Software Foundation.\n"
             "\n"
             "Centreon Engine is distributed in the hope that it will be "
             "useful,\n"
             "but WITHOUT ANY WARRANTY; without even the implied warranty "
             "of\n"
             "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the "
             "GNU\n"
             "General Public License for more details.\n"
             "\n"
             "You should have received a copy of the GNU General Public "
             "License\n"
             "along with this program. If not, see\n"
             "<http://www.gnu.org/licenses/>.\n";

      retval = EXIT_SUCCESS;
    } else {
      if (vm.count("verify-config"))
        verify_config = true;
      if (vm.count("test-scheduling"))
        test_scheduling = true;
      if (vm.count("dont-verify-paths"))
        verify_circular_paths = false;
      if (vm.count("diagnose"))
        diagnose = true;
      if (vm.count("proto-conf"))
        proto_conf = vm["proto-conf"].as<std::string>();
      if (vm.count("broker-config"))
        broker_config = vm["broker-config"].as<std::string>();
      if (vm.count("extended-config"))
        extended_conf_file =
            vm["extended-config"].as<std::vector<std::string>>();
      // Config file is last argument specified.
      if (vm.count("config-file"))
        config_file = vm["config-file"].as<std::string>();

      // Make sure the config file uses an absolute path.
      if (config_file[0] != '/') {
        // Get absolute path of current working directory.
        std::string buffer{
            fmt::format("{}/{}", std::string{std::filesystem::current_path()},
                        config_file)};
        config_file = std::move(buffer);
      }

      // Reset umask.
      umask(S_IWGRP | S_IWOTH);

      // Checker init
      checks::checker::init();

      // If an error occured, print usage.
      if (error) {
        std::cout << "Usage: " << argv[0] << " [options] cfg_file" << std::endl;
        std::cout << desc << std::endl;
        retval = EXIT_FAILURE;
      }
      // We're just verifying the configuration.
      else if (verify_config) {
        try {
          // Read in the configuration files (main config file,
          // resource and object config files).
          configuration::error_cnt err;
          cbm = std::make_unique<cbmod>(proto_conf);
          auto pb_cfg = std::make_unique<configuration::State>();
          configuration::state_helper state_hlp(pb_cfg.get());
          configuration::indexed_state indexed_config(std::move(pb_cfg));
          configuration::State& pb_config = indexed_config.mut_state();
          {
            configuration::parser p;
            p.parse(config_file, &pb_config, err);
            if (broker_config.empty())
              broker_config = pb_config.broker_module_cfg_file();
            state_hlp.expand(err);
          }
          configuration::applier::state::instance().apply(indexed_config, err);
          std::cout << "\n Checked " << commands::command::commands.size()
                    << " commands.\n Checked "
                    << commands::connector::connectors.size()
                    << " connectors.\n Checked " << contact::contacts.size()
                    << " contacts.\n Checked "
                    << hostdependency::hostdependencies.size()
                    << " host dependencies.\n Checked "
                    << hostescalation::hostescalations.size()
                    << " host escalations.\n Checked "
                    << hostgroup::hostgroups.size()
                    << " host groups.\n Checked " << host::hosts.size()
                    << " hosts.\n Checked "
                    << servicedependency::servicedependencies.size()
                    << " service dependencies.\n Checked "
                    << serviceescalation::serviceescalations.size()
                    << " service escalations.\n Checked "
                    << servicegroup::servicegroups.size()
                    << " service groups.\n Checked " << service::services.size()
                    << " services.\n Checked " << timeperiod::timeperiods.size()
                    << " time periods.\n\n Total Warnings: "
                    << err.config_warnings
                    << "\n Total Errors:   " << err.config_errors << std::endl;
          retval = err.config_errors ? EXIT_FAILURE : EXIT_SUCCESS;
        } catch (const std::exception& e) {
          std::cout << "Error while processing a config file: " << e.what()
                    << std::endl;

          std::cout
              << "One or more problems occurred while processing the config "
                 "files.\n "
                 "Check your configuration file(s) to ensure that they contain "
                 "valid directives and data definitions.\nIf you are upgrading "
                 "from "
                 "a previous version of Centreon Engine, you should be aware "
                 "that "
                 "some variables/definitions may have been removed or modified "
                 "in "
                 "this version.\n Make sure to read the documentation "
                 "regarding "
                 "the "
                 "config files, as well as the version changelog to find out "
                 "what "
                 "has changed.\n";
        }
      }
      // We're just testing scheduling.
      else if (test_scheduling) {
        try {
          // Parse configuration.
          auto pb_cfg = std::make_unique<configuration::State>();
          configuration::state_helper state_hlp(pb_cfg.get());
          configuration::indexed_state indexed_config(std::move(pb_cfg));
          configuration::State& pb_config = indexed_config.mut_state();
          configuration::error_cnt err;
          {
            configuration::parser p;
            p.parse(config_file, &pb_config, err);
            state_hlp.expand(err);
          }

          // Parse retention.
          retention::state state;
          if (!pb_config.state_retention_file().empty()) {
            retention::parser p;
            try {
              p.parse(pb_config.state_retention_file(), state);
            } catch (std::exception const& e) {
              std::cout << "Error while parsing the retention: {}" << e.what()
                        << std::endl;
            }
          }

          // Apply configuration.
          configuration::applier::state::instance().apply(indexed_config, err,
                                                          &state);

          display_scheduling_info();
          retval = EXIT_SUCCESS;
        } catch (std::exception const& e) {
          std::cout << e.what() << std::endl;
        }
      }
      // Diagnostic.
      else if (diagnose) {
        diagnostic diag;
        diag.generate(config_file);
      }
      // Else start to monitor things.
      else {
        auto generate_port = [] {
          std::random_device rd;  // Will be used to obtain a seed for the
                                  // random number engine
          std::mt19937 gen(
              rd());  // Standard mersenne_twister_engine seeded with rd()
          std::uniform_int_distribution<uint16_t> dis(50000, 50999);

          uint16_t port = dis(gen);
          return port;
        };

        try {
          // Parse configuration.
          configuration::error_cnt err;
          auto new_conf = std::make_unique<configuration::State>();
          std::filesystem::path proto_conf_file(proto_conf / "state.prot");
          if (!proto_conf.empty()) {
            std::ifstream ifs(proto_conf_file);
            if (ifs.good()) {
              new_conf->ParseFromIstream(&ifs);
              ifs.close();
            }
          } else {
            configuration::state_helper state_hlp(new_conf.get());
            configuration::parser p;
            p.parse(config_file, new_conf.get(), err);
            configuration::error_cnt err;
            state_hlp.expand(err);
          }
          configuration::indexed_state indexed_config(std::move(new_conf));
          configuration::extended_conf::load_all(extended_conf_file.begin(),
                                                 extended_conf_file.end());

          configuration::extended_conf::update_state(
              &indexed_config.mut_state());
          if (broker_config.empty())
            broker_config = indexed_config.broker_module_cfg_file();
          uint16_t port = indexed_config.mut_state().grpc_port();

          if (broker_config.empty()) {
            std::cerr << "No module configuration file provided in the Engine "
                         "configuration file."
                      << std::endl;
            exit(EXIT_FAILURE);
          }
          if (!port)
            port = generate_port();

          const std::string& listen_address =
              indexed_config.mut_state().rpc_listen_address();

          std::unique_ptr<enginerpc, std::function<void(enginerpc*)>> rpc(
              new enginerpc(listen_address, port), [](enginerpc* rpc) {
                rpc->shutdown();
                delete rpc;
              });

          // Parse retention.
          retention::state state;
          {
            retention::parser p;
            try {
              p.parse(indexed_config.mut_state().state_retention_file(), state);
            } catch (const std::exception& e) {
              config_logger->error("{}", e.what());
              engine_logger(logging::log_config_error, logging::basic)
                  << e.what();
            }
          }

          // Get program (re)start time and save as macro. Needs to be
          // done after we read config files, as user may have overridden
          // timezone offset.
          program_start = std::time(nullptr);
          mac->x[MACRO_PROCESSSTARTTIME] = std::to_string(program_start);

          // Handle signals (interrupts).
          setup_sighandler();

          configuration::State& new_config = indexed_config.mut_state();
          // Load broker modules.
          if (vm.count("log-file"))
            new_config.set_log_file(vm["log-file"].as<std::string>());

          configuration::applier::state::instance().apply_log_config(
              new_config);

          neb_init_callback_list();

          for (auto& m : new_config.broker_module()) {
            std::pair<std::string, std::string> p =
                absl::StrSplit(m, absl::MaxSplits(' ', 1));
            broker::loader::instance().add_module(p.first, p.second);
          }

          // Add broker backend.
          com::centreon::logging::engine::instance().add(
              &backend_broker_log, logging::log_all, logging::basic);

          // Apply configuration.
          configuration::applier::state::instance().apply(indexed_config, err,
                                                          &state);

          // Initialize status data.
          initialize_status_data();

          // Initialize scheduled downtime data.
          downtimes::downtime_manager::instance().initialize_downtime_data();

          // Initialize check statistics.
          init_check_stats();

          // Update all status data (with retained information).
          update_all_status_data();

          /* We don't start cbm earlier because when we apply the
           * configuration, we also send the configuration to Broker, but the
           * initial instance will be send by broker_program_state with the
           * NEBTYPE_PROCESS_EVENTLOOPSTART flag. So, if we'd do this, we'd
           * send the configuration twice to Broker. But the first time
           * without the initial instance, which can lead to issues in the
           * database. Doing this, imply we also have to check if cbm is
           * defined in broker.cc.
           */
          cbm = std::make_unique<cbmod>(broker_config, proto_conf);
          // Send program data to broker.
          broker_program_state(NEBTYPE_PROCESS_EVENTLOOPSTART, NEBFLAG_NONE);

          // if neb has not started g_io_context we do it here
          com::centreon::common::pool::set_pool_size(1);

          // Get event start time and save as macro.
          event_start = time(NULL);
          mac->x[MACRO_EVENTSTARTTIME] = std::to_string(event_start);

          engine_logger(logging::log_info_message, logging::basic)
              << "Event loop start at " << string::ctime(event_start);
          config_logger->info("Event loop start at {}",
                              string::ctime(event_start));
          // Start monitoring all services (doesn't return until a
          // restart or shutdown signal is encountered).
          com::centreon::engine::events::loop::instance().run();

          if (sigshutdown) {
            engine_logger(logging::log_process_info, logging::basic)
                << "Caught SIG" << sigs[sig_id] << ", shutting down ...";
            SPDLOG_LOGGER_INFO(process_logger,
                               "Caught SIG {}, shutting down ...",
                               sigs[sig_id]);
          }
          // Send program data to broker.
          broker_program_state(NEBTYPE_PROCESS_EVENTLOOPEND, NEBFLAG_NONE);
          if (sigshutdown)
            broker_program_state(NEBTYPE_PROCESS_SHUTDOWN,
                                 NEBFLAG_USER_INITIATED);

          // Save service and host state information.
          retention::dump::save(
              ::pb_indexed_config.state().state_retention_file());

          // Clean up the status data.
          cleanup_status_data(true);

          // Shutdown stuff.
          if (sigshutdown) {
            engine_logger(logging::log_process_info, logging::basic)
                << "Successfully shutdown ... (PID=" << getpid() << ")";
            SPDLOG_LOGGER_INFO(process_logger,
                               "Successfully shutdown ... (PID={})", getpid());
          }

          retval = EXIT_SUCCESS;
        } catch (std::exception const& e) {
          // Log.
          engine_logger(logging::log_runtime_error, logging::basic)
              << "Error: " << e.what();
          SPDLOG_LOGGER_ERROR(process_logger, "Error: {}", e.what());
          // Send program data to broker.
          broker_program_state(NEBTYPE_PROCESS_SHUTDOWN,
                               NEBFLAG_PROCESS_INITIATED);
        }
      }
    }

    // Memory cleanup.
    cleanup();
    spdlog::shutdown();
  } catch (std::exception const& e) {
    engine_logger(logging::log_runtime_error, logging::basic)
        << "Error: " << e.what();
    SPDLOG_LOGGER_ERROR(process_logger, "Error: {}", e.what());
  }

  // Unload singletons and global objects.

  cbm.reset();
  g_io_context->stop();
  com::centreon::common::pool::unload();

  return retval;
}
