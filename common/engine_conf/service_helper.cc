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
#include "service_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/message_helper.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Service object.
 *
 * @param obj The Service object on which this helper works. The helper is not
 * the owner of this object.
 */
service_helper::service_helper(Service* obj)
    : message_helper(object_type::service,
                     obj,
                     {
                         {"host", "host_name"},
                         {"hosts", "host_name"},
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
                     Service::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Service objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool service_helper::hook(std::string_view key, std::string_view value) {
  Service* obj = static_cast<Service*>(mut_obj());
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
    set_changed(Service::descriptor()
                    ->FindFieldByName("flap_detection_options")
                    ->index());
    return true;
  } else if (key == "notification_options") {
    uint16_t options(action_svc_none);
    if (fill_service_notification_options(&options, value)) {
      obj->set_notification_options(options);
      set_changed(Service::descriptor()
                      ->FindFieldByName("notification_options")
                      ->index());
      return true;
    } else
      return false;
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
    set_changed(
        Service::descriptor()->FindFieldByName("stalking_options")->index());
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
 * @brief Check the validity of the Service object.
 *
 * @param err An error counter.
 */
void service_helper::check_validity(error_cnt& err) const {
  const Service* o = static_cast<const Service*>(obj());

  if (o->obj().register_()) {
    if (o->service_description().empty()) {
      err.config_errors++;
      throw msg_fmt("Services must have a non-empty description");
    }
    if (o->check_command().empty()) {
      err.config_errors++;
      throw msg_fmt("Service '{}' has an empty check command",
                    o->service_description());
    }
    if (o->host_name().empty()) {
      err.config_errors++;
      throw msg_fmt("Service '{}' must contain one host name",
                    o->service_description());
    }
  }
}

/**
 * @brief Initializer of the Service object, in other words set its default
 * values.
 */
void service_helper::_init() {
  Service* obj = static_cast<Service*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_acknowledgement_timeout(0);
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
 * @brief If the provided key/value have their parsing to fail previously,
 * it is possible they are a customvariable. A customvariable name has its
 * name starting with an underscore. This method checks the possibility to
 * store a customvariable in the given object and stores it if possible.
 *
 * @param key   The name of the customvariable.
 * @param value Its value as a string.
 *
 * @return True if the customvariable has been well stored.
 */
bool service_helper::insert_customvariable(std::string_view key,
                                           std::string_view value) {
  if (key[0] != '_')
    return false;

  key.remove_prefix(1);

  Service* obj = static_cast<Service*>(mut_obj());
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

/**
 * @brief Expand the Service object.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void service_helper::expand(
    configuration::State& s,
    configuration::error_cnt& err,
    const absl::flat_hash_map<std::string_view, const configuration::Host*>&
        m_host,
    const absl::flat_hash_map<std::string_view, configuration::Servicegroup*>&
        sgs) {
  // Browse all services.
  for (auto& service_cfg : *s.mutable_services()) {
    // Browse service groups.
    for (auto& sg_name : service_cfg.servicegroups().data()) {
      // Find service group.
      auto found = sgs.find(sg_name);
      if (found == sgs.end()) {
        err.config_errors++;
        throw msg_fmt(
            "Could not add service '{}' of host '{}' to non-existing service "
            "group '{}'",
            service_cfg.service_description(), service_cfg.host_name(),
            sg_name);
      }

      // Add service to service members
      fill_pair_string_group(found->second->mutable_members(),
                             service_cfg.host_name(),
                             service_cfg.service_description());
    }

    if (!service_cfg.host_id() || service_cfg.contacts().data().empty() ||
        service_cfg.contactgroups().data().empty() ||
        service_cfg.notification_interval() == 0 ||
        service_cfg.notification_period().empty() ||
        service_cfg.timezone().empty()) {
      // Find host.
      auto it = m_host.find(service_cfg.host_name());
      if (it == m_host.end()) {
        err.config_errors++;
        throw msg_fmt(
            "Could not inherit special variables for service '{}': host '{}' "
            "does not exist",
            service_cfg.service_description(), service_cfg.host_name());
      }

      // Inherits variables.
      if (!service_cfg.host_id())
        service_cfg.set_host_id(it->second->host_id());
      if (service_cfg.contacts().data().empty() &&
          service_cfg.contactgroups().data().empty()) {
        service_cfg.mutable_contacts()->CopyFrom(it->second->contacts());
        service_cfg.mutable_contactgroups()->CopyFrom(
            it->second->contactgroups());
      }
      if (service_cfg.notification_interval() == 0)
        service_cfg.set_notification_interval(
            it->second->notification_interval());
      if (service_cfg.notification_period().empty())
        service_cfg.set_notification_period(it->second->notification_period());
      if (service_cfg.timezone().empty())
        service_cfg.set_timezone(it->second->timezone());
    }
  }
}
}  // namespace com::centreon::engine::configuration
