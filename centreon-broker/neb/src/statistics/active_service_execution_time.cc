/*
** Copyright 2013 Merethis
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

#include <iomanip>
#include <sstream>
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/statistics/active_service_execution_time.hh"
#include "com/centreon/broker/neb/statistics/compute_value.hh"
#include "com/centreon/engine/globals.hh"

using namespace com::centreon::broker::neb;
using namespace com::centreon::broker::neb::statistics;

/**
 *  Default constructor.
 */
active_service_execution_time::active_service_execution_time() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
active_service_execution_time::active_service_execution_time(active_service_execution_time const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
active_service_execution_time::~active_service_execution_time() {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
active_service_execution_time& active_service_execution_time::operator=(active_service_execution_time const& right) {
  (void)right;
  return (*this);
}

/**
 *  Get statistics.
 *
 *  @return Statistics output.
 */
std::string active_service_execution_time::run() {
  std::ostringstream oss;
  compute_value<double> cv;
  for (service* s(service_list); s; s = s->next)
    if (s->check_type == SERVICE_CHECK_ACTIVE)
      cv << s->execution_time;
  if (cv.size()) {
    oss << "Engine " << instance_name.toStdString()
        << " has an average active service execution time of "
        << std::fixed << std::setprecision(2) << cv.avg()
        << "s|avg=" << cv.avg() << "s min=" << cv.min()
        << "s max=" << cv.max() << "s\n";
  }
  else {
    oss << "No active service to compute active service "
        << "execution time on " << instance_name.toStdString() << "\n";
  }
  return (oss.str());
}
