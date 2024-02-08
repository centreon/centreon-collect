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

using namespace com::centreon::engine;
using namespace com::centreon::exceptions;

// Module handle
static void* gl_mod_handle(NULL);

static std::string _conf_file_path;

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
  return 0;
}

/**
 * @brief apply a new configuration, if server conf has been changed, grpc
 * server is restarted
 *
 * @param new_conf
 * @param old_conf nullptr if module start
 */
void apply_new_conf(const otl_config& new_conf, const otl_config* old_conf) {
  if (!old_conf ||
      !(*new_conf.get_grpc_config() == *old_conf->get_grpc_config())) {
    if (_otl_server) {
      _otl_server->shutdown(std::chrono::seconds(30));
    }
    _otl_server = otl_server::load(
        new_conf.get_grpc_config(), [](const metric_ptr& request) {
          metric_ptr copy(request);
          neb_make_callbacks(NEBCALLBACK_OTL_METRICS, &copy);
        });
  }
  fmt::formatter< ::opentelemetry::proto::collector::metrics::v1::
                      ExportMetricsServiceRequest>::max_length_log =
      new_conf.get_max_length_grpc_log();
  fmt::formatter< ::opentelemetry::proto::collector::metrics::v1::
                      ExportMetricsServiceRequest>::json_grpc_format =
      new_conf.get_json_grpc_log();
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

  if (args) {
    const std::string_view config_file("config_file=");
    const std::string_view arg(args);
    if (!config_file.compare(0, config_file.length(), arg))
      _conf_file_path = arg.substr(config_file.length());
  } else
    throw msg_fmt("main: no configuration file provided");

  return 0;
}

/**
 *  @brief Reload module after configuration reload.
 *
 */
extern "C" int nebmodule_reload() {
  return 0;
}
