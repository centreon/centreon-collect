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

#include "com/centreon/engine/macros/grab_service.hh"

#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/service.hh"

using namespace com::centreon::engine;

/**************************************
 *                                     *
 *          Global Functions           *
 *                                     *
 **************************************/

/**
 *  Grab macros that are specific to a service.
 *
 *  @param[in] mac Macros object.
 *  @param[in] svc Service pointer.
 *
 *  @return OK on success.
 */
int grab_service_macros_r(macro_context* mac,
                          com::centreon::engine::service* svc) {
  // Save pointer for later.
  mac->service_ptr = svc;
  mac->servicegroup_ptr = nullptr;

  if (svc == nullptr)
    return ERROR;

  // Save first/primary servicegroup pointer for later.
  if (!svc->get_parent_groups().empty())
    mac->servicegroup_ptr = svc->get_parent_groups().front();

  if (!svc->contacts().empty())
    mac->contact_ptr = svc->contacts().begin()->second.get();

  if (!svc->get_contactgroups().empty())
    mac->contactgroup_ptr = svc->get_contactgroups().begin()->second.get();

  return OK;
}
