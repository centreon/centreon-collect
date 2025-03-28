/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/serviceescalation_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

size_t serviceescalation_key(const Serviceescalation& se) {
  return absl::HashOf(se.hosts().data(0), se.service_description().data(0),
                      // se.contactgroups(),
                      se.escalation_options(), se.escalation_period(),
                      se.first_notification(), se.last_notification(),
                      se.notification_interval());
}

/**
 * @brief Constructor from a Serviceescalation object.
 *
 * @param obj The Serviceescalation object on which this helper works. The
 * helper is not the owner of this object.
 */
serviceescalation_helper::serviceescalation_helper(Serviceescalation* obj)
    : message_helper(object_type::serviceescalation,
                     obj,
                     {
                         {"host", "hosts"},
                         {"host_name", "hosts"},
                         {"description", "service_description"},
                         {"servicegroup", "servicegroups"},
                         {"servicegroup_name", "servicegroups"},
                         {"hostgroup", "hostgroups"},
                         {"hostgroup_name", "hostgroups"},
                         {"contact_groups", "contactgroups"},
                     },
                     Serviceescalation::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Serviceescalation objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool serviceescalation_helper::hook(std::string_view key,
                                    std::string_view value) {
  Serviceescalation* obj = static_cast<Serviceescalation*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key == "escalation_options") {
    uint32_t options = action_he_none;
    auto arr = absl::StrSplit(value, ',');
    for (auto& v : arr) {
      std::string_view vv = absl::StripAsciiWhitespace(v);
      if (vv == "w" || vv == "warning")
        options |= action_se_warning;
      else if (vv == "u" || vv == "unknown")
        options |= action_se_unknown;
      else if (vv == "c" || vv == "critical")
        options |= action_se_critical;
      else if (vv == "r" || vv == "recovery")
        options |= action_se_recovery;
      else if (vv == "n" || vv == "none")
        options = action_se_none;
      else if (vv == "a" || vv == "all")
        options = action_se_warning | action_se_unknown | action_se_critical |
                  action_se_recovery;
      else
        return false;
    }
    obj->set_escalation_options(options);
    set_changed(
        obj->descriptor()->FindFieldByName("escalation_options")->index());
    return true;
  } else if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "hosts") {
    fill_string_group(obj->mutable_hosts(), value);
    return true;
  } else if (key == "servicegroups") {
    fill_string_group(obj->mutable_servicegroups(), value);
    return true;
  } else if (key == "service_description") {
    fill_string_group(obj->mutable_service_description(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Serviceescalation object.
 *
 * @param err An error counter.
 */
void serviceescalation_helper::check_validity(error_cnt& err) const {
  const Serviceescalation* o = static_cast<const Serviceescalation*>(obj());

  if (o->servicegroups().data().empty()) {
    if (o->service_description().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service escalation is not attached to "
          "any service or service group (properties "
          "'service_description' and 'servicegroup_name', "
          "respectively)");
    } else if (o->hosts().data().empty() && o->hostgroups().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service escalation is not attached to "
          "any host or host group (properties 'host_name' or "
          "'hostgroup_name', respectively)");
    }
  }
}

/**
 * @brief Initializer of the Serviceescalation object, in other words set its
 * default values.
 */
void serviceescalation_helper::_init() {
  Serviceescalation* obj = static_cast<Serviceescalation*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_escalation_options(action_se_none);
  obj->set_first_notification(-2);
  obj->set_last_notification(-2);
  obj->set_notification_interval(0);
}

/**
 * @brief Expand the Serviceescalation object.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void serviceescalation_helper::expand(
    configuration::State& s,
    configuration::error_cnt& err,
    const absl::flat_hash_map<std::string_view, configuration::Hostgroup*>&
        hostgroups,
    const absl::flat_hash_map<std::string_view, configuration::Servicegroup*>&
        servicegroups) {
  std::list<std::unique_ptr<Serviceescalation>> resolved;

  for (auto& se : *s.mutable_serviceescalations()) {
    /* A set of all the hosts related to this escalation */
    absl::flat_hash_set<std::string> host_names;
    for (auto& hname : se.hosts().data())
      host_names.insert(hname);
    if (se.hostgroups().data().size() > 0) {
      for (auto& hg_name : se.hostgroups().data()) {
        auto found_hg = hostgroups.find(hg_name);
        if (found_hg != hostgroups.end()) {
          for (auto& h : found_hg->second->members().data())
            host_names.emplace(h);
        } else {
          err.config_errors++;
          throw msg_fmt("Could not expand non-existing host group '{}'",
                        hg_name);
        }
      }
    }

    /* A set of all the pairs (hostname, service-description) impacted by this
     * escalation. */
    absl::flat_hash_set<std::pair<std::string, std::string>> expanded;
    for (auto& hn : host_names) {
      for (auto& sn : se.service_description().data())
        expanded.emplace(hn, sn);
    }

    for (auto& sg_name : se.servicegroups().data()) {
      auto found = servicegroups.find(sg_name);
      if (found == servicegroups.end()) {
        err.config_errors++;
        throw msg_fmt("Could not resolve service group '{}'", sg_name);
      }

      for (auto& m : found->second->members().data())
        expanded.emplace(m.first(), m.second());
    }
    se.mutable_hostgroups()->clear_data();
    se.mutable_hosts()->clear_data();
    se.mutable_servicegroups()->clear_data();
    se.mutable_service_description()->clear_data();
    for (auto& p : expanded) {
      resolved.emplace_back(std::make_unique<Serviceescalation>());
      auto& e = resolved.back();
      e->CopyFrom(se);
      fill_string_group(e->mutable_hosts(), p.first);
      fill_string_group(e->mutable_service_description(), p.second);
    }
  }
  s.clear_serviceescalations();
  for (auto& e : resolved)
    s.mutable_serviceescalations()->AddAllocated(e.release());
}
}  // namespace com::centreon::engine::configuration
