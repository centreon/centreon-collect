/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2020 Centreon
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

#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/descriptors.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using com::centreon::engine::macros::descriptor;
using com::centreon::engine::macros::family;
using com::centreon::engine::macros::find_descriptor;
using com::centreon::engine::macros::macro_target;

/**************************************
 *                                     *
 *           Local Functions           *
 *                                     *
 **************************************/

/**
 *  Build the target of a macro computed on one object.
 *
 *  @param[in] obj The object.
 *
 *  @return The target.
 */
static macro_target target_of(com::centreon::engine::host* obj) {
  macro_target retval;
  retval.hst = obj;
  return retval;
}

static macro_target target_of(com::centreon::engine::service* obj) {
  macro_target retval;
  retval.svc = obj;
  return retval;
}

static macro_target target_of(com::centreon::engine::contact* obj) {
  macro_target retval;
  retval.cntct = obj;
  return retval;
}

static macro_target target_of(com::centreon::engine::hostgroup* obj) {
  macro_target retval;
  retval.hg = obj;
  return retval;
}

static macro_target target_of(com::centreon::engine::servicegroup* obj) {
  macro_target retval;
  retval.sg = obj;
  return retval;
}

static macro_target target_of(com::centreon::engine::contactgroup* obj) {
  macro_target retval;
  retval.cg = obj;
  return retval;
}

/**
 *  Get host macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
static int handle_host_macro(nagios_macros* mac,
                             const descriptor& desc,
                             const std::string_view& arg1,
                             const std::string_view& arg2,
                             std::string& output,
                             int* free_macro) {
  int retval;
  if (arg2.empty()) {
    // Find the host for on-demand macros
    // or use saved host pointer.
    host* hst = nullptr;

    if (!arg1.empty()) {
      host_map::const_iterator it(host::hosts.find(arg1));
      if (it != host::hosts.end())
        hst = it->second.get();
    } else
      hst = mac->host_ptr;

    if (hst) {
      // Get the host macro value.
      output = desc.get(target_of(hst), mac);
      *free_macro = true;
      retval = OK;
    } else
      retval = ERROR;
  }
  // A host macro with a hostgroup name and delimiter.
  else {
    hostgroup* hg(nullptr);
    hostgroup_map::const_iterator it{hostgroup::hostgroups.find(arg1)};
    if (it != hostgroup::hostgroups.end())
      hg = it->second.get();

    if (hg) {
      // Concatenate macro values for all hostgroup members.
      for (host_map_unsafe::iterator it(hg->members.begin()),
           end(hg->members.end());
           it != end; ++it) {
        if (it->second) {
          // Get the macro value for this host.
          std::string buffer = desc.get(target_of(it->second), mac);

          // Add macro value to already running macro.
          if (output.empty())
            output = buffer;
          else {
            output.append(arg2);
            output.append(buffer);
          }

          *free_macro = true;
        }
      }
      retval = OK;
    } else
      retval = ERROR;
  }
  return retval;
}

/**
 *  Get hostgroup macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
static int handle_hostgroup_macro(nagios_macros* mac,
                                  const descriptor& desc,
                                  const std::string_view& arg1,
                                  const std::string_view& arg2,
                                  std::string& output,
                                  int* free_macro) {
  (void)arg2;

  // Return value.
  int retval;

  // Use the saved hostgroup pointer
  // or find the hostgroup for on-demand macros.
  hostgroup* hg(nullptr);
  if (!arg1.empty()) {
    hostgroup_map::const_iterator it(hostgroup::hostgroups.find(arg1));
    if (it != hostgroup::hostgroups.end())
      hg = it->second.get();
  } else
    hg = mac->hostgroup_ptr;

  if (hg) {
    // Get the hostgroup macro value.
    output = desc.get(target_of(hg), mac);
    *free_macro = true;
    retval = OK;
  } else
    retval = ERROR;
  return retval;
}

/**
 *  Get service macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if macro must be freed by caller.
 *
 *  @return OK on success.
 */
static int handle_service_macro(nagios_macros* mac,
                                const descriptor& desc,
                                const std::string_view& arg1,
                                const std::string_view& arg2,
                                std::string& output,
                                int* free_macro) {
  // Return value.
  int retval;

  // Use saved service pointer.
  if (arg1.empty() && arg2.empty()) {
    if (!mac->service_ptr)
      retval = ERROR;
    else {
      output = desc.get(target_of(mac->service_ptr), mac);
      *free_macro = true;
      retval = OK;
    }
  }
  // Else and ondemand macro...
  else {
    // If first arg is blank, it means use the current host name.
    if (arg1.empty()) {
      if (!mac->host_ptr)
        retval = ERROR;
      else if (!arg2.empty()) {
        service_map::const_iterator found =
            service::services.find(std::make_pair(mac->host_ptr->name(), arg2));

        if (found == service::services.end() || !found->second)
          retval = ERROR;
        else {
          // Get the service macro value.
          output = desc.get(target_of(found->second.get()), mac);
          *free_macro = true;
          retval = OK;
        }
      } else
        retval = ERROR;
    } else if (!arg1.empty() && !arg2.empty()) {
      // On-demand macro with both host and service name.
      service_map::const_iterator found(
          service::services.find(std::make_pair(arg1, arg2)));

      if (found != service::services.end() && found->second) {
        // Get the service macro value.
        output = desc.get(target_of(found->second.get()), mac);
        *free_macro = true;
        retval = OK;
      }
      // Else we have a service macro with a
      // servicegroup name and a delimiter...
      else {
        servicegroup_map::const_iterator sg_it{
            servicegroup::servicegroups.find(arg1)};
        if (sg_it == servicegroup::servicegroups.end() || !sg_it->second)
          retval = ERROR;
        else {
          // Concatenate macro values for all servicegroup members.
          for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
               end(sg_it->second->members.end());
               it != end; ++it) {
            if (it->second != nullptr) {
              // Get the macro value for this service.
              std::string buffer = desc.get(target_of(it->second), mac);
              if (output.empty())
                output = buffer;
              else {
                output.append(arg2);
                output.append(buffer);
              }
            }
          }
          *free_macro = true;
          retval = OK;
        }
      }
    } else
      retval = ERROR;
  }
  return retval;
}

/**
 *  Get servicegroup macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
static int handle_servicegroup_macro(nagios_macros* mac,
                                     const descriptor& desc,
                                     const std::string_view& arg1,
                                     const std::string_view& arg2
                                     [[maybe_unused]],
                                     std::string& output,
                                     int* free_macro) {
  // Return value.
  int retval;

  // Use the saved servicegroup pointer
  // or find the servicegroup for on-demand macros.
  servicegroup_map::const_iterator sg_it{
      servicegroup::servicegroups.find(arg1)};
  if (sg_it == servicegroup::servicegroups.end() || !sg_it->second)
    retval = ERROR;
  else {
    // Get the servicegroup macro value.
    output = desc.get(target_of(sg_it->second.get()), mac);
    *free_macro = true;
    retval = OK;
  }

  return retval;
}

/**
 *  Get contact macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
static int handle_contact_macro(nagios_macros* mac,
                                const descriptor& desc,
                                const std::string_view& arg1,
                                const std::string_view& arg2,
                                std::string& output,
                                int* free_macro) {
  // Return value.
  int retval;

  if (arg2.empty()) {
    contact* cnct;
    // Find the contact for on-demand macros
    // or use saved contact pointer.
    if (!arg1.empty()) {
      contact_map::const_iterator ct_it{contact::contacts.find(arg1)};
      if (ct_it == contact::contacts.end())
        return ERROR;
      cnct = ct_it->second.get();
    } else
      cnct = mac->contact_ptr;

    if (!cnct)
      return ERROR;

    // Get the contact macro value.
    output = desc.get(target_of(cnct), mac);
    *free_macro = true;
    retval = OK;
  }
  // A contact macro with a contactgroup name and delimiter.
  else if (!arg1.empty() && !arg2.empty()) {
    contactgroup_map::iterator cg{contactgroup::contactgroups.find(arg1)};
    if (cg == contactgroup::contactgroups.end() || !cg->second)
      retval = ERROR;
    else {
      // Concatenate macro values for all contactgroup members.
      for (contact_map::const_iterator it = cg->second->get_members().begin(),
                                       end = cg->second->get_members().end();
           it != end; ++it) {
        if (it->second) {
          // Get the macro value for this contact.
          std::string buffer = desc.get(target_of(it->second.get()), mac);
          // Add macro value to already running macro.
          if (output.empty())
            output = buffer;
          else {
            output.append(arg2);
            output.append(buffer);
          }
        }
      }
      *free_macro = true;
      retval = OK;
    }
  } else
    retval = ERROR;
  return retval;
}

/**
 *  Get contactgroup macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
static int handle_contactgroup_macro(nagios_macros* mac [[maybe_unused]],
                                     const descriptor& desc,
                                     const std::string_view& arg1,
                                     const std::string_view& arg2,
                                     std::string& output,
                                     int* free_macro) {
  (void)arg2;

  // Return value.
  int retval = OK;

  // Use the saved contactgroup pointer.
  // or find the contactgroup for on-demand macros.
  std::shared_ptr<com::centreon::engine::contactgroup> cg{nullptr};
  contactgroup_map::iterator cg_it{contactgroup::contactgroups.find(arg1)};
  if (cg_it == contactgroup::contactgroups.end() || !cg_it->second)
    retval = ERROR;
  else
    cg = cg_it->second;

  if (cg) {
    // Get the contactgroup macro value.
    output = desc.get(target_of(cg.get()), mac);
    *free_macro = true;
  }

  return retval;
}

/**
 *  Get a notification macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to false.
 *
 *  @return OK on success.
 */
static int handle_notification_macro(nagios_macros* mac,
                                     const descriptor& desc,
                                     const std::string_view& arg1,
                                     const std::string_view& arg2,
                                     std::string& output,
                                     int* free_macro) {
  (void)arg1;
  (void)arg2;

  // Notification macros have already been pre-computed.
  output = desc.get(macro_target(), mac);
  *free_macro = false;

  return OK;
}

/**
 *  Get a date/time macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
static int handle_datetime_macro(nagios_macros* mac,
                                 const descriptor& desc,
                                 const std::string_view& arg1,
                                 const std::string_view& arg2,
                                 std::string& output,
                                 int* free_macro) {
  macro_target target;
  target.when = time(nullptr);
  if (desc.id == MACRO_ISVALIDTIME || desc.id == MACRO_NEXTVALIDTIME) {
    // Find the timeperiod.
    auto it = ::timeperiods.find(arg1);
    if (it == ::timeperiods.end() || !it->second)
      return ERROR;
    target.tp = it->second.get();

    // What timestamp should we use?
    time_t test_time;
    if (!arg2.empty() && absl::SimpleAtoi(arg2, &test_time))
      target.when = test_time;
  }

  output = desc.get(target, mac);
  *free_macro = true;
  return OK;
}

/**
 *  Get a static macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to false.
 *
 *  @return OK on success.
 */
static int handle_static_macro(nagios_macros* mac,
                               const descriptor& desc,
                               const std::string_view& arg1,
                               const std::string_view& arg2,
                               std::string& output,
                               int* free_macro) {
  (void)mac;
  (void)arg1;
  (void)arg2;

  // No need to do any more work - these are already precomputed for us.
  output = desc.get(macro_target(), mac);
  *free_macro = false;
  return OK;
}

/**
 *  Get a summary macro.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to false.
 *
 *  @return OK on success.
 */
static int handle_summary_macro(nagios_macros* mac,
                                const descriptor& desc,
                                const std::string_view& arg1 [[maybe_unused]],
                                const std::string_view& arg2 [[maybe_unused]],
                                std::string& output,
                                int* free_macro) {
  output = desc.get(macro_target(), mac);

  // Tell caller to NOT free memory when done.
  *free_macro = false;

  return OK;
}

/**************************************
 *                                     *
 *          Global Functions           *
 *                                     *
 **************************************/

/**
 *  Compute all the summary macros ($TOTAL*$) and cache them in the macros,
 *  if not already done.
 *
 *  @param[in,out] mac The macros.
 */
void generate_summary_macros(nagios_macros* mac) {
  // Generate summary macros if needed.
  if (mac->x[MACRO_TOTALHOSTSUP].empty()) {
    // Get host totals.
    unsigned int host_problems(0);
    unsigned int host_problems_unhandled(0);
    unsigned int hosts_down(0);
    unsigned int hosts_down_unhandled(0);
    unsigned int hosts_unreachable(0);
    unsigned int hosts_unreachable_unhandled(0);
    unsigned int hosts_up(0);
    for (host_map::iterator it(com::centreon::engine::host::hosts.begin()),
         end(com::centreon::engine::host::hosts.end());
         it != end; ++it) {
      // Filter totals based on contact if necessary.
      bool authorized(mac->contact_ptr ? is_contact_for_notifier(
                                             it->second.get(), mac->contact_ptr)
                                       : true);
      if (authorized) {
        bool problem(true);
        if ((it->second->get_current_state() == host::state_up) &&
            it->second->has_been_checked())
          hosts_up++;
        else if (it->second->get_current_state() == host::state_down) {
          if (it->second->get_scheduled_downtime_depth() > 0)
            problem = false;
          if (it->second->problem_has_been_acknowledged())
            problem = false;
          if (!it->second->active_checks_enabled())
            problem = false;
          if (problem)
            hosts_down_unhandled++;
          hosts_down++;
        } else if (it->second->get_current_state() == host::state_unreachable) {
          if (it->second->get_scheduled_downtime_depth() > 0)
            problem = false;
          if (it->second->problem_has_been_acknowledged())
            problem = false;
          if (!it->second->active_checks_enabled())
            problem = false;
          if (problem)
            hosts_unreachable_unhandled++;
          hosts_unreachable++;
        }
      }
    }
    host_problems = hosts_down + hosts_unreachable;
    host_problems_unhandled =
        hosts_down_unhandled + hosts_unreachable_unhandled;

    // Get service totals.
    unsigned int service_problems(0);
    unsigned int service_problems_unhandled(0);
    unsigned int services_critical(0);
    unsigned int services_critical_unhandled(0);
    unsigned int services_ok(0);
    unsigned int services_unknown(0);
    unsigned int services_unknown_unhandled(0);
    unsigned int services_warning(0);
    unsigned int services_warning_unhandled(0);

    for (service_map::iterator it(service::services.begin()),
         end(service::services.end());
         it != end; ++it) {
      // Filter totals based on contact if necessary.
      bool authorized(mac->contact_ptr ? is_contact_for_notifier(
                                             it->second.get(), mac->contact_ptr)
                                       : true);
      if (authorized) {
        bool problem(true);
        if (it->second->get_current_state() == service::state_ok &&
            it->second->has_been_checked())
          services_ok++;
        else if (it->second->get_current_state() == service::state_warning) {
          host* temp_host{nullptr};
          host_map::const_iterator found(
              host::hosts.find(it->second->get_hostname()));
          if (found != host::hosts.end())
            temp_host = found->second.get();

          if (temp_host != nullptr &&
              (temp_host->get_current_state() == host::state_down ||
               temp_host->get_current_state() == host::state_unreachable))
            problem = false;
          if (it->second->get_scheduled_downtime_depth() > 0)
            problem = false;
          if (it->second->problem_has_been_acknowledged())
            problem = false;
          if (!it->second->active_checks_enabled())
            problem = false;
          if (problem)
            services_warning_unhandled++;
          services_warning++;
        } else if (it->second->get_current_state() == service::state_unknown) {
          host* temp_host{nullptr};
          host_map::const_iterator found(
              host::hosts.find(it->second->get_hostname()));
          if (found != host::hosts.end())
            temp_host = found->second.get();

          if (temp_host != nullptr &&
              (temp_host->get_current_state() == host::state_down ||
               temp_host->get_current_state() == host::state_unreachable))
            problem = false;
          if (it->second->get_scheduled_downtime_depth() > 0)
            problem = false;
          if (it->second->problem_has_been_acknowledged())
            problem = false;
          if (!it->second->active_checks_enabled())
            problem = false;
          if (problem)
            services_unknown_unhandled++;
          services_unknown++;
        } else if (it->second->get_current_state() == service::state_critical) {
          host* temp_host{nullptr};
          host_map::const_iterator found(
              host::hosts.find(it->second->get_hostname()));
          if (found != host::hosts.end())
            temp_host = found->second.get();

          if (temp_host != nullptr &&
              (temp_host->get_current_state() == host::state_down ||
               temp_host->get_current_state() == host::state_unreachable))
            problem = false;
          if (it->second->get_scheduled_downtime_depth() > 0)
            problem = false;
          if (it->second->problem_has_been_acknowledged())
            problem = false;
          if (!it->second->active_checks_enabled())
            problem = false;
          if (problem)
            services_critical_unhandled++;
          services_critical++;
        }
      }
    }
    service_problems = services_warning + services_critical + services_unknown;
    service_problems_unhandled = services_warning_unhandled +
                                 services_critical_unhandled +
                                 services_unknown_unhandled;

    // These macros are time-intensive to compute, and will likely be
    // r, so save them all for future use.
    mac->x[MACRO_TOTALHOSTSUP] = fmt::to_string(hosts_up);
    mac->x[MACRO_TOTALHOSTSDOWN] = fmt::to_string(hosts_down);
    mac->x[MACRO_TOTALHOSTSUNREACHABLE] = fmt::to_string(hosts_unreachable);
    mac->x[MACRO_TOTALHOSTSDOWNUNHANDLED] =
        fmt::to_string(hosts_down_unhandled);
    mac->x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED] =
        fmt::to_string(hosts_unreachable_unhandled);
    mac->x[MACRO_TOTALHOSTPROBLEMS] = fmt::to_string(host_problems);
    mac->x[MACRO_TOTALHOSTPROBLEMSUNHANDLED] =
        fmt::to_string(host_problems_unhandled);
    mac->x[MACRO_TOTALSERVICESOK] = fmt::to_string(services_ok);
    mac->x[MACRO_TOTALSERVICESWARNING] = fmt::to_string(services_warning);
    mac->x[MACRO_TOTALSERVICESCRITICAL] = fmt::to_string(services_critical);
    mac->x[MACRO_TOTALSERVICESUNKNOWN] = fmt::to_string(services_unknown);
    mac->x[MACRO_TOTALSERVICESWARNINGUNHANDLED] =
        fmt::to_string(services_warning_unhandled);
    mac->x[MACRO_TOTALSERVICESCRITICALUNHANDLED] =
        fmt::to_string(services_critical_unhandled);
    mac->x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED] =
        fmt::to_string(services_unknown_unhandled);
    mac->x[MACRO_TOTALSERVICEPROBLEMS] = fmt::to_string(service_problems);
    mac->x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED] =
        fmt::to_string(service_problems_unhandled);
  }
}

/* this is the big one */
int grab_macro_value_r(nagios_macros* mac,
                       const std::string_view& macro_name,
                       std::string& output,
                       int* clean_options,
                       int* free_macro) {
  std::string_view buf;
  std::string_view arg[2];

  contact* temp_contact = nullptr;
  std::string temp_buffer;
  unsigned int x;
  int result = OK;

  if (macro_name.empty() || clean_options == nullptr || free_macro == nullptr)
    return ERROR;

  /* work with a copy of the original buffer */
  auto arr = absl::StrSplit(macro_name, ':');
  auto it = arr.begin();

  // The base name
  if (it != arr.end()) {
    buf = *it;
    ++it;
  }

  // First argument
  if (it != arr.end()) {
    arg[0] = *it;
    ++it;
  }
  // Second argument
  if (it != arr.end()) {
    arg[1] = *it;
  }

  // buf = string::dup(macro_name.c_str());

  /* BY DEFAULT, TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
  *free_macro = true;

  /* see if there's an argument - if so, this is most likely an on-demand macro
   */
  // if ((ptr = strchr(buf, ':'))) {
  //   ptr[0] = '\x0';
  //   ptr++;

  //  /* save the first argument - host name, hostgroup name, etc. */
  //  arg[0] = ptr;

  //  /* try and find a second argument */
  //  if ((ptr = strchr(ptr, ':'))) {
  //    ptr[0] = '\x0';
  //    ptr++;

  //    /* save second argument - service description or delimiter */
  //    arg[1] = ptr;
  //  }
  //}

  /***** X MACROS *****/
  const descriptor* desc = find_descriptor(buf);
  if (desc) {
    macros_logger->trace("  macros[{}] ({}) match.", desc->id, desc->name);

    /* get the macro value */
    result =
        grab_macrox_value_r(mac, *desc, arg[0], arg[1], output, free_macro);

    /* post-processing */
    if (desc->cleaned) {
      *clean_options |= (STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
      macros_logger->trace("  New clean options: {}", *clean_options);
    }
  }
  /***** ARGV MACROS *****/
  else if (macro_name.size() > 3 && macro_name.compare(0, 3, "ARG") == 0) {
    /* which arg do we want? */
    if (!absl::SimpleAtoi(macro_name.substr(3), &x)) {
      macros_logger->error(
          "Error: could not grab macro value : '{}' must be a positive integer",
          macro_name.substr(3));
      return ERROR;
    }

    if (!x || x > MAX_COMMAND_ARGUMENTS) {
      return ERROR;
    }

    /* use a pre-computed macro value */
    output = mac->argv[x - 1];
    *free_macro = false;
  }
  /***** USER MACROS *****/
  else if (macro_name.size() > 4 && macro_name.compare(0, 4, "USER") == 0) {
    /* which macro do we want? */
    if (!absl::SimpleAtoi(macro_name.substr(4), &x)) {
      macros_logger->error(
          "Error: could not grab macro value : '{}' must be a positive integer",
          macro_name.substr(4));
      return ERROR;
    }

    if (!x || x > MAX_USER_MACROS) {
      return ERROR;
    }

    /* use a pre-computed macro value */
    output = macro_user[x - 1];
    *free_macro = false;
  }

  /***** CONTACT ADDRESS MACROS *****/
  /* NOTE: the code below should be broken out into a separate function */
  else if (buf.size() > 14 && buf.compare(0, 14, "CONTACTADDRESS") == 0) {
    /* which address do we want? (buf, the name without the arguments of an
     * on-demand macro) */
    if (!absl::SimpleAtoi(buf.substr(14), &x)) {
      macros_logger->error(
          "Error: could not grab macro value : '{}' must be a positive integer",
          buf.substr(14));
      return ERROR;
    }
    x -= 1;

    /* regular macro */
    if (arg[0] == "") {
      /* use the saved pointer */
      if ((temp_contact = mac->contact_ptr) == nullptr) {
        return ERROR;
      }

      /* get the macro value */
      result = grab_contact_address_macro(x, temp_contact, output);
    }
    /* on-demand macro */
    else {
      /* on-demand contact macro with a contactgroup and a delimiter */
      if (arg[1] != "") {
        contactgroup_map::iterator cg_it{
            contactgroup::contactgroups.find(arg[0])};
        if (cg_it == contactgroup::contactgroups.end() || !cg_it->second)
          return ERROR;

        /* concatenate macro values for all contactgroup members */
        for (contact_map::const_iterator
                 it = cg_it->second->get_members().begin(),
                 end = cg_it->second->get_members().end();
             it != end; ++it) {
          if (!it->second)
            continue;

          /* get the macro value for this contact */
          grab_contact_address_macro(x, it->second.get(), temp_buffer);

          if (temp_buffer.empty())
            continue;

          /* add macro value to already running macro */
          if (output.empty())
            output.append(temp_buffer);
          else {
            output.append(arg[1]);
            output.append(temp_buffer);
          }
          temp_buffer = "";
        }
      }
      /* else on-demand contact macro */
      else {
        /* find the contact */
        contact_map::const_iterator it{contact::contacts.find(arg[0])};
        if (it == contact::contacts.end()) {
          return ERROR;
        }

        /* get the macro value */
        result = grab_contact_address_macro(x, it->second.get(), output);
      }
    }
  }
  /***** CUSTOM VARIABLE MACROS *****/
  else if (macro_name[0] == '_') {
    /* get the macro value */

    result = grab_custom_macro_value_r(mac, buf, arg[0], arg[1], output);
  } else if (configuration::applier::state::instance().user_macros().find(
                 macro_name) !=
             configuration::applier::state::instance().user_macros().end()) {
    /*** New style user macros ***/
    output = configuration::applier::state::instance()
                 .user_macros_find(macro_name)
                 ->second;
    result = true;
  }
  /* no macro matched... */
  else {
    macros_logger->trace(" WARNING: Could not find a macro matching '{}'!",
                         macro_name);
    result = ERROR;
  }

  // some macros are encrypted?
  if (pb_indexed_config.state().credentials_encryption()) {
    if (!output.compare(0, 5, "raw::")) {
      output.erase(0, 5);
    } else if (!output.compare(0, 9, "encrypt::")) {
      if (!credentials_decrypt) {
        SPDLOG_LOGGER_ERROR(macros_logger,
                            "no encryption configured => can't decryp macro {}",
                            macro_name);
        return ERROR;
      }
      try {
        output =
            credentials_decrypt->decrypt(std::string_view(output).substr(9));
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(macros_logger,
                            "fail to decrypt content of macro {}: {}",
                            macro_name, e.what());
      }
    }
  }

  return result;
}

/**
 *  Grab a macro value.
 *
 *  @param[out] mac        Macro object.
 *  @param[in]  desc       Macro to get.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *  @param[out] free_macro Set to true if output buffer must be freed by
 *                         caller.
 *
 *  @return OK on success.
 */
int grab_macrox_value_r(nagios_macros* mac,
                        const descriptor& desc,
                        const std::string_view& arg1,
                        const std::string_view& arg2,
                        std::string& output,
                        int* free_macro) {
  if (!mac || !free_macro)
    return ERROR;

  switch (desc.fam) {
    case family::host:
      return handle_host_macro(mac, desc, arg1, arg2, output, free_macro);
    case family::hostgroup:
      return handle_hostgroup_macro(mac, desc, arg1, arg2, output, free_macro);
    case family::service:
      return handle_service_macro(mac, desc, arg1, arg2, output, free_macro);
    case family::servicegroup:
      return handle_servicegroup_macro(mac, desc, arg1, arg2, output,
                                       free_macro);
    case family::contact:
      return handle_contact_macro(mac, desc, arg1, arg2, output, free_macro);
    case family::contactgroup:
      return handle_contactgroup_macro(mac, desc, arg1, arg2, output,
                                       free_macro);
    case family::notification:
      return handle_notification_macro(mac, desc, arg1, arg2, output,
                                       free_macro);
    case family::datetime:
      return handle_datetime_macro(mac, desc, arg1, arg2, output, free_macro);
    case family::statics:
      return handle_static_macro(mac, desc, arg1, arg2, output, free_macro);
    case family::summary:
      return handle_summary_macro(mac, desc, arg1, arg2, output, free_macro);
  }
  return ERROR;
}
