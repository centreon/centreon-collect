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

#ifndef CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
#  define CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH

#  include "com/centreon/engine/configuration/hostgroup.hh"
#  include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

namespace               configuration {
  class                 state;

  namespace             applier {
    class               hostgroup {
    public:
                        hostgroup();
                        hostgroup(hostgroup const& right);
                        ~hostgroup() throw ();
      hostgroup&        operator=(hostgroup const& right);
      void              add_object(
                          hostgroup_ptr obj,
                          configuration::state& s);
      void              modify_object(
                          hostgroup_ptr obj,
                          configuration::state& s);
      void              remove_object(
                          hostgroup_ptr obj,
                          configuration::state& s);
      void              resolve_object(
                          hostgroup_ptr obj,
                          configuration::state& s);
    };
  }
}

CCE_END()

#endif // !CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
