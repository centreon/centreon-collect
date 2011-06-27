/*
** Copyright 2009-2011 Merethis
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/logging/ostream.hh"
#include "com/centreon/broker/neb/callbacks.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "nagios/common.h"
#include "nagios/nebcallbacks.h"

using namespace com::centreon::broker;

/**************************************
*                                     *
*           Global Objects            *
*                                     *
**************************************/

// Specify the event broker API version.
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

// Configuration file name.
QString neb::gl_configuration_file;

// Instance information.
unsigned int neb::instance_id;
QString      neb::instance_name;

// List of host IDs.
std::map<std::string, int> neb::gl_hosts;

// List of service IDs.
std::map<std::pair<std::string, std::string>, std::pair<int, int> > neb::gl_services;

// Sender object.
multiplexing::publisher neb::gl_publisher;

/**************************************
*                                     *
*           Static Objects            *
*                                     *
**************************************/

// List of callbacks
static struct {
  unsigned int macro;
  int (* callback)(int, void*);
  bool registered;
} gl_callbacks[] = {
  { NEBCALLBACK_ACKNOWLEDGEMENT_DATA, &neb::callback_acknowledgement, false },
  { NEBCALLBACK_COMMENT_DATA, &neb::callback_comment, false },
  { NEBCALLBACK_DOWNTIME_DATA, &neb::callback_downtime, false },
  { NEBCALLBACK_EVENT_HANDLER_DATA, &neb::callback_event_handler, false },
  { NEBCALLBACK_FLAPPING_DATA, &neb::callback_flapping_status, false },
  { NEBCALLBACK_HOST_CHECK_DATA, &neb::callback_host_check, false },
  { NEBCALLBACK_HOST_STATUS_DATA, &neb::callback_host_status, false },
  { NEBCALLBACK_LOG_DATA, &neb::callback_log, false },
  { NEBCALLBACK_PROCESS_DATA, &neb::callback_process, false },
  { NEBCALLBACK_PROGRAM_STATUS_DATA, &neb::callback_program_status, false },
  { NEBCALLBACK_SERVICE_CHECK_DATA, &neb::callback_service_check, false },
  { NEBCALLBACK_SERVICE_STATUS_DATA, &neb::callback_service_status, false }
};

// Module handle
static void*   gl_mod_handle = NULL;

/**************************************
*                                     *
*          Static Functions           *
*                                     *
**************************************/

/**
 *  @brief Deregister callbacks.
 *
 *  Deregister all callbacks previously registered.
 */
static void deregister_callbacks() {
  for (unsigned int i = 0;
       i < sizeof(gl_callbacks) / sizeof(*gl_callbacks);
       ++i)
    if (gl_callbacks[i].registered) {
      neb_deregister_callback(gl_callbacks[i].macro,
        gl_callbacks[i].callback);
      gl_callbacks[i].registered = false;
    }
  return ;
}

/**************************************
*                                     *
*         Exported Functions          *
*                                     *
**************************************/

extern "C" {
  /**
   *  @brief Module exit point.
   *
   *  This function is called when the module gets unloaded by Nagios.
   *  It will deregister all previously registered callbacks and perform
   *  some shutdown stuff.
   *
   *  @param[in] flags  XXX
   *  @param[in] reason XXX
   *
   *  @return 0 on success, any other value on failure.
   */
  int nebmodule_deinit(int flags, int reason) {
    (void)flags;
    (void)reason;
    try {
      deregister_callbacks();
    }
    // Avoid exception propagation in C code.
    catch (...) {}

    return (0);
  }

  /**
   *  @brief Module entry point.
   *
   *  This function is called when the module gets loaded by Nagios. It
   *  will register callbacks to catch events and perform some
   *  initialization stuff like config file parsing, thread creation,
   *  ...
   *
   *  @param[in] flags  XXX
   *  @param[in] args   The argument string of the module (shall contain the
   *                    configuration file name).
   *  @param[in] handle The module handle.
   *
   *  @return 0 on success, any other value on failure.
   */
  int nebmodule_init(int flags, char const* args, void* handle) {
    (void)flags;

    // Save module handle for future use.
    gl_mod_handle = handle;

    // Set module informations.
    neb_set_module_info(gl_mod_handle,
      NEBMODULE_MODINFO_TITLE,
      "CentreonBroker's cbmod");
    neb_set_module_info(gl_mod_handle,
      NEBMODULE_MODINFO_AUTHOR,
      "Merethis");
    neb_set_module_info(gl_mod_handle,
      NEBMODULE_MODINFO_COPYRIGHT,
      "Copyright 2009-2011 Merethis");
    neb_set_module_info(gl_mod_handle,
      NEBMODULE_MODINFO_VERSION,
      "2.0.0");
    neb_set_module_info(gl_mod_handle,
      NEBMODULE_MODINFO_LICENSE,
      "GPL version 2");
    neb_set_module_info(gl_mod_handle,
      NEBMODULE_MODINFO_DESC,
      "cbmod is part of CentreonBroker and is designed to " \
      "convert internal Nagios events to a proper data "    \
      "stream that can then be parsed by CentreonBroker's " \
      "cb2db.");

    // Disable timestamp printing in logs (cause starvation when forking).
    logging::ostream::with_timestamp(false);

    try {
      // Set configuration file.
      if (args)
        neb::gl_configuration_file = args;
      else
        throw (exceptions::msg()
                 << "main: no configuration file provided");

      // Try configuration parsing.
      config::parser p;
      config::state s;
      p.parse(neb::gl_configuration_file, s);
    }
    catch (std::exception const& e) {
      logging::config << logging::HIGH << e.what();
      return (-1);
    }
    catch (...) {
      logging::config << logging::HIGH
                      << "main: configuration file parsing failed";
      return (-1);
    }

    // Register callbacks.
    for (unsigned int i = 0;
         i < sizeof(gl_callbacks) / sizeof(*gl_callbacks);
         ++i)
      if (neb_register_callback(gl_callbacks[i].macro,
            gl_mod_handle,
            0,
            gl_callbacks[i].callback) != NDO_OK) {
        deregister_callbacks();
        return (-1);
      }
      else
        gl_callbacks[i].registered = true;

    return (0);
  }
}
