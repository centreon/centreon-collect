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

#ifndef CCE_RETENTION_INFO_HH
#  define CCE_RETENTION_INFO_HH

#  include <string>
#  include "com/centreon/engine/namespace.hh"
#  include "com/centreon/engine/retention/object.hh"

CCE_BEGIN()

namespace retention {
  class   info
    : public object {
  public:
          info();
          ~info() throw ();
    bool  set(
            std::string const& key,
            std::string const& value);

  private:
    bool  _scheduling_info_is_ok;
  };
}

CCE_END()

#endif // !CCE_RETENTION_INFO_HH
