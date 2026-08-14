/**
 * Copyright 2001-2009 Ethan Galstad
 * Copyright 2011-2024 Centreon
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

#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/statusdata.hh"
#include "common/notifications/notification_types.hh"

using namespace com::centreon::engine;
namespace notifications = com::centreon::common::notifications;

/******************************************************************/
/***************** FLAP DETECTION STATUS FUNCTIONS ****************/
/******************************************************************/

/* enables flap detection on a program wide basis */
void enable_flap_detection_routines() {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  functions_logger->trace("enable_flap_detection_routines()");

  /* bail out if we're already set */
  if (pb_indexed_config.state().enable_flap_detection())
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* set flap detection flag */
  pb_indexed_config.mut_state().set_enable_flap_detection(true);

  /* update program status */
  update_program_status(false);

  /* check for flapping. check_for_flapping() may start the flapping of an
   * object, and nothing else publishes that flag on this path, so each object
   * that just toggled has to send an adaptive status. Objects that did not
   * toggle publish nothing, which is the general case. */
  for (auto& [_, hst] : com::centreon::engine::host::hosts) {
    bool was_flapping = hst->get_is_flapping();
    hst->check_for_flapping(false, false, true);
    if (hst->get_is_flapping() != was_flapping)
      hst->update_status(notifications::STATUS_FLAPPING);
  }
  for (auto& [_, svc] : service::services) {
    bool was_flapping = svc->get_is_flapping();
    svc->check_for_flapping(false, true);
    if (svc->get_is_flapping() != was_flapping)
      svc->update_status(notifications::STATUS_FLAPPING);
  }
}

/* disables flap detection on a program wide basis */
void disable_flap_detection_routines() {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  functions_logger->trace("disable_flap_detection_routines()");

  /* bail out if we're already set */
  if (!pb_indexed_config.state().enable_flap_detection())
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* set flap detection flag */
  pb_indexed_config.mut_state().set_enable_flap_detection(false);

  /* update program status */
  update_program_status(false);

  /* handle the details... */
  for (host_map::iterator it(com::centreon::engine::host::hosts.begin()),
       end(com::centreon::engine::host::hosts.end());
       it != end; ++it)
    it->second->handle_flap_detection_disabled();
  for (service_map::iterator it(service::services.begin()),
       end(service::services.end());
       it != end; ++it)
    it->second->handle_flap_detection_disabled();
}

// disables flap detection for a particular host
void disable_host_flap_detection(host* hst) {
  hst->disable_flap_detection();
}

// enables flap detection for a particular host
void enable_host_flap_detection(host* hst) {
  hst->enable_flap_detection();
}

// enables flap detection for a particular service
void enable_service_flap_detection(com::centreon::engine::service* svc) {
  svc->enable_flap_detection();
}

// disables flap detection for a particular service
void disable_service_flap_detection(service* svc) {
  svc->disable_flap_detection();
}
