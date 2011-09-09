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

#ifndef CCE_OBJECTS_HOSTESCALATION_HH
# define CCE_OBJECTS_HOSTESCALATION_HH

# ifdef __cplusplus
#  include <QVector>
#  include <QString>
# endif
# include "objects.hh"

# ifdef __cplusplus
extern "C" {
# endif

  bool link_hostescalation(hostescalation* obj,
                           contact** contacts,
                           contactgroup** contactgroups,
                           timeperiod* escalation_period);
  void release_hostescalation(hostescalation const* obj);

# ifdef __cplusplus
}

namespace       com {
  namespace     centreon {
    namespace   engine {
      namespace objects {
        void    link(hostescalation* obj,
                     QVector<contact*> const& contacts = QVector<contact*>(),
                     QVector<contactgroup*> const& contactgroups = QVector<contactgroup*>(),
                     timeperiod* escalation_period = NULL);
        void    release(hostescalation const* obj);
      }
    }
  }
}
# endif

#endif // !CCE_OBJECTS_HOSTESCALATION_HH
