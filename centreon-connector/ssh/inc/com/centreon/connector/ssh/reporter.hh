/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCS_REPORTER_HH
#  define CCCS_REPORTER_HH

#  include <string>
#  include "com/centreon/connector/ssh/checks/listener.hh"
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

/**
 *  @class reporter reporter.hh "com/centreon/connector/ssh/reporter.hh"
 *  @brief Report data back to the monitoring engine.
 *
 *  Send replies to the monitoring engine.
 */
class                reporter : public com::centreon::handle_listener {
public:
                     reporter();
                     reporter(reporter const& r);
                     ~reporter() throw ();
  reporter&          operator=(reporter const& r);
  bool               can_report() const throw ();
  void               error(handle& h);
  std::string const& get_buffer() const throw ();
  void               send_result(checks::result const& r);
  void               send_version(unsigned int major, unsigned int minor);
  bool               want_write(handle& h);
  void               write(handle& h);

private:
  void               _copy(reporter const& r);

  std::string        _buffer;
  bool               _can_report;
  unsigned int       _reported;
};

CCCS_END()

#endif // !CCCS_REPORTER_HH
