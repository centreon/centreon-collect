/*
** Copyright 2011-2013,2015,2017 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_CONFIGURATION_CONTACT_HH
#define CCE_CONFIGURATION_CONTACT_HH

#include <absl/container/flat_hash_map.h>

#include "com/centreon/engine/configuration/group.hh"
#include "com/centreon/engine/configuration/object.hh"
#include "com/centreon/engine/customvariable.hh"
#include "com/centreon/engine/opt.hh"

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
  void check_validity() const override;
  key_type const& key() const noexcept;
  void merge(object const& obj) override;
  bool parse(const char* key, const char* value) override;

  tab_string const& address() const noexcept;
  std::string const& alias() const noexcept;
  bool can_submit_commands() const noexcept;
  set_string& contactgroups() noexcept;
  set_string const& contactgroups() const noexcept;
  std::string const& contact_name() const noexcept;
  map_customvar const& customvariables() const noexcept;
  map_customvar& customvariables() noexcept;
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
  map_customvar _customvariables;
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

}

#endif  // !CCE_CONFIGURATION_CONTACT_HH
