/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "configuration/host_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
host_helper::host_helper(Host* obj)
    : message_helper(object_type::host,
                     obj,
                     {
                         {"_HOST_ID", "host_id"},
                         {"host_groups", "hostgroups"},
                         {"contact_groups", "contactgroups"},
                         {"gd2_image", "statusmap_image"},
                         {"normal_check_interval", "check_interval"},
                         {"retry_check_interval", "retry_interval"},
                         {"checks_enabled", "checks_active"},
                         {"active_checks_enabled", "checks_active"},
                         {"passive_checks_enabled", "checks_passive"},
                         {"2d_coords", "coords_2d"},
                         {"3d_coords", "coords_3d"},
                         {"severity", "severity_id"},
                     },
                     53) {
  init_host(static_cast<Host*>(mut_obj()));
}

bool host_helper::hook(const absl::string_view& key,
                       const absl::string_view& value) {
  Host* obj = static_cast<Host*>(mut_obj());
  if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "contacts") {
    fill_string_group(obj->mutable_contacts(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "notification_options") {
    uint16_t options(action_svc_none);
    auto values = absl::StrSplit(value, ',');
    for (auto it = values.begin(); it != values.end(); ++it) {
      absl::string_view v = absl::StripAsciiWhitespace(*it);
      if (v == "u" || v == "unknown")
        options |= action_svc_unknown;
      else if (v == "w" || v == "warning")
        options |= action_svc_warning;
      else if (v == "c" || v == "critical")
        options |= action_svc_critical;
      else if (v == "r" || v == "recovery")
        options |= action_svc_ok;
      else if (v == "f" || v == "flapping")
        options |= action_svc_flapping;
      else if (v == "s" || v == "downtime")
        options |= action_svc_downtime;
      else if (v == "n" || v == "none")
        options = action_svc_none;
      else if (v == "a" || v == "all")
        options = action_svc_unknown | action_svc_warning |
                  action_svc_critical | action_svc_ok | action_svc_flapping |
                  action_svc_downtime;
      else
        return false;
    }
    obj->set_notification_options(options);
    return true;
  } else if (key == "parents") {
    fill_string_group(obj->mutable_parents(), value);
    return true;
  } else if (key == "category_tags") {
    std::list<absl::string_view> tags{absl::StrSplit(value, ',')};

    for (auto& tag : tags) {
      uint64_t id;
      bool parse_ok;
      parse_ok = absl::SimpleAtoi(tag, &id);
      if (parse_ok) {
        for (auto it = obj->mutable_tags()->begin();
             it != obj->mutable_tags()->end();) {
          if (it->second() == TagType::tag_hostcategory && it->first() == id)
            ++it;
          else {
            auto t = obj->add_tags();
            t->set_first(id);
            t->set_second(TagType::tag_hostcategory);
            break;
          }
        }
      }
    }
    return true;
  } else if (key == "group_tags") {
    std::list<absl::string_view> tags{absl::StrSplit(value, ',')};

    for (auto& tag : tags) {
      uint64_t id;
      bool parse_ok;
      parse_ok = absl::SimpleAtoi(tag, &id);
      if (parse_ok) {
        for (auto it = obj->mutable_tags()->begin();
             it != obj->mutable_tags()->end();) {
          if (it->second() == TagType::tag_hostgroup && it->first() == id)
            ++it;
          else {
            auto t = obj->add_tags();
            t->set_first(id);
            t->set_second(TagType::tag_hostgroup);
            break;
          }
        }
      }
    }
    return true;
  }
  return false;
}
void host_helper::check_validity() const {
  const Host* o = static_cast<const Host*>(obj());

  if (o->host_name().empty())
    throw msg_fmt("Host has no name (property 'host_name')");
  if (o->address().empty())
    throw msg_fmt("Host '{}' has no address (property 'address')",
                  o->host_name());
}
}  // namespace com::centreon::engine::configuration
