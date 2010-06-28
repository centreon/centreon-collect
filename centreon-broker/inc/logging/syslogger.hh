/*
**  Copyright 2010 MERETHIS
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

#ifndef LOGGING_SYSLOGGER_HH_
# define LOGGING_SYSLOGGER_HH_

# include "logging/backend.hh"

namespace      logging
{
  /**
   *  @class syslogger syslogger.hh "logging/syslogger.hh"
   *  @brief Log messages to syslog.
   *
   *  Log messages to a configured facility of syslog.
   */
  class        syslogger : public backend
  {
   public:
               syslogger();
               syslogger(syslogger const& s);
               syslogger(int facility);
               ~syslogger();
    syslogger& operator=(syslogger const& s);
    void       log_msg(char const* msg,
                 unsigned int len,
                 type log_type,
                 level l) throw ();
  };
}

#endif /* !LOGGING_SYSLOGGER_HH_ */
