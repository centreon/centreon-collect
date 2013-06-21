/*
** Copyright 2011-2013 Merethis
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

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/deleter/servicegroup.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/misc/object.hh"
#include "com/centreon/engine/misc/string.hh"
#include "com/centreon/engine/objects/servicegroup.hh"
#include "com/centreon/engine/objects/servicesmember.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/shared_ptr.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::misc;

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool operator==(
       servicegroup const& obj1,
       servicegroup const& obj2) throw () {
  return (is_equal(obj1.group_name, obj2.group_name)
          && is_equal(obj1.alias, obj2.alias)
          && is_equal(obj1.members, obj2.members)
          && is_equal(obj1.notes, obj2.notes)
          && is_equal(obj1.notes_url, obj2.notes_url)
          && is_equal(obj1.action_url, obj2.action_url));
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool operator!=(
       servicegroup const& obj1,
       servicegroup const& obj2) throw () {
  return (!operator==(obj1, obj2));
}

/**
 *  Dump servicegroup content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The servicegroup to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, servicegroup const& obj) {
  os << "servicegroup {\n"
    "  group_name: " << chkstr(obj.group_name) << "\n"
    "  alias:      " << chkstr(obj.alias) << "\n"
    "  members:    " << chkobj(obj.members) << "\n"
    "  notes:      " << chkstr(obj.notes) << "\n"
    "  notes_url:  " << chkstr(obj.notes_url) << "\n"
    "  action_url: " << chkstr(obj.action_url) << "\n"
    "}\n";
  return (os);
}

/**
 *  Add a new service group to the list in memory.
 *
 *  @param[in] name       Group name.
 *  @param[in] alias      Group alias.
 *  @param[in] notes      Notes.
 *  @param[in] notes_url  URL.
 *  @param[in] action_url Action URL.
 *
 *  @return New service group.
 */
servicegroup* add_servicegroup(
                char const* name,
                char const* alias,
                char const* notes,
                char const* notes_url,
                char const* action_url) {
  // Make sure we have the data we need.
  if (!name || !name[0]) {
    logger(log_config_error, basic)
      << "Error: Servicegroup name is NULL";
    return (NULL);
  }

  // Allocate memory.
  shared_ptr<servicegroup> obj(new servicegroup, deleter::servicegroup);
  memset(obj.get(), 0, sizeof(*obj));

  try {
    // Duplicate vars.
    obj->group_name = my_strdup(name);
    obj->alias = my_strdup(alias ? alias : name);
    if (action_url)
      obj->action_url = my_strdup(action_url);
    if (notes)
      obj->notes = my_strdup(notes);
    if (notes_url)
      obj->notes_url = my_strdup(notes_url);

    // Add new servicegroup to the monitoring engine.
    std::string id(name);
    umap<std::string, shared_ptr<servicegroup_struct> >::const_iterator
      it(state::instance().servicegroups().find(id));
    if (it != state::instance().servicegroups().end()) {
      logger(log_config_error, basic)
        << "Error: Servicegroup '" << name << "' has already been defined";
      return (NULL);
    }

    // Add new items to the configuration state.
    state::instance().servicegroups()[name] = obj;

    // Add  new items to the list.
    obj->next = servicegroup_list;
    servicegroup_list = obj.get();

    // Notify event broker.
    timeval tv(get_broker_timestamp(NULL));
    broker_group(
      NEBTYPE_SERVICEGROUP_ADD,
      NEBFLAG_NONE,
      NEBATTR_NONE,
      obj.get(),
      &tv);
  }
  catch (...) {
    obj.clear();
  }

  return (obj.get());
}

/**
 *  Tests whether a host is a member of a particular servicegroup.
 *
 *  @deprecated This function is only used by the CGIS.
 *
 *  @param[in] group Target service group.
 *  @param[in] hst   Target host.
 *
 *  @return true or false.
 */
int is_host_member_of_servicegroup(servicegroup* group, host* hst) {
  if (!group || !hst)
    return (false);

  for (servicesmember* member(group->members);
       member;
       member = member->next)
    if (member->service_ptr
        && (member->service_ptr->host_ptr == hst))
      return (true);
  return (false);
}

/**
 *  Tests whether a service is a member of a particular servicegroup.
 *
 *  @deprecated This function is only used by the CGIS.
 *
 *  @param[in] group Target group.
 *  @param[in] svc   Target service.
 *
 *  @return true or false.
 */
int is_service_member_of_servicegroup(
      servicegroup* group,
      service* svc) {
  if (!group || !svc)
    return (false);

  for (servicesmember* member(group->members);
       member;
       member = member->next)
    if (member->service_ptr == svc)
      return (true);
  return (false);
}

