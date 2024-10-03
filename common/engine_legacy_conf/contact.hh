/**
 * Copyright 2011-2013,2015,2017-2024 Centreon (https://www.centreon.com/)
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
#ifndef CCE_CONFIGURATION_CONTACT_HH
#define CCE_CONFIGURATION_CONTACT_HH

#include <absl/container/flat_hash_map.h>

#include "com/centreon/common/opt.hh"
#include "customvariable.hh"
#include "group.hh"
#include "object.hh"

using com::centreon::common::opt;

typedef std::vector<std::string> tab_string;

namespace com::centreon::engine {

namespace configuration {
class contact : public object {
 public:
  typedef std::string key_type;

  contact(key_type const& key = "");
  contact(contact const& other);
  ~contact() noexcept override = default;
  contact& operator=(contact const& other);
  bool operator==(contact const& other) const noexcept;
  bool operator!=(contact const& other) const noexcept;
  bool operator<(contact const& other) const noexcept;
  void check_validity(error_cnt& err) const override;
  key_type const& key() const noexcept;
  void merge(object const& obj) override;
  bool parse(const char* key, const char* value) override;

  tab_string const& address() const noexcept;
  std::string const& alias() const noexcept;
  bool can_submit_commands() const noexcept;
  set_string& contactgroups() noexcept;
  set_string const& contactgroups() const noexcept;
  std::string const& contact_name() const noexcept;
  const std::unordered_map<std::string, customvariable>& customvariables()
      const noexcept;
  std::unordered_map<std::string, customvariable>&
  mutable_customvariables() noexcept;
  std::string const& email() const noexcept;
  bool host_notifications_enabled() const noexcept;
  list_string const& host_notification_commands() const noexcept;
  unsigned int host_notification_options() const noexcept;
  std::string const& host_notification_period() const noexcept;
  bool retain_nonstatus_information() const noexcept;
  bool retain_status_information() const noexcept;
  std::string const& pager() const noexcept;
  list_string const& service_notification_commands() const noexcept;
  unsigned int service_notification_options() const noexcept;
  std::string const& service_notification_period() const noexcept;
  bool service_notifications_enabled() const noexcept;
  std::string const& timezone() const noexcept;

 private:
  typedef bool (*setter_func)(contact&, const char*);

  bool _set_address(std::string const& key, std::string const& value);
  bool _set_alias(std::string const& value);
  bool _set_can_submit_commands(bool value);
  bool _set_contactgroups(std::string const& value);
  bool _set_contact_name(std::string const& value);
  bool _set_email(std::string const& value);
  bool _set_host_notifications_enabled(bool value);
  bool _set_host_notification_commands(std::string const& value);
  bool _set_host_notification_options(std::string const& value);
  bool _set_host_notification_period(std::string const& value);
  bool _set_retain_nonstatus_information(bool value);
  bool _set_retain_status_information(bool value);
  bool _set_pager(std::string const& value);
  bool _set_service_notification_commands(std::string const& value);
  bool _set_service_notification_options(std::string const& value);
  bool _set_service_notification_period(std::string const& value);
  bool _set_service_notifications_enabled(bool value);
  bool _set_timezone(std::string const& value);

  tab_string _address;
  std::string _alias;
  opt<bool> _can_submit_commands;
  group<set_string> _contactgroups;
  std::string _contact_name;
  std::unordered_map<std::string, customvariable> _customvariables;
  std::string _email;
  opt<bool> _host_notifications_enabled;
  group<list_string> _host_notification_commands;
  opt<unsigned int> _host_notification_options;
  std::string _host_notification_period;
  opt<bool> _retain_nonstatus_information;
  opt<bool> _retain_status_information;
  std::string _pager;
  group<list_string> _service_notification_commands;
  opt<unsigned int> _service_notification_options;
  std::string _service_notification_period;
  opt<bool> _service_notifications_enabled;
  opt<std::string> _timezone;
  static const absl::flat_hash_map<std::string, setter_func> _setters;
};

typedef std::shared_ptr<contact> contact_ptr;
typedef std::set<contact> set_contact;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_CONTACT_HH
