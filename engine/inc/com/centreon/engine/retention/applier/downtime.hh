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

#ifndef CCE_RETENTION_APPLIER_DOWNTIME_HH
#define CCE_RETENTION_APPLIER_DOWNTIME_HH

#include "com/centreon/engine/retention/downtime.hh"

namespace com::centreon::engine {

namespace retention {
namespace applier {
class downtime {
 public:
  static void apply(list_downtime const& lst);

 private:
  static void _add_host_downtime(retention::downtime const& obj) throw();
  static void _add_service_downtime(retention::downtime const& obj) throw();
};
}  // namespace applier
}  // namespace retention

}

#endif  // !CCE_RETENTION_APPLIER_DOWNTIME_HH
