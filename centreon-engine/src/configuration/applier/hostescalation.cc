/*
** Copyright 2011-2019 Centreon
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
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/hostescalation.hh"
#include "com/centreon/engine/configuration/applier/object.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/error.hh"
#include "com/centreon/engine/globals.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Default constructor.
 */
applier::hostescalation::hostescalation() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
applier::hostescalation::hostescalation(applier::hostescalation const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
applier::hostescalation::~hostescalation() throw() {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
applier::hostescalation& applier::hostescalation::operator=(
    applier::hostescalation const& right) {
  (void)right;
  return *this;
}

/**
 *  Add new host escalation.
 *
 *  @param[in] obj  The new host escalation to add into the monitoring
 *                  engine.
 */
void applier::hostescalation::add_object(
    configuration::hostescalation const& obj) {
  // Check host escalation.
  if (obj.hosts().size() != 1 || !obj.hostgroups().empty())
    throw(engine_error() << "Could not create host escalation "
                         << "with multiple hosts / host groups");

  // Logging.
  logger(logging::dbg_config, logging::more)
      << "Creating new escalation for host '" << *obj.hosts().begin() << "'.";

  // Add escalation to the global configuration set.
  config->hostescalations().insert(obj);

  // Create host escalation.
  std::shared_ptr<engine::hostescalation> he{new engine::hostescalation(
      *obj.hosts().begin(),
      obj.first_notification(),
      obj.last_notification(),
      obj.notification_interval(),
      obj.escalation_period(),
      ((obj.escalation_options() & configuration::hostescalation::down)
           ? notifier::down
           : notifier::none) |
          ((obj.escalation_options() &
            configuration::hostescalation::unreachable)
               ? notifier::unreachable
               : notifier::none) |
          ((obj.escalation_options() & configuration::hostescalation::recovery)
               ? notifier::recovery
               : notifier::none))};

  // Add new items to the configuration state.
  uint64_t host_id{get_host_id(he->get_hostname())};
  host_id_map::iterator it{engine::host::hosts_by_id.find(host_id)};
  if (it != engine::host::hosts_by_id.end())
    it->second->add_escalation(he);

  // Add new items to the list.
  engine::hostescalation::hostescalations.insert({he->get_hostname(), he});

  // Notify event broker.
  timeval tv(get_broker_timestamp(nullptr));
  broker_adaptive_escalation_data(
      NEBTYPE_HOSTESCALATION_ADD, NEBFLAG_NONE, NEBATTR_NONE, he.get(), &tv);

  // Add contacts to host escalation.
  for (set_string::const_iterator it(obj.contacts().begin()),
       end(obj.contacts().end());
       it != end;
       ++it)
    he->contacts().insert({*it, nullptr});

  // Add contact groups to host escalation.
  for (set_string::const_iterator it(obj.contactgroups().begin()),
       end(obj.contactgroups().end());
       it != end;
       ++it)
    he->contact_groups.insert({*it, nullptr});
}

/**
 *  Expand a host escalation.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::hostescalation::expand_objects(configuration::state& s) {
  // Browse all escalations.
  configuration::set_hostescalation expanded;
  for (configuration::set_hostescalation::const_iterator
           it_esc(s.hostescalations().begin()),
       end_esc(s.hostescalations().end());
       it_esc != end_esc;
       ++it_esc) {
    // Expanded hosts.
    std::set<std::string> expanded_hosts;
    _expand_hosts(it_esc->hosts(), it_esc->hostgroups(), s, expanded_hosts);

    // Browse all hosts.
    for (std::set<std::string>::const_iterator it(expanded_hosts.begin()),
         end(expanded_hosts.end());
         it != end;
         ++it) {
      configuration::hostescalation hesc(*it_esc);
      hesc.hostgroups().clear();
      hesc.hosts().clear();
      hesc.hosts().insert(*it);

      // Insert new host escalation and expand it.
      _inherits_special_vars(hesc, s);
      expanded.insert(hesc);
    }
  }

  // Set expanded host escalations in configuration state.
  s.hostescalations().swap(expanded);
}

/**
 *  @brief Modify host escalation.
 *
 *  Host escalations cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::hostescalation::modify_object(
    configuration::hostescalation const& obj) {
  (void)obj;
  throw(engine_error()
        << "Could not modify a host escalation: "
        << "host escalation objects can only be added or removed, "
        << "this is likely a software bug that you should report to "
        << "Centreon Engine developers");
}

/**
 *  Remove old hostescalation.
 *
 *  @param[in] obj  The new hostescalation to remove from the monitoring
 *                  engine.
 */
void applier::hostescalation::remove_object(
    configuration::hostescalation const& obj) {
  // Logging.
  logger(logging::dbg_config, logging::more) << "Removing a host escalation.";

  // Find host escalation.
  std::string const& host_name{*obj.hosts().begin()};
  uint64_t host_id{get_host_id(host_name)};
  std::pair<hostescalation_mmap::iterator, hostescalation_mmap::iterator> range{
      engine::hostescalation::hostescalations.equal_range(host_name)};
  host_id_map::iterator hit{engine::host::hosts_by_id.find(host_id)};

  for (hostescalation_mmap::iterator it{range.first}, end{range.second};
       it != end;
       ++it) {
    std::list<std::shared_ptr<escalation> >& escalations(
        hit->second->get_escalations());
    for (std::list<std::shared_ptr<engine::escalation> >::iterator
             itt{escalations.begin()},
         next_itt{escalations.begin()},
         end{escalations.end()};
         itt != end;
         itt = next_itt) {
      ++next_itt;
      /* It's a pity but for now we don't have any possibility or key to verify
       * if the hostescalation is the good one. */
      if ((*itt)->get_first_notification() == obj.first_notification() &&
          (*itt)->get_last_notification() == obj.last_notification() &&
          (*itt)->get_notification_interval() == obj.notification_interval() &&
          (*itt)->get_escalation_period() == obj.escalation_period() &&
          (*itt)->get_escalate_on(notifier::down) ==
              (obj.escalation_options() &
               configuration::hostescalation::down) &&
          (*itt)->get_escalate_on(notifier::unreachable) ==
              (obj.escalation_options() &
               configuration::hostescalation::unreachable) &&
          (*itt)->get_escalate_on(notifier::recovery) ==
              (obj.escalation_options() &
               configuration::hostescalation::recovery)) {
        // We have the hostescalation to remove.
        escalations.erase(itt);
        break;
      }
    }
    // Notify event broker.
    timeval tv(get_broker_timestamp(nullptr));
    broker_adaptive_escalation_data(NEBTYPE_HOSTESCALATION_DELETE,
                                    NEBFLAG_NONE,
                                    NEBATTR_NONE,
                                    it->second.get(),
                                    &tv);

    // Remove host escalation from its list.
    engine::hostescalation::hostescalations.erase(it->first);
  }

  config->hostescalations().erase(obj);
}

/**
 *  Resolve a hostescalation.
 *
 *  @param[in] obj  Hostescalation object.
 */
void applier::hostescalation::resolve_object(
    configuration::hostescalation const& obj) {
  // Logging.
  logger(logging::dbg_config, logging::more) << "Resolving a host escalation.";

  uint64_t host_id{get_host_id(*obj.hosts().begin())};
  host_id_map::iterator it{engine::host::hosts_by_id.find(host_id)};
  for (std::list<std::shared_ptr<engine::escalation> >::const_iterator
           itt{it->second->get_escalations().begin()},
       end{it->second->get_escalations().end()};
       itt != end;
       ++itt) {
    /* It's a pity but for now we don't have any idea or key to verify if
     * the hostescalation is the good one. */
    if ((*itt)->get_first_notification() == obj.first_notification() &&
        (*itt)->get_last_notification() == obj.last_notification() &&
        (*itt)->get_notification_interval() == obj.notification_interval() &&
        (*itt)->get_escalation_period() == obj.escalation_period() &&
        (*itt)->get_escalate_on(notifier::down) ==
            (obj.escalation_options() & configuration::hostescalation::down) &&
        (*itt)->get_escalate_on(notifier::unreachable) ==
            (obj.escalation_options() &
             configuration::hostescalation::unreachable) &&
        (*itt)->get_escalate_on(notifier::recovery) ==
            (obj.escalation_options() &
             configuration::hostescalation::recovery)) {
      // Resolve host escalation.
      if (!check_hostescalation(
               std::static_pointer_cast<engine::hostescalation>(*itt),
               &config_warnings,
               &config_errors))
        throw engine_error() << "Cannot resolve host escalation";
    } else
      throw engine_error() << "Cannot resolve non-existing host escalation";
  }
}

/**
 *  Expand hosts.
 *
 *  @param[in]     hosts      Host list.
 *  @param[in]     hostgroups Host group list.
 *  @param[in,out] s          Configuration being applied.
 *  @param[out]    expanded   Expanded hosts.
 */
void applier::hostescalation::_expand_hosts(
    std::set<std::string> const& hosts,
    std::set<std::string> const& hostgroups,
    configuration::state const& s,
    std::set<std::string>& expanded) {
  // Copy hosts.
  expanded = hosts;

  // Browse host groups.
  for (set_string::const_iterator it(hostgroups.begin()), end(hostgroups.end());
       it != end;
       ++it) {
    // Find host group.
    set_hostgroup::const_iterator it_group(s.hostgroups_find(*it));
    if (it_group == s.hostgroups().end())
      throw(engine_error() << "Could not expand non-existing host group '"
                           << *it << "'");

    // Add host group members.
    expanded.insert(it_group->members().begin(), it_group->members().end());
  }
}

/**
 *  Inherits special variables from the host.
 *
 *  @param[in,out] obj  Host escalation object.
 *  @param[in]     s    Configuration state.
 */
void applier::hostescalation::_inherits_special_vars(
    configuration::hostescalation& obj,
    configuration::state& s) {
  // Detect if any special variable has not been defined.
  if (!obj.contacts_defined() || !obj.contactgroups_defined() ||
      !obj.notification_interval_defined() ||
      !obj.escalation_period_defined()) {
    // Find host.
    configuration::set_host::const_iterator it(
        s.hosts_find(get_host_id(obj.hosts().begin()->c_str())));
    if (it == s.hosts().end())
      throw(engine_error() << "Could not inherit special variables from host '"
                           << *obj.hosts().begin() << "': host does not exist");

    // Inherits variables.
    if (!obj.contacts_defined())
      obj.contacts() = it->contacts();
    if (!obj.contactgroups_defined())
      obj.contactgroups() = it->contactgroups();
    if (!obj.notification_interval_defined())
      obj.notification_interval(it->notification_interval());
    if (!obj.escalation_period_defined())
      obj.escalation_period(it->notification_period());
  }
}
