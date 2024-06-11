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

#include "com/centreon/engine/commands/otel_connector.hh"
#include "com/centreon/engine/nebmods.hh"
#include "com/centreon/engine/nebmodules.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

#include "com/centreon/engine/modules/opentelemetry/open_telemetry.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

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
  open_telemetry::unload(log_v2::instance().get(log_v2::OTEL));
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

  open_telemetry::load(conf_file_path, g_io_context,
                       log_v2::instance().get(log_v2::OTEL));
  commands::otel_connector::init_all();

  return 0;
}

/**
 *  @brief Reload module after configuration reload.
 *
 */
extern "C" int nebmodule_reload() {
  open_telemetry::reload(log_v2::instance().get(log_v2::OTEL));
  return 0;
}
