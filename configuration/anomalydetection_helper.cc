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
#include "configuration/anomalydetection_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
anomalydetection_helper::anomalydetection_helper(Anomalydetection* obj)
    : message_helper(object_type::anomalydetection,
                     obj,
                     {
                         {"_HOST_ID", "host_id"},
                         {"_SERVICE_ID", "service_id"},
                         {"description", "service_description"},
                         {"service_groups", "servicegroups"},
                         {"contact_groups", "contactgroups"},
                         {"normal_check_interval", "check_interval"},
                         {"retry_check_interval", "retry_interval"},
                         {"active_checks_enabled", "checks_active"},
                         {"passive_checks_enabled", "checks_passive"},
                         {"severity", "severity_id"},
                     },
                     53) {
  init_anomalydetection(static_cast<Anomalydetection*>(mut_obj()));
}

bool anomalydetection_helper::hook(const absl::string_view& key,
                                   const absl::string_view& value) {
  Anomalydetection* obj = static_cast<Anomalydetection*>(mut_obj());
  if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "contacts") {
    fill_string_group(obj->mutable_contacts(), value);
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
  } else if (key == "servicegroups") {
    fill_string_group(obj->mutable_servicegroups(), value);
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
          if (it->second() == TagType::tag_servicecategory && it->first() == id)
            ++it;
          else {
            auto t = obj->add_tags();
            t->set_first(id);
            t->set_second(TagType::tag_servicecategory);
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
          if (it->second() == TagType::tag_servicegroup && it->first() == id)
            ++it;
          else {
            auto t = obj->add_tags();
            t->set_first(id);
            t->set_second(TagType::tag_servicegroup);
            break;
          }
        }
      }
    }
    return true;
  }
  return false;
}
void anomalydetection_helper::check_validity() const {
  const Anomalydetection* o = static_cast<const Anomalydetection*>(obj());

  if (o->obj().register_()) {
    if (o->service_description().empty())
      throw msg_fmt(
          "Anomaly detection has no name (property 'service_description')");
    if (o->host_name().empty())
      throw msg_fmt(
          "Anomaly detection '{}' has no host name (property 'host_name')",
          o->service_description());
    if (o->metric_name().empty())
      throw msg_fmt(
          "Anomaly detection '{}' has no metric name (property 'metric_name')",
          o->service_description());
    if (o->thresholds_file().empty())
      throw msg_fmt(
          "Anomaly detection '{}' has no thresholds file (property "
          "'thresholds_file')",
          o->service_description());
  }
}
}  // namespace com::centreon::engine::configuration
