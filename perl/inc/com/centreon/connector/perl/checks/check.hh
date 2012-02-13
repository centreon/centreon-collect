/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCP_CHECKS_CHECK_HH
#  define CCCP_CHECKS_CHECK_HH

#  include <string>
#  include <sys/types.h>
#  include "com/centreon/connector/perl/namespace.hh"
#  include "com/centreon/connector/perl/pipe_handle.hh"
#  include "com/centreon/handle_listener.hh"

CCCP_BEGIN()

namespace              checks {
  // Forward declarations.
  class                listener;
  class                result;

  /**
   *  @class check check.hh "com/centreon/connector/perl/checks/check.hh"
   *  @brief Perl check.
   *
   *  Class wrapping a Perl check as requested by the monitoring engine.
   */
  class                check : public handle_listener {
  public:
                       check();
                       ~check() throw ();
    void               error(handle& h);
    pid_t              execute(
                         unsigned long long cmd_id,
                         std::string const& cmd,
                         time_t tmt);
    void               listen(listener* listnr);
    void               on_timeout(bool final = true);
    void               read(handle& h);
    void               terminated(int exit_code);
    void               unlisten(listener* listnr);
    bool               want_read(handle& h);
    void               write(handle& h);

  private:
                       check(check const& c);
    check&             operator=(check const& c);
    void               _internal_copy(check const& c);
    void               _send_result_and_unregister(result const& r);

    pid_t              _child;
    unsigned long long _cmd_id;
    pipe_handle        _err;
    listener*          _listnr;
    pipe_handle        _out;
    std::string        _stderr;
    std::string        _stdout;
    unsigned long      _timeout;
  };
}

CCCP_END()

#endif // !CCCP_CHECKS_CHECK_HH
