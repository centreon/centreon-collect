/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <QDebug>
#include <exception>
#include "error.hh"
#include "objects.hh"
#include "utils.hh"
#include "macros.hh"
#include "globals.hh"

using namespace com::centreon::engine;

/**
 *  Check if remove hostescalation works with some hostescalation.
 */
static void remove_all_hostescalation() {
  init_object_skiplists();

  add_hostescalation("hostescalation_host_name_1",
		     0, 0, 0.0,
		     "hostescalation_escalation_period",
		     0, 0, 0);
  add_hostescalation("hostescalation_host_name_2",
		     0, 0, 0.0,
		     "hostescalation_escalation_period",
		     0, 0, 0);
  add_hostescalation("hostescalation_host_name_3",
		     0, 0, 0.0,
		     "hostescalation_escalation_period",
		     0, 0, 0);

  if (remove_hostescalation_by_id("hostescalation_host_name_2") != 1
      || remove_hostescalation_by_id("hostescalation_host_name_1") != 1
      || remove_hostescalation_by_id("hostescalation_host_name_3") != 1
      || hostescalation_list != NULL
      || hostescalation_list_tail != NULL)
    throw (engine_error() << "remove all hostescalation failed.");

  free_object_skiplists();
}

/**
 *  Check if remove hostescalation works with invalid call.
 */
static void remove_hostescalation_failed() {
  init_object_skiplists();

  if (remove_hostescalation_by_id("") == 1)
    throw (engine_error() << "hostescalation remove but dosen't exist.");
  if (remove_hostescalation_by_id(NULL) == 1)
    throw (engine_error() << "hostescalation remove but pointer is NULL.");

  free_object_skiplists();
}

/**
 *  check if remove hostescalation works with contactgroups.
 */
static void remove_hostescalation_with_contactgroups() {
  init_object_skiplists();

  hostescalation* he = add_hostescalation("hostescalation_host_name_1",
					  0, 0, 0.0,
					  "hostescalation_escalation_period",
					  0, 0, 0);
  contactgroup* cgroup = add_contactgroup("contactgroup_name", "contactgroup_alias");
  contactgroupsmember* cgm = add_contactgroup_to_hostescalation(he, "contactgroup_name");
  cgm->group_ptr = cgroup;

  if (remove_hostescalation_by_id("hostescalation_host_name_1") != 1
      || hostescalation_list != NULL
      || hostescalation_list_tail != NULL)
    throw (engine_error() << "remove hostescalation with hostescalation failed.");

  delete[] cgroup->group_name;
  delete[] cgroup->alias;
  delete cgroup;

  free_object_skiplists();
}

/**
 *  Check if remove hostescalation works with some contacts.
 */
static void remove_hostescalation_with_contacts() {
  init_object_skiplists();

  hostescalation* he = add_hostescalation("hostescalation_host_name_1",
					  0, 0, 0.0,
					  "hostescalation_escalation_period",
					  0, 0, 0);
  contact* cntct = add_contact("contact_name", NULL, NULL, NULL, NULL, NULL,
			       NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  contactsmember* cm = add_contact_to_hostescalation(he, "contact_name");
  cm->contact_ptr = cntct;

  if (remove_hostescalation_by_id("hostescalation_host_name_1") != 1
      || hostescalation_list != NULL
      || hostescalation_list_tail != NULL)
    throw (engine_error() << "remove hostescalation with hostescalation failed.");

  delete[] cntct->name;
  delete[] cntct->alias;
  delete cntct;

  free_object_skiplists();
}

/**
 *  Check if remove hostescalation works.
 */
int main(void) {
  try {
    remove_all_hostescalation();
    remove_hostescalation_failed();
    remove_hostescalation_with_contactgroups();
    remove_hostescalation_with_contacts();
  }
  catch (std::exception const& e) {
    qDebug() << "error: " << e.what();
    free_memory(get_global_macros());
    return (1);
  }
  return (0);
}
