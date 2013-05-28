/*
** Copyright 2011-2013 Merethis
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
#  define CCE_CONFIGURATION_CONTACT_HH

#  include <vector>
#  include "com/centreon/engine/configuration/group.hh"
#  include "com/centreon/engine/configuration/object.hh"
#  include "com/centreon/engine/configuration/opt.hh"
#  include "com/centreon/engine/namespace.hh"
#  include "com/centreon/unordered_hash.hh"

CCE_BEGIN()

namespace                  configuration {
  class                    contact
    : public object {
  public:
                           contact();
                           contact(contact const& right);
                           ~contact() throw ();
    contact&               operator=(contact const& right);
    bool                   operator==(
                             contact const& right) const throw ();
    bool                   operator!=(
                             contact const& right) const throw ();
    std::size_t            id() const throw ();
    bool                   is_valid() const throw ();
    void                   merge(object const& obj);
    bool                   parse(
                             std::string const& key,
                             std::string const& value);

  private:
    bool                   _set_address(
                             std::string const& key,
                             std::string const& value);
    bool                   _set_alias(std::string const& value);
    bool                   _set_can_submit_commands(bool value);
    bool                   _set_contactgroups(std::string const& value);
    bool                   _set_contact_name(std::string const& value);
    bool                   _set_email(std::string const& value);
    bool                   _set_host_notifications_enabled(bool value);
    bool                   _set_host_notification_commands(std::string const& value);
    bool                   _set_host_notification_options(std::string const& value);
    bool                   _set_host_notification_period(std::string const& value);
    bool                   _set_retain_nonstatus_information(bool value);
    bool                   _set_retain_status_information(bool value);
    bool                   _set_pager(std::string const& value);
    bool                   _set_service_notification_commands(std::string const& value);
    bool                   _set_service_notification_options(std::string const& value);
    bool                   _set_service_notification_period(std::string const& value);
    bool                   _set_service_notifications_enabled(bool value);

    std::vector<std::string>
                           _address;
    std::string            _alias;
    opt<bool>              _can_submit_commands;
    group                  _contactgroups;
    std::string            _contact_name;
    umap<std::string, std::string>
                           _customvariables;
    std::string            _email;
    opt<bool>              _host_notifications_enabled;
    group                  _host_notification_commands;
    opt<unsigned int>      _host_notification_options;
    std::string            _host_notification_period;
    opt<bool>              _retain_nonstatus_information;
    opt<bool>              _retain_status_information;
    std::string            _pager;
    group                  _service_notification_commands;
    opt<unsigned int>      _service_notification_options;
    std::string            _service_notification_period;
    opt<bool>              _service_notifications_enabled;
  };

  typedef umap<std::size_t, shared_ptr<contact> > map_contact;
}

CCE_END()

#endif // !CCE_CONFIGURATION_CONTACT_HH

