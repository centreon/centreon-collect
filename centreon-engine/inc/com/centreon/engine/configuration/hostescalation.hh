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

#ifndef CCE_CONFIGURATION_HOSTESCALATION_HH
#  define CCE_CONFIGURATION_HOSTESCALATION_HH

#  include "com/centreon/engine/configuration/group.hh"
#  include "com/centreon/engine/configuration/object.hh"
#  include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

namespace                  configuration {
  class                    hostescalation
    : public object {
  public:
                           hostescalation();
                           hostescalation(hostescalation const& right);
                           ~hostescalation() throw ();
    hostescalation&        operator=(hostescalation const& right);
    bool                   operator==(
                             hostescalation const& right) const throw ();
    bool                   operator!=(
                             hostescalation const& right) const throw ();
    /*
    std::list<std::string> const&
                           contactgroups() const throw ();
    std::list<std::string> const&
                           contacts() const throw ();
    std::string const&     escalation_options() const throw ();
    std::string const&     escalation_period() const throw ();
    unsigned int           first_notification() const throw ();
    std::list<std::string> const&
                           hostgroups() const throw ();
    std::list<std::string> const&
                           hosts() const throw ();
    unsigned int           last_notification() const throw ();
    unsigned int           notification_interval() const throw ();
    */

    void                   merge(object const& obj);
    bool                   parse(
                             std::string const& key,
                             std::string const& value);

  private:
    void                   _set_contactgroups(std::string const& value);
    void                   _set_contacts(std::string const& value);
    void                   _set_escalation_options(std::string const& value);
    void                   _set_escalation_period(std::string const& value);
    void                   _set_first_notification(unsigned int value);
    void                   _set_hostgroups(std::string const& value);
    void                   _set_hosts(std::string const& value);
    void                   _set_last_notification(unsigned int value);
    void                   _set_notification_interval(unsigned int value);

    group                  _contactgroups;
    group                  _contacts;
    std::string            _escalation_options;
    std::string            _escalation_period;
    unsigned int           _first_notification;
    group                  _hostgroups;
    group                  _hosts;
    unsigned int           _last_notification;
    unsigned int           _notification_interval;
  };
}

CCE_END()

#endif // !CCE_CONFIGURATION_HOSTESCALATION_HH
