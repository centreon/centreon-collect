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
#include "common/engine_conf/anomalydetection_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from an Anomalydetection object.
 *
 * @param obj The Anomalydetection object on which this helper works. The helper
 * is not the owner of this object, it just helps to initialize it.
 */
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
                     Anomalydetection::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Anomalydetection objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool anomalydetection_helper::hook(std::string_view key,
                                   std::string_view value) {
  Anomalydetection* obj = static_cast<Anomalydetection*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "contacts") {
    fill_string_group(obj->mutable_contacts(), value);
    return true;
  } else if (key == "flap_detection_options") {
    uint16_t options(action_svc_none);
    auto values = absl::StrSplit(value, ',');
    for (auto it = values.begin(); it != values.end(); ++it) {
      std::string_view v = absl::StripAsciiWhitespace(*it);
      if (v == "o" || v == "ok")
        options |= action_svc_ok;
      else if (v == "w" || v == "warning")
        options |= action_svc_warning;
      else if (v == "u" || v == "unknown")
        options |= action_svc_unknown;
      else if (v == "c" || v == "critical")
        options |= action_svc_critical;
      else if (v == "n" || v == "none")
        options |= action_svc_none;
      else if (v == "a" || v == "all")
        options = action_svc_ok | action_svc_warning | action_svc_unknown |
                  action_svc_critical;
      else
        return false;
    }
    obj->set_flap_detection_options(options);
    return true;
  } else if (key == "notification_options") {
    uint16_t options(action_svc_none);
    if (fill_service_notification_options(&options, value)) {
      obj->set_notification_options(options);
      return true;
    } else
      return false;
    obj->set_notification_options(options);
    return true;
  } else if (key == "servicegroups") {
    fill_string_group(obj->mutable_servicegroups(), value);
    return true;
  } else if (key == "stalking_options") {
    uint16_t options(action_svc_none);
    auto values = absl::StrSplit(value, ',');
    for (auto it = values.begin(); it != values.end(); ++it) {
      std::string_view v = absl::StripAsciiWhitespace(*it);
      if (v == "u" || v == "unknown")
        options |= action_svc_unknown;
      else if (v == "o" || v == "ok")
        options |= action_svc_ok;
      else if (v == "w" || v == "warning")
        options |= action_svc_warning;
      else if (v == "c" || v == "critical")
        options |= action_svc_critical;
      else if (v == "n" || v == "none")
        options = action_svc_none;
      else if (v == "a" || v == "all")
        options = action_svc_ok | action_svc_unknown | action_svc_warning |
                  action_svc_critical;
      else
        return false;
    }
    obj->set_stalking_options(options);
    return true;
  } else if (key == "category_tags") {
    auto tags{absl::StrSplit(value, ',')};
    bool ret = true;

    for (auto it = obj->tags().begin(); it != obj->tags().end();) {
      if (it->second() == TagType::tag_servicecategory)
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
        t->set_second(TagType::tag_servicecategory);
      } else {
        ret = false;
      }
    }
    return ret;
  } else if (key == "group_tags") {
    auto tags{absl::StrSplit(value, ',')};
    bool ret = true;

    for (auto it = obj->tags().begin(); it != obj->tags().end();) {
      if (it->second() == TagType::tag_servicegroup)
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
        t->set_second(TagType::tag_servicegroup);
      } else {
        ret = false;
      }
    }
    return ret;
  }
  return false;
}

/**
 * @brief Check the validity of the Anomalydetection object.
 *
 * @param err An error counter.
 */
void anomalydetection_helper::check_validity(error_cnt& err) const {
  const Anomalydetection* o = static_cast<const Anomalydetection*>(obj());

  if (o->obj().register_()) {
    if (o->service_description().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Anomaly detection has no name (property 'service_description')");
    }
    if (o->host_name().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Anomaly detection '{}' has no host name (property 'host_name')",
          o->service_description());
    }
    if (o->metric_name().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Anomaly detection '{}' has no metric name (property 'metric_name')",
          o->service_description());
    }
    if (o->thresholds_file().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Anomaly detection '{}' has no thresholds file (property "
          "'thresholds_file')",
          o->service_description());
    }
  }
}

/**
 * @brief Initializer of the Anomalydetection object, in other words set its
 * default values. Protobuf does not allow specific default values, so we fix
 * this with this method.
 */
void anomalydetection_helper::_init() {
  Anomalydetection* obj = static_cast<Anomalydetection*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_acknowledgement_timeout(0);
  obj->set_status_change(false);
  obj->set_checks_active(true);
  obj->set_checks_passive(true);
  obj->set_check_freshness(0);
  obj->set_check_interval(5);
  obj->set_event_handler_enabled(true);
  obj->set_first_notification_delay(0);
  obj->set_flap_detection_enabled(true);
  obj->set_flap_detection_options(action_svc_ok | action_svc_warning |
                                  action_svc_unknown | action_svc_critical);
  obj->set_freshness_threshold(0);
  obj->set_high_flap_threshold(0);
  obj->set_is_volatile(false);
  obj->set_low_flap_threshold(0);
  obj->set_max_check_attempts(3);
  obj->set_notifications_enabled(true);
  obj->set_notification_interval(0);
  obj->set_notification_options(action_svc_ok | action_svc_warning |
                                action_svc_critical | action_svc_unknown |
                                action_svc_flapping | action_svc_downtime);
  obj->set_obsess_over_service(true);
  obj->set_process_perf_data(true);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_retry_interval(1);
  obj->set_stalking_options(action_svc_none);
}

/**
 * @brief If the provided key/value have their parsing that failed previously,
 * it is possible they are a customvariable. A customvariable name has its
 * name starting with an underscore. This method checks the possibility to
 * store a customvariable in the given object and stores it if possible.
 *
 * @param key   The name of the customvariable.
 * @param value Its value as a string.
 *
 * @return True if the customvariable has been well stored.
 */
bool anomalydetection_helper::insert_customvariable(std::string_view key,
                                                    std::string_view value) {
  if (key[0] != '_')
    return false;

  key.remove_prefix(1);

  Anomalydetection* obj = static_cast<Anomalydetection*>(mut_obj());
  auto* cvs = obj->mutable_customvariables();
  for (auto& c : *cvs) {
    if (c.name() == key) {
      c.set_value(value.data(), value.size());
      return true;
    }
  }
  auto new_cv = cvs->Add();
  new_cv->set_name(key.data(), key.size());
  new_cv->set_value(value.data(), value.size());
  return true;
}
}  // namespace com::centreon::engine::configuration
