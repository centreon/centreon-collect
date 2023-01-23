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
  _init();
}

/**
 * @brief For several keys, the parser of Host objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
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
    auto tags{absl::StrSplit(value, ',')};
    bool ret = true;

    for (auto it = obj->tags().begin(); it != obj->tags().end();) {
      if (it->second() == TagType::tag_hostcategory)
        it = obj->mutable_tags()->erase(it);
      else
        ++it;
    }

    for (auto& tag : tags) {
      uint64_t id;
      bool parse_ok;
      parse_ok = absl::SimpleAtoi(tag, &id);
      if (parse_ok) {
        auto t = obj->add_tags();
        t->set_first(id);
        t->set_second(TagType::tag_hostcategory);
      } else {
        ret = false;
      }
    }
    return ret;
  } else if (key == "group_tags") {
    auto tags{absl::StrSplit(value, ',')};
    bool ret = true;

    for (auto it = obj->tags().begin(); it != obj->tags().end();) {
      if (it->second() == TagType::tag_hostgroup)
        it = obj->mutable_tags()->erase(it);
      else
        ++it;
    }

    for (auto& tag : tags) {
      uint64_t id;
      bool parse_ok;
      parse_ok = absl::SimpleAtoi(tag, &id);
      if (parse_ok) {
        auto t = obj->add_tags();
        t->set_first(id);
        t->set_second(TagType::tag_hostgroup);
      } else {
        ret = false;
      }
    }
    return ret;
  }
  return false;
}

/**
 * @brief Check the validity of the Host object.
 */
void host_helper::check_validity() const {
  const Host* o = static_cast<const Host*>(obj());

  if (o->host_name().empty())
    throw msg_fmt("Host has no name (property 'host_name')");
  if (o->address().empty())
    throw msg_fmt("Host '{}' has no address (property 'address')",
                  o->host_name());
}
void host_helper::_init() {
  Host* obj = static_cast<Host*>(mut_obj());
  obj->set_checks_active(true);
  obj->set_checks_passive(true);
  obj->set_check_freshness(false);
  obj->set_check_interval(5);
  obj->set_event_handler_enabled(true);
  obj->set_first_notification_delay(0);
  obj->set_flap_detection_enabled(true);
  obj->set_flap_detection_options(action_hst_up | action_hst_down |
                                  action_hst_unreachable);
  obj->set_freshness_threshold(0);
  obj->set_high_flap_threshold(0);
  obj->set_initial_state(engine::host::state_up);
  obj->set_low_flap_threshold(0);
  obj->set_max_check_attempts(3);
  obj->set_notifications_enabled(true);
  obj->set_notification_interval(30);
  obj->set_notification_options(action_hst_up | action_hst_down |
                                action_hst_unreachable | action_hst_flapping |
                                action_hst_downtime);
  obj->set_obsess_over_host(true);
  obj->set_process_perf_data(true);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_retry_interval(1);
  obj->set_stalking_options(action_hst_none);
}

}  // namespace com::centreon::engine::configuration