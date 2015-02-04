/*
** Copyright 2014 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCB_BAM_TIME_PTR_TYPEDEF_HH
#  define CCB_BAM_TIME_PTR_TYPEDEF_HH

#  include "com/centreon/broker/misc/shared_ptr.hh"

#  define DECLARE_SHARED_PTR(klass)\
  typedef ::com::centreon::broker::misc::shared_ptr<klass> ptr

#endif // !CCB_BAM_TIME_PTR_TYPEDEF_HH
