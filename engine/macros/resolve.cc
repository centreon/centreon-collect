/**
 * Copyright 2011-2026 Centreon
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

#include <absl/strings/match.h>

#include "engine/macros/resolve.hh"

#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/servicegroup.hh"
#include "com/centreon/engine/string.hh"
#include "engine/macros/process.hh"

namespace com::centreon::engine::macros {

namespace {

/**
 *  The raw pointer of a map value.
 */
template <typename T>
T* ptr_of(T* p) {
  return p;
}

template <typename P>
auto* ptr_of(const P& p) {
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
auto find_object(const Map& map, const Key& key)
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
std::string_view family_name(family fam) {
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
std::string concatenate(const Members& members,
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
 *
 *  @return The value of the macro, empty on failure.
 */
template <typename Value>
std::string resolve(macro_context& mac,
                    std::string_view name,
                    family fam,
                    std::string_view arg1,
                    std::string_view arg2,
                    const Value& value) {
  /* The object of the context, or the object called object_name. */
  auto single = [&](auto* obj, family kind,
                    std::string_view object_name) -> std::string {
    if (!obj) {
      if (object_name.empty())
        macros_logger->debug("macro {}: no {} in the context", name,
                             family_name(kind));
      else
        macros_logger->debug("macro {}: {} '{}' not found", name,
                             family_name(kind), object_name);
      return std::string();
    }
    return value(obj);
  };
  /* $X::separator$: the group name is missing. */
  auto no_group_name = [&]() -> std::string {
    macros_logger->debug(
        "macro {}: a group name is expected before the separator '{}'", name,
        arg2);
    return std::string();
  };
  /* The members of the group called arg1. */
  auto group = [&](auto* grp, family kind, const auto& members) -> std::string {
    if (!grp) {
      macros_logger->debug("macro {}: {} '{}' not found", name,
                           family_name(kind), arg1);
      return std::string();
    }
    return concatenate(members(*grp), arg2, value);
  };

  switch (fam) {
    case family::host:
      if (arg2.empty()) {
        if (arg1.empty())
          return single(mac.host_ptr, family::host, arg1);
        return single(find_object(host::hosts, arg1), family::host, arg1);
      }
      if (arg1.empty())
        return no_group_name();
      return group(find_object(hostgroup::hostgroups, arg1), family::hostgroup,
                   [](const hostgroup& hg) -> auto& { return hg.members; });

    case family::hostgroup:
      if (arg1.empty())
        return single(mac.hostgroup_ptr, family::hostgroup, arg1);
      return single(find_object(hostgroup::hostgroups, arg1), family::hostgroup,
                    arg1);

    case family::service: {
      if (arg1.empty()) {
        if (arg2.empty())
          return single(mac.service_ptr, family::service, arg1);
        if (!mac.host_ptr)
          return single(mac.host_ptr, family::host, arg1);
        service* svc = find_object(
            service::services,
            std::make_pair(std::string_view(mac.host_ptr->name()), arg2));
        if (!svc) {
          macros_logger->debug("macro {}: service '{}/{}' not found", name,
                               mac.host_ptr->name(), arg2);
          return std::string();
        }
        return value(svc);
      }
      if (arg2.empty()) {
        macros_logger->debug(
            "macro {}: a service description or a servicegroup separator is "
            "expected after '{}'",
            name, arg1);
        return std::string();
      }
      if (service* svc =
              find_object(service::services, std::make_pair(arg1, arg2)))
        return value(svc);
      if (servicegroup* sg = find_object(servicegroup::servicegroups, arg1))
        return group(sg, family::servicegroup,
                     [](const servicegroup& g) -> auto& { return g.members; });
      macros_logger->debug(
          "macro {}: neither service '{}/{}' nor servicegroup '{}' found", name,
          arg1, arg2, arg1);
      return std::string();
    }

    case family::servicegroup:
      if (arg1.empty())
        return single(mac.servicegroup_ptr, family::servicegroup, arg1);
      return single(find_object(servicegroup::servicegroups, arg1),
                    family::servicegroup, arg1);

    case family::contact:
      if (arg2.empty()) {
        if (arg1.empty())
          return single(mac.contact_ptr, family::contact, arg1);
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
        return single(mac.contactgroup_ptr, family::contactgroup, arg1);
      return single(find_object(contactgroup::contactgroups, arg1),
                    family::contactgroup, arg1);

    default:
      return std::string();
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
 *
 *  @return The value of the macro, empty on failure.
 */
std::string custom_variable_value(macro_context& mac,
                                  std::string_view macro_name,
                                  std::string_view arg1,
                                  std::string_view arg2) {
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
  } else {
    macros_logger->debug("macro {}: unknown custom macro", macro_name);
    return std::string();
  }

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
      });
}

/**
 *  Get a date/time macro. $ISVALIDTIME$ and $NEXTVALIDTIME$ take a
 *  timeperiod name and an optional timestamp, the others use the current
 *  time.
 *
 *  @param[in] mac  The macros.
 *  @param[in] desc Macro to get.
 *  @param[in] arg1 Argument 1.
 *  @param[in] arg2 Argument 2.
 *
 *  @return The value of the macro, empty on failure.
 */
std::string datetime_value(macro_context& mac,
                           const descriptor& desc,
                           std::string_view arg1,
                           std::string_view arg2) {
  macros::datetime_target target;
  target.when = time(nullptr);
  if (desc.id == MACRO_ISVALIDTIME || desc.id == MACRO_NEXTVALIDTIME) {
    // Find the timeperiod.
    auto it = ::timeperiods.find(arg1);
    if (it == ::timeperiods.end() || !it->second) {
      macros_logger->debug("macro {}: timeperiod '{}' not found", desc.name,
                           arg1);
      return std::string();
    }
    target.tp = it->second.get();

    // An optional timestamp, the current time otherwise.
    time_t test_time;
    if (!arg2.empty() && absl::SimpleAtoi(arg2, &test_time))
      target.when = test_time;
  }

  return desc.get(target, mac);
}

/**
 *  Get the value of a macro whose name is not fixed, so that it cannot be in
 *  the descriptors table: $ARGn$, $CONTACTADDRESSn$, the custom macros and
 *  the user macros of the resource file ($USERn$ and the other ones), read
 *  in the configuration. The reason of a failure is logged.
 *
 *  @param[in] mac        The macros.
 *  @param[in] macro_name The whole macro, with its arguments.
 *  @param[in] buf        The macro name, without its arguments.
 *  @param[in] arg1       Argument 1.
 *  @param[in] arg2       Argument 2.
 *
 *  @return The value of the macro, empty on failure.
 */
std::string variable_name_value(macro_context& mac,
                                std::string_view macro_name,
                                std::string_view buf,
                                std::string_view arg1,
                                std::string_view arg2) {
  unsigned int x;

  // $ARGn$: the arguments of the command, already processed by
  // macro_context::set_arguments().
  if (macro_name.size() > 3 && macro_name.compare(0, 3, "ARG") == 0) {
    if (!absl::SimpleAtoi(macro_name.substr(3), &x)) {
      macros_logger->error(
          "Error: could not grab macro value : '{}' must be a positive integer",
          macro_name.substr(3));
      return std::string();
    }

    if (!x || x > MAX_COMMAND_ARGUMENTS) {
      macros_logger->debug("macro {}: the number must be between 1 and {}",
                           macro_name, MAX_COMMAND_ARGUMENTS);
      return std::string();
    }

    return mac.argv[x - 1];
  }

  // $CONTACTADDRESSn$: the n-th address of the contact.
  if (buf.size() > 14 && buf.compare(0, 14, "CONTACTADDRESS") == 0) {
    // The number is read in the name without its arguments.
    if (!absl::SimpleAtoi(buf.substr(14), &x)) {
      macros_logger->error(
          "Error: could not grab macro value : '{}' must be a positive integer",
          buf.substr(14));
      return std::string();
    }
    x -= 1;

    return resolve(mac, buf, family::contact, arg1, arg2,
                   [x, buf](const macro_target& t) -> std::string {
                     if (x >= MAX_CONTACT_ADDRESSES) {
                       macros_logger->debug(
                           "macro {}: the address number must be between 1 "
                           "and {}",
                           buf, MAX_CONTACT_ADDRESSES);
                       return std::string();
                     }
                     // A contact may have fewer addresses.
                     const contact* cntct = std::get<contact*>(t);
                     if (x < cntct->get_addresses().size())
                       return cntct->get_address(x);
                     return std::string();
                   });
  }

  // $_HOSTfoo$, $_SERVICEfoo$, $_CONTACTfoo$: the custom variables.
  if (macro_name[0] == '_')
    return custom_variable_value(mac, buf, arg1, arg2);

  // The user macros of the resource file, $USERn$ and the other ones.
  const auto& users = pb_indexed_config.state().users();
  auto it = users.find(macro_name);
  if (it != users.end())
    return it->second;

  // An unknown macro.
  macros_logger->trace(" WARNING: Could not find a macro matching '{}'!",
                       macro_name);
  return std::string();
}

/**
 *  Decrypt the value of a macro if the credentials are encrypted: a "raw::"
 *  value is kept as is without its prefix, an "encrypt::" value is
 *  decrypted. On failure, the value is not changed.
 *
 *  @param[in]     macro_name The macro, for the logs.
 *  @param[in,out] value      The value of the macro.
 */
void decrypt_macro_value(std::string_view macro_name, std::string& value) {
  if (!pb_indexed_config.state().credentials_encryption())
    return;
  if (!value.compare(0, 5, "raw::")) {
    value.erase(0, 5);
  } else if (!value.compare(0, 9, "encrypt::")) {
    if (!credentials_decrypt) {
      SPDLOG_LOGGER_ERROR(macros_logger,
                          "no encryption configured => can't decryp macro {}",
                          macro_name);
      return;
    }
    try {
      value = credentials_decrypt->decrypt(std::string_view(value).substr(9));
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(macros_logger,
                          "fail to decrypt content of macro {}: {}", macro_name,
                          e.what());
    }
  }
}

}  // namespace

/**
 *  Get the value of a macro, given without its '$': its name and its
 *  arguments separated by ':'. The reason of a failure is logged.
 *
 *  @param[in] mac           The macros.
 *  @param[in] macro_name    The macro, as "NAME", "NAME:arg1" or
 *                           "NAME:arg1:arg2".
 *  @param[in] strip_illegal Whether the illegal characters are removed from
 *                           the value. They are always removed from the
 *                           macros whose descriptor says so.
 *
 *  @return The value of the macro, empty on failure.
 */
std::string value(macro_context& mac,
                  std::string_view macro_name,
                  bool strip_illegal) {
  if (macro_name.empty())
    return std::string();

  std::string_view buf;
  std::string_view arg[2];

  /* split the name and its arguments */
  auto arr = absl::StrSplit(macro_name, ':');
  auto it = arr.begin();

  // The name, then at most two arguments.
  if (it != arr.end()) {
    buf = *it;
    ++it;
  }

  if (it != arr.end()) {
    arg[0] = *it;
    ++it;
  }
  if (it != arr.end()) {
    arg[1] = *it;
  }

  std::string retval;
  // A macro of the descriptors table, else a macro with a variable name.
  const descriptor* desc = find_descriptor(buf);
  if (desc) {
    macros_logger->trace("  macros[{}] ({}) match.", desc->id, desc->name);
    retval = table_value(mac, *desc, arg[0], arg[1]);
    if (desc->cleaned)
      strip_illegal = true;
  } else
    retval = variable_name_value(mac, macro_name, buf, arg[0], arg[1]);

  // some macros are encrypted?
  decrypt_macro_value(macro_name, retval);

  if (strip_illegal)
    strip_illegal_macro_chars(retval);
  return retval;
}

/**
 *  Get the value of a macro of the descriptors table. The reason of a
 *  failure is logged.
 *
 *  @param[in] mac  The macros.
 *  @param[in] desc Macro to get.
 *  @param[in] arg1 Argument 1.
 *  @param[in] arg2 Argument 2.
 *
 *  @return The value of the macro, empty on failure.
 */
std::string table_value(macro_context& mac,
                        const descriptor& desc,
                        std::string_view arg1,
                        std::string_view arg2) {
  switch (desc.fam) {
    case family::host:
    case family::hostgroup:
    case family::service:
    case family::servicegroup:
    case family::contact:
    case family::contactgroup:
      return resolve(
          mac, desc.name, desc.fam, arg1, arg2,
          [&desc, &mac](const macro_target& t) { return desc.get(t, mac); });
    case family::datetime:
      return datetime_value(mac, desc, arg1, arg2);
    case family::notification:
    case family::statics:
    case family::summary:
      // Computed from the macros, or precomputed for us.
      return desc.get(macro_target(), mac);
  }
  return std::string();
}

}  // namespace com::centreon::engine::macros
