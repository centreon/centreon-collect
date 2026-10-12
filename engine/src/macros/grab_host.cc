/**
 * Copyright 1999-2010      Ethan Galstad
 * Copyright 2011-2013,2016 Centreon
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

#include "com/centreon/engine/macros/grab_host.hh"

#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/host.hh"

using namespace com::centreon::engine;

/**************************************
 *                                     *
 *           Global Functions          *
 *                                     *
 **************************************/

extern "C" {
/**
 *  Grab macros that are specific to a host.
 *
 *  @param[in] mac Macros object.
 *  @param[in] hst Host pointer.
 *
 *  @return OK on success.
 */
int grab_host_macros_r(nagios_macros* mac, host* hst) {
  // Save pointer to host.
  mac->host_ptr = hst;
  mac->hostgroup_ptr = nullptr;
  // The cached totals were those of the previous host.
  mac->host_services.reset();

  if (hst == nullptr)
    return ERROR;

  // Save pointer to host's first/primary hostgroup.
  if (!hst->get_parent_groups().empty())
    mac->hostgroup_ptr = hst->get_parent_groups().front();

  if (!hst->contacts().empty())
    mac->contact_ptr = hst->contacts().begin()->second.get();

  if (!hst->get_contactgroups().empty())
    mac->contactgroup_ptr = hst->get_contactgroups().begin()->second.get();

  return OK;
}
}
