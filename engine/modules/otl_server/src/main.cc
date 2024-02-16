/**
 * Copyright 4 Centreon
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

#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/nebmods.hh"
#include "com/centreon/engine/nebmodules.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#include "com/centreon/engine/modules/otl_server/opentelemetry.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::modules::otl_server;
using namespace com::centreon::exceptions;

// Specify the event broker API version.
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

// Module handle
static void* gl_mod_handle(NULL);

extern std::shared_ptr<asio::io_context> g_io_context;

/**************************************
 *                                     *
 *         Exported Functions          *
 *                                     *
 **************************************/

/**
 *  @brief Module exit point.
 *
 *  This function is called when the module gets unloaded by Centreon-Engine.
 *  It will deregister all previously registered callbacks and perform
 *  some shutdown stuff.
 *
 *  @param[in] flags  Unused.
 *  @param[in] reason Unused.
 *
 *  @return 0 on success, any other value on failure.
 */
extern "C" int nebmodule_deinit(int /*flags*/, int /*reason*/) {
  open_telemetry::unload();
  return 0;
}

/**
 *  @brief Module entry point.
 *
 *  This function is called when the module gets loaded by Centreon-Engine. It
 *  will register callbacks to catch events and perform some initialization
 *  stuff like config file parsing, thread creation, ...
 *
 *  @param[in] flags  Unused.
 *  @param[in] args   Unused.
 *  @param[in] handle The module handle.
 *
 *  @return 0 on success, any other value on failure.
 */
extern "C" int nebmodule_init(int flags, char const* args, void* handle) {
  (void)args;
  (void)flags;

  // Save module handle for future use.
  gl_mod_handle = handle;

  // Set module informations.
  neb_set_module_info(gl_mod_handle, NEBMODULE_MODINFO_TITLE,
                      "Centreon-Engine's OpenTelemetry module");
  neb_set_module_info(gl_mod_handle, NEBMODULE_MODINFO_AUTHOR, "Merethis");
  neb_set_module_info(gl_mod_handle, NEBMODULE_MODINFO_COPYRIGHT,
                      "Copyright 2024 Centreon");
  neb_set_module_info(gl_mod_handle, NEBMODULE_MODINFO_VERSION, "1.0.0");
  neb_set_module_info(gl_mod_handle, NEBMODULE_MODINFO_LICENSE,
                      "GPL version 2");
  neb_set_module_info(
      gl_mod_handle, NEBMODULE_MODINFO_DESC,
      "Centreon-Engine's open telemetry grpc server. It provides grpc otl "
      "server and inject incomming metrics in centreon system.");

  std::string_view conf_file_path;
  if (args) {
    const std::string_view config_file("config_file=");
    const std::string_view arg(args);
    if (!config_file.compare(0, config_file.length(), arg)) {
      conf_file_path = arg.substr(config_file.length());
    } else {
      conf_file_path = arg;
    }
  } else
    throw msg_fmt("main: no configuration file provided");

  open_telemetry::load(conf_file_path, *g_io_context);

  return 0;
}

/**
 *  @brief Reload module after configuration reload.
 *
 */
extern "C" int nebmodule_reload() {
  open_telemetry::reload();
  return 0;
}
