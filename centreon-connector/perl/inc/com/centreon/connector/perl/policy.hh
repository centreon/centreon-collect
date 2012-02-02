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

#ifndef CCCP_POLICY_HH
#  define CCCP_POLICY_HH

#  include <map>
#  include <sys/types.h>
#  include "com/centreon/connector/perl/checks/listener.hh"
#  include "com/centreon/connector/perl/namespace.hh"
#  include "com/centreon/connector/perl/orders/listener.hh"
#  include "com/centreon/connector/perl/orders/parser.hh"
#  include "com/centreon/connector/perl/reporter.hh"
#  include "com/centreon/io/file_stream.hh"

CCCP_BEGIN()

// Forward declarations.
namespace         checks {
  class           check;
  class           result;
}

/**
 *  @class policy policy.hh "com/centreon/connector/perl/policy.hh"
 *  @brief Software policy.
 *
 *  Wraps software policy within a class.
 */
class             policy : public orders::listener,
                           public checks::listener {
public:
                  policy();
                  ~policy() throw ();
  void            on_eof();
  void            on_error();
  void            on_execute(
                    unsigned long long cmd_id,
                    time_t timeout,
                    std::string const& cmd);
  void            on_quit();
  void            on_result(checks::result const& r);
  void            on_version();
  bool            run();

private:
                  policy(policy const& p);
  policy&         operator=(policy const& p);
  void            _internal_copy(policy const& p);

  std::map<pid_t, checks::check*>
                  _checks;
  bool            _error;
  orders::parser  _parser;
  reporter        _reporter;
  io::file_stream _sin;
  io::file_stream _sout;
};

CCCP_END()

#endif // !CCCP_POLICY_HH
