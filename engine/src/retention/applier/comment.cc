/**
* Copyright 2011-2013 Merethis
* Copyright 2014-2024 Centreon
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

#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/retention/applier/comment.hh"

using namespace com::centreon::engine::retention;
using namespace com::centreon::engine;

/**
 *  Add comments on appropriate hosts and services.
 *
 *  @param[in] lst The comment list to add.
 */
void applier::comment::apply(list_comment const& lst) {
  // Big speedup when reading retention.dat in bulk.

  for (list_comment::const_iterator it(lst.begin()), end(lst.end()); it != end;
       ++it) {
    if ((*it)->comment_type() == retention::comment::host)
      _add_host_comment(**it);
    else
      _add_service_comment(**it);
  }
}

/**
 *  Add host comment.
 *
 *  @param[in] obj The comment to add into the host.
 */
void applier::comment::_add_host_comment(
    retention::comment const& obj) noexcept {
  host_map::const_iterator it(host::hosts.find(obj.host_name()));
  if (it == host::hosts.end() || !it->second)
    return;

  // Engine no longer keeps comments in memory: Broker owns them. Reading an old
  // retention file (with comment blocks) only drives the acknowledgement-comment
  // bookkeeping and the boot-time purge of non-persistent comments.
  if (obj.entry_type() == com::centreon::engine::comment::acknowledgment) {
    if (!it->second->problem_has_been_acknowledged() && !obj.persistent())
      engine::comment::delete_comment(obj.comment_id());
    // a kept non-persistent ack comment is owned by the host: restore the link
    // so it can be deleted by id when the acknowledgement is cleared.
    else if (!obj.persistent())
      it->second->set_acknowledgement_comment_id(obj.comment_id());
  }
  // non-persistent comments don't last past restarts UNLESS
  // they're acks (see above).
  else if (!obj.persistent())
    engine::comment::delete_comment(obj.comment_id());
}

/**
 *  Add service comment.
 *
 *  @param[in] obj The comment to add into the service.
 */
void applier::comment::_add_service_comment(
    retention::comment const& obj) noexcept {
  service_map::const_iterator it_svc(
      service::services.find({obj.host_name(), obj.service_description()}));
  if (it_svc == service::services.end() || !it_svc->second)
    return;

  // Engine no longer keeps comments in memory: Broker owns them. Reading an old
  // retention file (with comment blocks) only drives the acknowledgement-comment
  // bookkeeping and the boot-time purge of non-persistent comments.
  if (obj.entry_type() == com::centreon::engine::comment::acknowledgment) {
    if (!it_svc->second->problem_has_been_acknowledged() && !obj.persistent())
      engine::comment::delete_comment(obj.comment_id());
    // a kept non-persistent ack comment is owned by the service: restore the
    // link so it can be deleted by id when the acknowledgement is cleared.
    else if (!obj.persistent())
      it_svc->second->set_acknowledgement_comment_id(obj.comment_id());
  }
  // non-persistent comments don't last past restarts UNLESS
  // they're acks (see above).
  else if (!obj.persistent())
    engine::comment::delete_comment(obj.comment_id());
}
