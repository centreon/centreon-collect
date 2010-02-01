/*
**  Copyright 2009 MERETHIS
**  This file is part of CentreonBroker.
**
**  CentreonBroker is free software: you can redistribute it and/or modify it
**  under the terms of the GNU General Public License as published by the Free
**  Software Foundation, either version 2 of the License, or (at your option)
**  any later version.
**
**  CentreonBroker is distributed in the hope that it will be useful, but
**  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
**  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
**  for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with CentreonBroker.  If not, see <http://www.gnu.org/licenses/>.
**
**  For more information : contact@centreon.com
*/

#ifndef MODULE_INTERNAL_H_
# define MODULE_INTERNAL_H_

# include <map>
# include <string>
# include <utility>
# include "multiplexing/publisher.h"

// List of host IDs.
extern std::map<std::string, int> gl_hosts;

// List of service IDs.
extern std::map<std::pair<std::string, std::string>, int> gl_services;

// Sender object.
extern Multiplexing::Publisher gl_publisher;

#endif /* !MODULE_INTERNAL_H_ */
