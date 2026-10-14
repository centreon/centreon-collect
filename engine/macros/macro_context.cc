/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "engine/macros/macro_context.hh"

#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

namespace com::centreon::engine {

/**
 * @brief Set the host the macros are about, with its first hostgroup. Its
 * first contact and contactgroup become those of the context.
 *
 * @param hst The host, nullptr to forget the current one.
 */
void macro_context::set_host(host* hst) {
  host_ptr = hst;
  hostgroup_ptr = nullptr;
  // The cached totals were those of the previous host.
  host_services.reset();

  if (hst == nullptr)
    return;

  if (!hst->get_parent_groups().empty())
    hostgroup_ptr = hst->get_parent_groups().front();

  if (!hst->contacts().empty())
    contact_ptr = hst->contacts().begin()->second.get();

  if (!hst->get_contactgroups().empty())
    contactgroup_ptr = hst->get_contactgroups().begin()->second.get();
}

/**
 * @brief Set the service the macros are about, with its first servicegroup.
 * Its first contact and contactgroup become those of the context.
 *
 * @param svc The service, nullptr to forget the current one.
 */
void macro_context::set_service(service* svc) {
  service_ptr = svc;
  servicegroup_ptr = nullptr;

  if (svc == nullptr)
    return;

  if (!svc->get_parent_groups().empty())
    servicegroup_ptr = svc->get_parent_groups().front();

  if (!svc->contacts().empty())
    contact_ptr = svc->contacts().begin()->second.get();

  if (!svc->get_contactgroups().empty())
    contactgroup_ptr = svc->get_contactgroups().begin()->second.get();
}

/**
 * @brief Set the contact the macros are about, with its first contactgroup.
 * The $TOTAL*$ macros are filtered by contact, so their cache is cleared.
 *
 * @param cntct The contact, nullptr to forget the current one.
 */
void macro_context::set_contact(contact* cntct) {
  contact_ptr = cntct;
  contactgroup_ptr = nullptr;
  summary.reset();

  if (cntct == nullptr)
    return;

  if (!cntct->get_parent_groups().empty())
    contactgroup_ptr = cntct->get_parent_groups().begin()->second;
}

/**
 * @brief Clear the $ARGn$ macros.
 */
void macro_context::clear_argv() {
  for (std::string& arg : argv)
    arg.clear();
}

/**
 * @brief Clear the whole context, as if it was just created.
 */
void macro_context::reset() {
  *this = macro_context();
}

}  // namespace com::centreon::engine
