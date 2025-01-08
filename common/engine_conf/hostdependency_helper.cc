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
#include "common/engine_conf/hostdependency_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Builds a key from a Hostdependency message. This is useful to check
 * modifications in hostdependencies.
 *
 * @param hd The Hostdependency object to use to build the key.
 *
 * @return A number of type size_t.
 */
size_t hostdependency_key(const Hostdependency& hd) {
  assert(hd.hosts().data().size() == 1 && hd.hostgroups().data().empty() &&
         hd.dependent_hosts().data().size() == 1 &&
         hd.dependent_hostgroups().data().empty());
  return absl::HashOf(hd.dependency_period(), hd.dependency_type(),
                      hd.dependent_hosts().data(0), hd.hosts().data(0),
                      hd.inherits_parent(), hd.notification_failure_options());
}

/**
 * @brief Constructor from a Hostdependency object.
 *
 * @param obj The Hostdependency object on which this helper works. The helper
 * is not the owner of this object.
 */
hostdependency_helper::hostdependency_helper(Hostdependency* obj)
    : message_helper(
          object_type::hostdependency,
          obj,
          {
              {"hostgroup", "hostgroups"},
              {"hostgroup_name", "hostgroups"},
              {"host", "hosts"},
              {"host_name", "hosts"},
              {"master_host", "hosts"},
              {"master_host_name", "hosts"},
              {"dependent_hostgroup", "dependent_hostgroups"},
              {"dependent_hostgroup_name", "dependent_hostgroups"},
              {"dependent_host", "dependent_hosts"},
              {"dependent_host_name", "dependent_hosts"},
              {"notification_failure_criteria", "notification_failure_options"},
              {"execution_failure_criteria", "execution_failure_options"},
          },
          Hostdependency::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Hostdependency objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool hostdependency_helper::hook(std::string_view key, std::string_view value) {
  Hostdependency* obj = static_cast<Hostdependency*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key == "notification_failure_options" ||
      key == "execution_failure_options") {
    auto opts = absl::StrSplit(value, ',');
    uint16_t options = action_hd_none;

    for (auto& o : opts) {
      std::string_view ov = absl::StripAsciiWhitespace(o);
      if (ov == "o" || ov == "up")
        options |= action_hd_up;
      else if (ov == "d" || ov == "down")
        options |= action_hd_down;
      else if (ov == "u" || ov == "unreachable")
        options |= action_hd_unreachable;
      else if (ov == "p" || ov == "pending")
        options |= action_hd_pending;
      else if (ov == "n" || ov == "none")
        options |= action_hd_none;
      else if (ov == "a" || ov == "all")
        options = action_hd_up | action_hd_down | action_hd_unreachable |
                  action_hd_pending;
      else
        return false;
    }
    if (key[0] == 'n')
      obj->set_notification_failure_options(options);
    else
      obj->set_execution_failure_options(options);
    return true;
  } else if (key == "dependent_hostgroups") {
    fill_string_group(obj->mutable_dependent_hostgroups(), value);
    return true;
  } else if (key == "dependent_hosts") {
    fill_string_group(obj->mutable_dependent_hosts(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "hosts") {
    fill_string_group(obj->mutable_hosts(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Hostdependency object.
 *
 * @param err An error counter.
 */
void hostdependency_helper::check_validity(error_cnt& err) const {
  const Hostdependency* o = static_cast<const Hostdependency*>(obj());

  if (o->hosts().data().empty() && o->hostgroups().data().empty()) {
    err.config_errors++;
    throw msg_fmt(
        "Host dependency is not attached to any host or host group (properties "
        "'hosts' or 'hostgroups', respectively)");
  }
  if (o->dependent_hosts().data().empty() &&
      o->dependent_hostgroups().data().empty()) {
    err.config_errors++;
    throw msg_fmt(
        "Host dependency is not attached to any "
        "dependent host or dependent host group (properties "
        "'dependent_hosts' or 'dependent_hostgroups', "
        "respectively)");
  }
}

/**
 * @brief Initializer of the Hostdependency object, in other words set its
 * default values.
 */
void hostdependency_helper::_init() {
  Hostdependency* obj = static_cast<Hostdependency*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_execution_failure_options(action_hd_none);
  obj->set_inherits_parent(false);
  obj->set_notification_failure_options(action_hd_none);
}

/**
 * @brief Expand the hostdependencies.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void hostdependency_helper::expand(
    State& s,
    error_cnt& err,
    absl::flat_hash_map<std::string, configuration::Hostgroup*>& m_hostgroups) {
  std::list<std::unique_ptr<configuration::Hostdependency> > lst;

  for (int i = s.hostdependencies_size() - 1; i >= 0; --i) {
    auto* hd_conf = s.mutable_hostdependencies(i);
    if (hd_conf->hosts().data().size() > 1 ||
        !hd_conf->hostgroups().data().empty() ||
        hd_conf->dependent_hosts().data().size() > 1 ||
        !hd_conf->dependent_hostgroups().data().empty() ||
        hd_conf->dependency_type() == unknown) {
      for (auto& hg_name : hd_conf->dependent_hostgroups().data()) {
        auto found = m_hostgroups.find(hg_name);
        if (found != m_hostgroups.end()) {
          auto& hg_conf = *found->second;
          for (auto& h : hg_conf.members().data())
            fill_string_group(hd_conf->mutable_dependent_hosts(), h);
        } else {
          err.config_errors++;
          throw msg_fmt("Host dependency dependent hostgroup '{}' not found",
                        hg_name);
        }
      }
      for (auto& hg_name : hd_conf->hostgroups().data()) {
        auto found = m_hostgroups.find(hg_name);
        if (found != m_hostgroups.end()) {
          auto& hg_conf = *found->second;
          for (auto& h : hg_conf.members().data())
            fill_string_group(hd_conf->mutable_hosts(), h);
        } else {
          err.config_errors++;
          throw msg_fmt("Host dependency hostgroup '{}' not found", hg_name);
        }
      }
      for (auto& h : hd_conf->hosts().data()) {
        for (auto& h_dep : hd_conf->dependent_hosts().data()) {
          for (int ii = 1; ii <= 2; ii++) {
            if (hd_conf->dependency_type() == DependencyKind::unknown ||
                static_cast<int32_t>(hd_conf->dependency_type()) == ii) {
              lst.emplace_back(std::make_unique<Hostdependency>());
              auto& new_hd = lst.back();
              new_hd->set_dependency_period(hd_conf->dependency_period());
              new_hd->set_inherits_parent(hd_conf->inherits_parent());
              fill_string_group(new_hd->mutable_hosts(), h);
              fill_string_group(new_hd->mutable_dependent_hosts(), h_dep);
              if (ii == 2) {
                new_hd->set_dependency_type(
                    DependencyKind::execution_dependency);
                new_hd->set_execution_failure_options(
                    hd_conf->execution_failure_options());
              } else {
                new_hd->set_dependency_type(
                    DependencyKind::notification_dependency);
                new_hd->set_notification_failure_options(
                    hd_conf->notification_failure_options());
              }
            }
          }
        }
      }
      s.mutable_hostdependencies()->DeleteSubrange(i, 1);
    }
  }
  for (auto& hd : lst)
    s.mutable_hostdependencies()->AddAllocated(hd.release());
}

}  // namespace com::centreon::engine::configuration
