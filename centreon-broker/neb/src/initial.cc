/*
** Copyright 2009-2013,2015 Merethis
**
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

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/neb/callbacks.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/neb/initial.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/nebcallbacks.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/objects.hh"

// Internal Nagios host list.
extern "C" {
  extern host* host_list;
  extern hostdependency* hostdependency_list;
  extern service* service_list;
  extern servicedependency* servicedependency_list;
}

using namespace com::centreon::broker;

// NEB module list.
extern "C" {
  nebmodule* neb_module_list;
}

/**************************************
*                                     *
*          Static Functions           *
*                                     *
**************************************/

/**
 *  Send to the global publisher the list of custom variables.
 */
static void send_custom_variables_list() {
  // Start log message.
  logging::info(logging::medium)
    << "init: beginning custom variables dump";

  // Iterate through all hosts.
  for (host* h(host_list); h; h = h->next)
    // Send all custom variables.
    for (customvariablesmember* cv(h->custom_variables);
         cv;
         cv = cv->next) {
      // Fill callback struct.
      nebstruct_custom_variable_data nscvd;
      memset(&nscvd, 0, sizeof(nscvd));
      nscvd.type = NEBTYPE_HOSTCUSTOMVARIABLE_ADD;
      nscvd.timestamp.tv_sec = time(NULL);
      nscvd.var_name = cv->variable_name;
      nscvd.var_value = cv->variable_value;
      nscvd.object_ptr = h;

      // Callback.
      neb::callback_custom_variable(
             NEBCALLBACK_CUSTOM_VARIABLE_DATA,
             &nscvd);
    }

  // Iterate through all services.
  for (service* s(service_list); s; s = s->next)
    // Send all custom variables.
    for (customvariablesmember* cv(s->custom_variables);
         cv;
         cv = cv->next) {
      // Fill callback struct.
      nebstruct_custom_variable_data nscvd;
      memset(&nscvd, 0, sizeof(nscvd));
      nscvd.type = NEBTYPE_SERVICECUSTOMVARIABLE_ADD;
      nscvd.timestamp.tv_sec = time(NULL);
      nscvd.var_name = cv->variable_name;
      nscvd.var_value = cv->variable_value;
      nscvd.object_ptr = s;

      // Callback.
      neb::callback_custom_variable(
             NEBCALLBACK_CUSTOM_VARIABLE_DATA,
             &nscvd);
    }

  // End log message.
  logging::info(logging::medium)
    << "init: end of custom variables dump";

  return ;
}

/**
 *  Send to the global publisher the list of host dependencies within Nagios.
 */
static void send_host_dependencies_list() {
  // Start log message.
  logging::info(logging::medium)
    << "init: beginning host dependencies dump";

  try {
    // Loop through all dependencies.
    for (hostdependency* hd(hostdependency_list); hd; hd = hd->next) {
      // Fill callback struct.
      nebstruct_adaptive_dependency_data nsadd;
      memset(&nsadd, 0, sizeof(nsadd));
      nsadd.type = NEBTYPE_HOSTDEPENDENCY_ADD;
      nsadd.flags = NEBFLAG_NONE;
      nsadd.attr = NEBATTR_NONE;
      nsadd.timestamp.tv_sec = time(NULL);
      nsadd.object_ptr = hd;

      // Callback.
      neb::callback_dependency(
             NEBCALLBACK_ADAPTIVE_DEPENDENCY_DATA,
             &nsadd);
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high)
      << "init: error occurred while dumping host dependencies: "
      << e.what();
  }
  catch (...) {
    logging::error(logging::high)
      << "init: unknown error occurred while dumping host dependencies";
  }

  // End log message.
  logging::info(logging::medium)
    << "init: end of host dependencies dump";

  return ;
}

/**
 *  Send to the global publisher the list of hosts within Nagios.
 */
static void send_host_list() {
  // Start log message.
  logging::info(logging::medium) << "init: beginning host dump";

  // Loop through all hosts.
  for (host* h(host_list); h; h = h->next) {
    // Fill callback struct.
    nebstruct_adaptive_host_data nsahd;
    memset(&nsahd, 0, sizeof(nsahd));
    nsahd.type = NEBTYPE_HOST_ADD;
    nsahd.command_type = CMD_NONE;
    nsahd.modified_attribute = MODATTR_ALL;
    nsahd.modified_attributes = MODATTR_ALL;
    nsahd.object_ptr = h;

    // Callback.
    neb::callback_host(NEBCALLBACK_ADAPTIVE_HOST_DATA, &nsahd);
  }

  // End log message.
  logging::info(logging::medium) << "init: end of host dump";

  return ;
}

/**
 *  Send to the global publisher the list of host parents within Nagios.
 */
static void send_host_parents_list() {
  // Start log message.
  logging::info(logging::medium) << "init: beginning host parents dump";

  try {
    // Loop through all hosts.
    for (host* h(host_list); h; h = h->next)
      // Loop through all parents.
      for (hostsmember* parent(h->parent_hosts);
           parent;
           parent = parent->next) {
        // Fill callback struct.
        nebstruct_relation_data nsrd;
        memset(&nsrd, 0, sizeof(nsrd));
        nsrd.type = NEBTYPE_PARENT_ADD;
        nsrd.flags = NEBFLAG_NONE;
        nsrd.attr = NEBATTR_NONE;
        nsrd.timestamp.tv_sec = time(NULL);
        nsrd.hst = parent->host_ptr;
        nsrd.dep_hst = h;

        // Callback.
        neb::callback_relation(NEBTYPE_PARENT_ADD, &nsrd);
      }
  }
  catch (std::exception const& e) {
    logging::error(logging::high)
      << "init: error occurred while dumping host parents: "
      << e.what();
  }
  catch (...) {
    logging::error(logging::high)
      << "init: unknown error occurred while dumping host parents";
  }

  // End log message.
  logging::info(logging::medium) << "init: end of host parents dump";

  return ;
}

/**
 *  Send to the global publisher the list of modules loaded by Engine.
 */
static void send_module_list() {
  // Start log message.
  logging::info(logging::medium)
    << "init: beginning modules dump";

  // Browse module list.
  for (nebmodule* nm(neb_module_list); nm; nm = nm->next)
    if (nm->filename) {
      // Fill callback struct.
      nebstruct_module_data nsmd;
      memset(&nsmd, 0, sizeof(nsmd));
      nsmd.module = nm->filename;
      nsmd.args = nm->args;
      nsmd.type = NEBTYPE_MODULE_ADD;

      // Callback.
      neb::callback_module(NEBTYPE_MODULE_ADD, &nsmd);
    }

  // End log message.
  logging::info(logging::medium) << "init: end of modules dump";

  return ;
}

/**
 *  Send to the global publisher the list of service dependencies within
 *  Nagios.
 */
static void send_service_dependencies_list() {
  // Start log message.
  logging::info(logging::medium)
    << "init: beginning service dependencies dump";

  try {
    // Loop through all dependencies.
    for (servicedependency* sd(servicedependency_list);
         sd;
         sd = sd->next) {
      // Fill callback struct.
      nebstruct_adaptive_dependency_data nsadd;
      memset(&nsadd, 0, sizeof(nsadd));
      nsadd.type = NEBTYPE_SERVICEDEPENDENCY_ADD;
      nsadd.flags = NEBFLAG_NONE;
      nsadd.attr = NEBATTR_NONE;
      nsadd.timestamp.tv_sec = time(NULL);
      nsadd.object_ptr = sd;

      // Callback.
      neb::callback_dependency(
             NEBCALLBACK_ADAPTIVE_DEPENDENCY_DATA,
             &nsadd);
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high)
      << "init: error occurred while dumping service dependencies: "
      << e.what();
  }
  catch (...) {
    logging::error(logging::high) << "init: unknown error occurred "
      << "while dumping service dependencies";
  }

  // End log message.
  logging::info(logging::medium)
    << "init: end of service dependencies dump";

  return ;
}

/**
 *  Send to the global publisher the list of services within Nagios.
 */
static void send_service_list() {
  // Start log message.
  logging::info(logging::medium) << "init: beginning service dump";

  // Loop through all services.
  for (service* s = service_list; s; s = s->next) {
    // Fill callback struct.
    nebstruct_adaptive_service_data nsasd;
    memset(&nsasd, 0, sizeof(nsasd));
    nsasd.type = NEBTYPE_SERVICE_ADD;
    nsasd.command_type = CMD_NONE;
    nsasd.modified_attribute = MODATTR_ALL;
    nsasd.modified_attributes = MODATTR_ALL;
    nsasd.object_ptr = s;

    // Callback.
    neb::callback_service(NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &nsasd);
  }

  // End log message.
  logging::info(logging::medium) << "init: end of services dump";

  return ;
}

/**************************************
*                                     *
*          Global Functions           *
*                                     *
**************************************/

/**
 *  Send initial configuration to the global publisher.
 */
void neb::send_initial_configuration() {
  send_host_list();
  send_service_list();
  send_custom_variables_list();
  send_host_parents_list();
  send_host_dependencies_list();
  send_service_dependencies_list();
  send_module_list();
  return ;
}
