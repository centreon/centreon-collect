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
#include "common/engine_conf/host_helper.hh"
#include <absl/strings/numbers.h>

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Host object.
 *
 * @param obj The Host object on which this helper works. The helper is not the
 * owner of this object.
 */
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
                     Host::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Host objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool host_helper::hook(std::string_view key, std::string_view value) {
  Host* obj = static_cast<Host*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "host_name") {
    obj->set_host_name(std::string(value));
    set_changed(obj->descriptor()->FindFieldByName("host_name")->index());
    if (obj->alias().empty()) {
      obj->set_alias(obj->host_name());
      set_changed(obj->descriptor()->FindFieldByName("alias")->index());
    }
    return true;
  } else if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "contacts") {
    fill_string_group(obj->mutable_contacts(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "notification_options") {
    uint16_t options = action_hst_none;
    if (fill_host_notification_options(&options, value)) {
      obj->set_notification_options(options);
      set_changed(
          obj->descriptor()->FindFieldByName("notification_options")->index());
      return true;
    } else
      return false;
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
  } else if (key == "coords_3d") {
    std::vector<std::string_view> coords_list{absl::StrSplit(value, ',')};

    if (coords_list.size() != 3)
      return false;

    double value;
    if (absl::SimpleAtod(coords_list[0], &value))
      obj->mutable_coords_3d()->set_x(value);
    else
      return false;

    if (absl::SimpleAtod(coords_list[1], &value))
      obj->mutable_coords_3d()->set_y(value);
    else
      return false;

    if (absl::SimpleAtod(coords_list[2], &value))
      obj->mutable_coords_3d()->set_z(value);
    else
      return false;

    set_changed(obj->descriptor()->FindFieldByName("coords_3d")->index());

    return true;
  } else if (key == "coords_2d") {
    std::vector<std::string_view> coords_list{absl::StrSplit(value, ',')};

    if (coords_list.size() != 2)
      return false;

    double value;
    if (absl::SimpleAtod(coords_list[0], &value))
      obj->mutable_coords_2d()->set_x(value);
    else
      return false;

    if (absl::SimpleAtod(coords_list[1], &value))
      obj->mutable_coords_2d()->set_y(value);
    else
      return false;

    set_changed(obj->descriptor()->FindFieldByName("coords_2d")->index());

    return true;
  } else if (key == "stalking_options") {
    uint8_t options(action_hst_none);
    auto values = absl::StrSplit(value, ',');
    for (auto it = values.begin(); it != values.end(); ++it) {
      std::string_view v = absl::StripAsciiWhitespace(*it);
      if (v == "o" || v == "up")
        options |= action_hst_up;
      else if (v == "d" || v == "down")
        options |= action_hst_down;
      else if (v == "u" || v == "unreachable")
        options |= action_hst_unreachable;
      else if (v == "n" || v == "none")
        options = action_hst_none;
      else if (v == "a" || v == "all")
        options = action_hst_up | action_hst_down | action_hst_unreachable;
      else
        return false;
    }
    obj->set_stalking_options(options);
    set_changed(
        obj->descriptor()->FindFieldByName("stalking_options")->index());

    return true;
  } else if (key == "flap_detection_options") {
    uint8_t options(action_hst_none);
    auto values = absl::StrSplit(value, ',');
    for (auto& val : values) {
      auto v = absl::StripAsciiWhitespace(val);
      if (v == "o" || v == "up")
        options |= action_hst_up;
      else if (v == "d" || v == "down")
        options |= action_hst_down;
      else if (v == "u" || v == "unreachable")
        options |= action_hst_unreachable;
      else if (v == "n" || v == "none")
        options = action_hst_none;
      else if (v == "a" || v == "all")
        options = action_hst_up | action_hst_down | action_hst_unreachable;
      else
        return false;
    }
    obj->set_flap_detection_options(options);
    set_changed(
        obj->descriptor()->FindFieldByName("flap_detection_options")->index());

    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Host object.
 *
 * @param err An error counter.
 */
void host_helper::check_validity(error_cnt& err) const {
  const Host* o = static_cast<const Host*>(obj());

  if (o->obj().register_()) {
    if (o->host_name().empty()) {
      err.config_errors++;
      throw msg_fmt("Host has no name (property 'host_name')");
    }
    if (o->address().empty()) {
      err.config_errors++;
      throw msg_fmt("Host '{}' has no address (property 'address')",
                    o->host_name());
    }
  }
}

/**
 * @brief Initializer of the Host object, in other words set its default values.
 */
void host_helper::_init() {
  Host* obj = static_cast<Host*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
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
  obj->set_low_flap_threshold(0);
  obj->set_max_check_attempts(3);
  obj->set_notifications_enabled(true);
  obj->set_notification_interval(0);
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
bool host_helper::insert_customvariable(std::string_view key,
                                        std::string_view value) {
  if (key[0] != '_')
    return false;

  key.remove_prefix(1);

  Host* obj = static_cast<Host*>(mut_obj());
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
 * @brief Expand the hosts.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void host_helper::expand(
    configuration::State& s,
    configuration::error_cnt& err,
    const absl::flat_hash_map<std::string_view, configuration::Hostgroup*>&
        hgs) {
  // Browse all hosts.
  for (auto& host_cfg : *s.mutable_hosts()) {
    for (auto& grp : host_cfg.hostgroups().data()) {
      auto it = hgs.find(grp);
      if (it != hgs.end()) {
        fill_string_group(it->second->mutable_members(), host_cfg.host_name());
      } else {
        err.config_errors++;
        throw msg_fmt(
            "Could not add host '{}' to non-existing host group '{}'\n",
            host_cfg.host_name(), grp);
      }
    }
  }
}

}  // namespace com::centreon::engine::configuration
