/*
** Copyright 2011 Merethis
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

#ifndef CCE_OBJECTS_TIMEPERIOD_HH
# define CCE_OBJECTS_TIMEPERIOD_HH

# include "objects.hh"

# ifdef __cplusplus
extern "C" {
# endif

  // void add_timeperiod(char const* name,
  //                     char const* alias,
  //                     char const** range,
  //                     char const** exclude);

  void release_timeperiod(timeperiod const* obj);

# ifdef __cplusplus
}

namespace       com {
  namespace     centreon {
    namespace   engine {
      namespace objects {
        void    add_timeperiod(std::string const& name,
                               std::string const& alias,
                               std::vector<std::string> const& range,
                               std::vector<std::string> const& exclude);
        void    release(timeperiod const* obj);
      }
    }
  }
}
# endif

#endif // !CCE_OBJECTS_TIMEPERIOD_HH
