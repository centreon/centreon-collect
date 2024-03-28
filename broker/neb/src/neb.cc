/**
 * Copyright 2009-2016, 2018-2024 Centreon
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

#include <clocale>
#include <csignal>

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/neb/callbacks.hh"
#include "com/centreon/broker/neb/instance_configuration.hh"
#include "com/centreon/engine/nebcallbacks.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

// Specify the event broker API version.
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

namespace com::centreon::broker {
std::shared_ptr<spdlog::logger> neb_logger =
    log_v2::instance().get(log_v2::NEB);
}  // namespace com::centreon::broker

extern "C" {
/**
 *  @brief Module exit point.
 *
 *  This function is called when the module gets unloaded by Nagios.
 *  It will deregister all previously registered callbacks and perform
 *  some shutdown stuff.
 *
 *  @param[in] flags  Informational flags.
 *  @param[in] reason Unload reason.
 *
 *  @return 0 on success, any other value on failure.
 */
int nebmodule_deinit(int flags, int reason) {
  (void)flags;
  (void)reason;

  try {
    // Unregister callbacks.
    neb::unregister_callbacks();

    com::centreon::broker::config::applier::deinit();
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }

  return 0;
}

/**
 *  @brief Module entry point.
 *
 *  This function is called when the module gets loaded by Nagios. It
 *  will register callbacks to catch events and perform some
 *  initialization stuff like config file parsing, thread creation,
 *  ...
 *
 *  @param[in] flags  Informational flags.
 *  @param[in] args   The argument string of the module (shall contain the
 *                    configuration file name).
 *  @param[in] handle The module handle.
 *
 *  @return 0 on success, any other value on failure.
 */
int nebmodule_init(int flags, char const* args, void* handle) {
  neb_logger = log_v2::instance().get(log_v2::NEB);

  try {
    // Save module handle and flags for future use.
    neb::gl_mod_flags = flags;
    neb::gl_mod_handle = handle;

    // Set module informations.
    neb_set_module_info(neb::gl_mod_handle, NEBMODULE_MODINFO_TITLE,
                        "Centreon Broker's cbmod");
    neb_set_module_info(neb::gl_mod_handle, NEBMODULE_MODINFO_AUTHOR,
                        "Centreon");
    neb_set_module_info(neb::gl_mod_handle, NEBMODULE_MODINFO_COPYRIGHT,
                        "Copyright 2009-2021 Centreon");
    neb_set_module_info(neb::gl_mod_handle, NEBMODULE_MODINFO_VERSION,
                        CENTREON_BROKER_VERSION);
    neb_set_module_info(neb::gl_mod_handle, NEBMODULE_MODINFO_LICENSE,
                        "ASL 2.0");
    neb_set_module_info(
        neb::gl_mod_handle, NEBMODULE_MODINFO_DESC,
        "cbmod is part of Centreon Broker and is designed to "
        "convert internal Centreon Engine events to a "
        "proper data stream that can then be parsed by Centreon "
        "Broker's cbd.");

    signal(SIGCHLD, SIG_DFL);

    // Reset locale.
    setlocale(LC_NUMERIC, "C");

    try {
      // Set configuration file.
      if (args) {
        char const* config_file("config_file=");
        size_t config_file_size(strlen(config_file));
        if (!strncmp(args, config_file, config_file_size))
          args += config_file_size;
        neb::gl_configuration_file = args;
      } else
        throw msg_fmt("main: no configuration file provided");

      // Try configuration parsing.
      com::centreon::broker::config::parser p;
      com::centreon::broker::config::state s{
          p.parse(neb::gl_configuration_file)};

      // Initialization.
      /* This is a little hack to avoid to replace the log file set by
       * centengine */
      s.mut_log_conf().allow_only_atomic_changes(true);
      com::centreon::broker::config::applier::init(s);
      //      try {
      //        log_v2::instance().apply(s.log_conf());
      //      } catch (const std::exception& e) {
      //        log_v2::instance().get(log_v2::CORE)->error("main: {}",
      //        e.what());
      //      }

      com::centreon::broker::config::applier::state::instance().apply(s);

      // Register process and log callback.
      if (s.get_bbdo_version().major_v > 2) {
        neb::gl_registered_callbacks.emplace_back(
            std::make_unique<neb::callback>(NEBCALLBACK_PROCESS_DATA,
                                            neb::gl_mod_handle,
                                            &neb::callback_pb_process));
        neb::gl_registered_callbacks.emplace_back(
            std::make_unique<neb::callback>(NEBCALLBACK_LOG_DATA,
                                            neb::gl_mod_handle,
                                            &neb::callback_pb_log));
      } else {
        neb::gl_registered_callbacks.emplace_back(
            std::make_unique<neb::callback>(NEBCALLBACK_PROCESS_DATA,
                                            neb::gl_mod_handle,
                                            &neb::callback_process));
        neb::gl_registered_callbacks.emplace_back(
            std::make_unique<neb::callback>(
                NEBCALLBACK_LOG_DATA, neb::gl_mod_handle, &neb::callback_log));
      }
    } catch (std::exception const& e) {
      log_v2::instance().get(log_v2::CORE)->error("main: {}", e.what());
      return -1;
    } catch (...) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error("main: configuration file parsing failed");
      return -1;
    }

  } catch (std::exception const& e) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->error("main: cbmod loading failed: {}", e.what());
    nebmodule_deinit(0, 0);
    return -1;
  } catch (...) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->error("main: cbmod loading failed due to an unknown exception");
    nebmodule_deinit(0, 0);
    return -1;
  }

  return 0;
}

/**
 *  @brief Reload module after configuration reload.
 *
 *  @return OK.
 */
int nebmodule_reload() {
  multiplexing::publisher p;
  if (com::centreon::broker::config::applier::state::instance()
          .get_bbdo_version()
          .major_v > 2) {
    auto ic = std::make_shared<neb::pb_instance_configuration>();
    ic->mut_obj().set_loaded(true);
    ic->mut_obj().set_poller_id(config::applier::state::instance().poller_id());
    p.write(ic);
  } else {
    std::shared_ptr<neb::instance_configuration> ic(
        new neb::instance_configuration);
    ic->loaded = true;
    ic->poller_id = config::applier::state::instance().poller_id();
    p.write(ic);
  }
  return 0;
}
}
