/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2026 Centreon
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

#include <absl/strings/match.h>

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

/**
 *  The raw pointer of a map value.
 */
template <typename T>
static T* ptr_of(T* p) {
  return p;
}

template <typename P>
static auto* ptr_of(const P& p) {
  return p.get();
}

/**
 *  Find an object by its key.
 *
 *  @param[in] map The objects.
 *  @param[in] key The key of the object.
 *
 *  @return The object, nullptr if not found.
 */
template <typename Map, typename Key>
static auto find_object(const Map& map, const Key& key)
    -> decltype(ptr_of(map.begin()->second)) {
  auto it = map.find(key);
  if (it == map.end())
    return nullptr;
  return ptr_of(it->second);
}

/**
 *  The name of the kind of objects a family of macros is computed on.
 *
 *  @param[in] fam The family.
 *
 *  @return Its name ("host"...).
 */
static std::string_view family_name(family fam) {
  switch (fam) {
    case family::host:
      return "host";
    case family::hostgroup:
      return "hostgroup";
    case family::service:
      return "service";
    case family::servicegroup:
      return "servicegroup";
    case family::contact:
      return "contact";
    case family::contactgroup:
      return "contactgroup";
    default:
      return "object";
  }
}

/**
 *  Concatenate the values of the members of a group. The empty values are
 *  skipped.
 *
 *  @param[in]  members   The members of the group.
 *  @param[in]  separator The separator between two values.
 *  @param[in]  value     Computes a value on a member (see resolve()).
 *
 *  @return The concatenated values.
 */
template <typename Members, typename Value>
static std::string concatenate(const Members& members,
                               std::string_view separator,
                               const Value& value) {
  std::string retval;
  for (auto& [name, member] : members) {
    if (!member)
      continue;
    std::string member_value = value(ptr_of(member));
    if (member_value.empty())
      continue;
    if (retval.empty())
      retval = std::move(member_value);
    else {
      retval.append(separator);
      retval.append(member_value);
    }
  }
  return retval;
}

/**
 *  Compute a macro on the objects it designates: the object of the context
 *  ($X$), a named object ($X:name$, $X:host:service$) or the members of a
 *  group ($X:group:separator$). The reason of a failure is logged.
 *
 *  The configuration forbids empty names, so an empty argument never
 *  designates an object: it means the object of the context, or it is an
 *  error.
 *
 *  @param[in]  mac    The macros, with the objects of the context.
 *  @param[in]  name   The macro name, for the logs.
 *  @param[in]  fam    The kind of object the macro is computed on.
 *  @param[in]  arg1   Argument 1.
 *  @param[in]  arg2   Argument 2.
 *  @param[in]  value  Computes the value on one object, as
 *                     std::string(const macro_target&). A template parameter
 *                     rather than a function reference, so that it is
 *                     inlined.
 *  @param[out] output The value of the macro.
 *
 *  @return OK on success.
 */
template <typename Value>
static int resolve(macro_context* mac,
                   std::string_view name,
                   family fam,
                   std::string_view arg1,
                   std::string_view arg2,
                   const Value& value,
                   std::string& output) {
  /* The object of the context, or the object called object_name. */
  auto single = [&](auto* obj, family kind,
                    std::string_view object_name) -> int {
    if (!obj) {
      if (object_name.empty())
        macros_logger->debug("macro {}: no {} in the context", name,
                             family_name(kind));
      else
        macros_logger->debug("macro {}: {} '{}' not found", name,
                             family_name(kind), object_name);
      return ERROR;
    }
    output = value(obj);
    return OK;
  };
  /* $X::separator$: the group name is missing. */
  auto no_group_name = [&]() -> int {
    macros_logger->debug(
        "macro {}: a group name is expected before the separator '{}'", name,
        arg2);
    return ERROR;
  };
  /* The members of the group called arg1. */
  auto group = [&](auto* grp, family kind, const auto& members) -> int {
    if (!grp) {
      macros_logger->debug("macro {}: {} '{}' not found", name,
                           family_name(kind), arg1);
      return ERROR;
    }
    output = concatenate(members(*grp), arg2, value);
    return OK;
  };

  switch (fam) {
    case family::host:
      if (arg2.empty()) {
        if (arg1.empty())
          return single(mac->host_ptr, family::host, arg1);
        return single(find_object(host::hosts, arg1), family::host, arg1);
      }
      if (arg1.empty())
        return no_group_name();
      return group(find_object(hostgroup::hostgroups, arg1), family::hostgroup,
                   [](const hostgroup& hg) -> auto& { return hg.members; });

    case family::hostgroup:
      if (arg1.empty())
        return single(mac->hostgroup_ptr, family::hostgroup, arg1);
      return single(find_object(hostgroup::hostgroups, arg1), family::hostgroup,
                    arg1);

    case family::service: {
      if (arg1.empty()) {
        if (arg2.empty())
          return single(mac->service_ptr, family::service, arg1);
        if (!mac->host_ptr)
          return single(mac->host_ptr, family::host, arg1);
        service* svc = find_object(
            service::services,
            std::make_pair(std::string_view(mac->host_ptr->name()), arg2));
        if (!svc) {
          macros_logger->debug("macro {}: service '{}/{}' not found", name,
                               mac->host_ptr->name(), arg2);
          return ERROR;
        }
        output = value(svc);
        return OK;
      }
      if (arg2.empty()) {
        macros_logger->debug(
            "macro {}: a service description or a servicegroup separator is "
            "expected after '{}'",
            name, arg1);
        return ERROR;
      }
      if (service* svc =
              find_object(service::services, std::make_pair(arg1, arg2))) {
        output = value(svc);
        return OK;
      }
      if (servicegroup* sg = find_object(servicegroup::servicegroups, arg1))
        return group(sg, family::servicegroup,
                     [](const servicegroup& g) -> auto& { return g.members; });
      macros_logger->debug(
          "macro {}: neither service '{}/{}' nor servicegroup '{}' found", name,
          arg1, arg2, arg1);
      return ERROR;
    }

    case family::servicegroup:
      if (arg1.empty())
        return single(mac->servicegroup_ptr, family::servicegroup, arg1);
      return single(find_object(servicegroup::servicegroups, arg1),
                    family::servicegroup, arg1);

    case family::contact:
      if (arg2.empty()) {
        if (arg1.empty())
          return single(mac->contact_ptr, family::contact, arg1);
        return single(find_object(contact::contacts, arg1), family::contact,
                      arg1);
      }
      if (arg1.empty())
        return no_group_name();
      return group(
          find_object(contactgroup::contactgroups, arg1), family::contactgroup,
          [](const contactgroup& cg) -> auto& { return cg.get_members(); });

    case family::contactgroup:
      if (arg1.empty())
        return single(mac->contactgroup_ptr, family::contactgroup, arg1);
      return single(find_object(contactgroup::contactgroups, arg1),
                    family::contactgroup, arg1);

    default:
      return ERROR;
  }
}

/**
 *  Get a custom macro ($_HOSTFOO$, $_SERVICEFOO$, $_CONTACTFOO$). A missing
 *  variable gives an empty value and is logged.
 *
 *  @param[in]  mac        Macro object.
 *  @param[in]  macro_name The macro name, without its arguments.
 *  @param[in]  arg1       Argument 1.
 *  @param[in]  arg2       Argument 2.
 *  @param[out] output     Output buffer.
 *
 *  @return OK on success.
 */
static int grab_custom_macro_value_r(macro_context* mac,
                                     std::string_view macro_name,
                                     std::string_view arg1,
                                     std::string_view arg2,
                                     std::string& output) {
  family fam;
  std::string_view name;
  if (absl::StartsWith(macro_name, "_HOST")) {
    fam = family::host;
    name = macro_name.substr(5);
  } else if (absl::StartsWith(macro_name, "_SERVICE")) {
    fam = family::service;
    name = macro_name.substr(8);
  } else if (absl::StartsWith(macro_name, "_CONTACT")) {
    fam = family::contact;
    name = macro_name.substr(8);
  } else
    return ERROR;

  return resolve(
      mac, macro_name, fam, arg1, arg2,
      [fam, macro_name, name](const macro_target& t) -> std::string {
        const map_customvar& vars =
            fam == family::host ? std::get<host*>(t)->custom_variables
            : fam == family::service
                ? std::get<service*>(t)->custom_variables
                : std::get<contact*>(t)->get_custom_variables();
        if (!name.empty())
          for (auto& [key, var] : vars)
            if (key == name)
              return var.value();
        if (fam == family::host)
          macros_logger->debug("macro {}: host '{}' has no variable '{}'",
                               macro_name, std::get<host*>(t)->name(), name);
        else if (fam == family::service) {
          const service* svc = std::get<service*>(t);
          macros_logger->debug("macro {}: service '{}/{}' has no variable '{}'",
                               macro_name, svc->get_hostname(),
                               svc->description(), name);
        } else
          macros_logger->debug("macro {}: contact '{}' has no variable '{}'",
                               macro_name, std::get<contact*>(t)->get_name(),
                               name);
        return std::string();
      },
      output);
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
static int handle_notification_macro(macro_context* mac,
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
static int handle_datetime_macro(macro_context* mac,
                                 const descriptor& desc,
                                 const std::string_view& arg1,
                                 const std::string_view& arg2,
                                 std::string& output,
                                 int* free_macro) {
  macros::datetime_target target;
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
static int handle_static_macro(macro_context* mac,
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
static int handle_summary_macro(macro_context* mac,
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

namespace {
/**
 * @brief Tell whether the problem of a host or a service is handled: it is
 * acknowledged, in downtime or its active checks are disabled.
 *
 * @param n The host or the service.
 *
 * @return true if the problem is handled.
 */
bool problem_is_handled(const notifier& n) {
  return n.get_scheduled_downtime_depth() > 0 ||
         n.problem_has_been_acknowledged() || !n.active_checks_enabled();
}
}  // namespace

/**
 *  Compute all the summary macros ($TOTAL*$) and cache them in the macros,
 *  if not already done.
 *
 *  @param[in,out] mac The macros.
 */
void generate_summary_macros(macro_context* mac) {
  if (mac->summary)
    return;

  // Get host totals.
  uint32_t hosts_up = 0;
  uint32_t hosts_down = 0;
  uint32_t hosts_down_unhandled = 0;
  uint32_t hosts_unreachable = 0;
  uint32_t hosts_unreachable_unhandled = 0;
  for (const auto& [name, hst] : host::hosts) {
    // Filter totals based on contact if necessary.
    if (mac->contact_ptr &&
        !is_contact_for_notifier(hst.get(), mac->contact_ptr))
      continue;
    switch (hst->get_current_state()) {
      case host::state_up:
        if (hst->has_been_checked())
          hosts_up++;
        break;
      case host::state_down:
        hosts_down++;
        if (!problem_is_handled(*hst))
          hosts_down_unhandled++;
        break;
      case host::state_unreachable:
        hosts_unreachable++;
        if (!problem_is_handled(*hst))
          hosts_unreachable_unhandled++;
        break;
    }
  }

  // Get service totals.
  uint32_t services_ok = 0;
  uint32_t services_warning = 0;
  uint32_t services_warning_unhandled = 0;
  uint32_t services_critical = 0;
  uint32_t services_critical_unhandled = 0;
  uint32_t services_unknown = 0;
  uint32_t services_unknown_unhandled = 0;
  for (const auto& [key, svc] : service::services) {
    // Filter totals based on contact if necessary.
    if (mac->contact_ptr &&
        !is_contact_for_notifier(svc.get(), mac->contact_ptr))
      continue;
    const service::service_state state = svc->get_current_state();
    if (state == service::state_ok) {
      if (svc->has_been_checked())
        services_ok++;
      continue;
    }
    // The problem of a service whose host is down or unreachable is handled.
    const host* hst = svc->get_host_ptr();
    const bool unhandled =
        !problem_is_handled(*svc) &&
        !(hst && (hst->get_current_state() == host::state_down ||
                  hst->get_current_state() == host::state_unreachable));
    switch (state) {
      case service::state_warning:
        services_warning++;
        if (unhandled)
          services_warning_unhandled++;
        break;
      case service::state_critical:
        services_critical++;
        if (unhandled)
          services_critical_unhandled++;
        break;
      case service::state_unknown:
        services_unknown++;
        if (unhandled)
          services_unknown_unhandled++;
        break;
      default:
        break;
    }
  }

  // These macros are time-intensive to compute, and will likely be
  // reused, so save them all for future use, in the order of the macro ids.
  mac->summary = std::array<uint32_t, MACRO_SUMMARY_COUNT>{
      hosts_up,
      hosts_down,
      hosts_unreachable,
      hosts_down_unhandled,
      hosts_unreachable_unhandled,
      hosts_down + hosts_unreachable,
      hosts_down_unhandled + hosts_unreachable_unhandled,
      services_ok,
      services_warning,
      services_critical,
      services_unknown,
      services_warning_unhandled,
      services_critical_unhandled,
      services_unknown_unhandled,
      services_warning + services_critical + services_unknown,
      services_warning_unhandled + services_critical_unhandled +
          services_unknown_unhandled};
}

/* this is the big one */
int grab_macro_value_r(macro_context* mac,
                       const std::string_view& macro_name,
                       std::string& output,
                       int* clean_options,
                       int* free_macro) {
  std::string_view buf;
  std::string_view arg[2];

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

    result = resolve(
        mac, buf, family::contact, arg[0], arg[1],
        [x, buf](const macro_target& t) -> std::string {
          if (x >= MAX_CONTACT_ADDRESSES) {
            macros_logger->debug(
                "macro {}: the address number must be between 1 and {}", buf,
                MAX_CONTACT_ADDRESSES);
            return std::string();
          }
          /* the contact may have less than MAX_CONTACT_ADDRESSES addresses */
          const contact* cntct = std::get<contact*>(t);
          if (x < cntct->get_addresses().size())
            return cntct->get_address(x);
          return std::string();
        },
        output);
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
int grab_macrox_value_r(macro_context* mac,
                        const descriptor& desc,
                        const std::string_view& arg1,
                        const std::string_view& arg2,
                        std::string& output,
                        int* free_macro) {
  if (!mac || !free_macro)
    return ERROR;

  switch (desc.fam) {
    case family::host:
    case family::hostgroup:
    case family::service:
    case family::servicegroup:
    case family::contact:
    case family::contactgroup:
      *free_macro = true;
      return resolve(
          mac, desc.name, desc.fam, arg1, arg2,
          [&desc, mac](const macro_target& t) { return desc.get(t, mac); },
          output);
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
