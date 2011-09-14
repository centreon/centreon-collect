/*
** Copyright 2011 Merethis
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

#ifndef CCC_PERL_PROCESS_HH_
# define CCC_PERL_PROCESS_HH_

# include <string>
# include <sys/types.h>
# include "com/centreon/connector/perl/namespace.hh"

CCC_PERL_BEGIN()

/**
 *  @class process process.hh
 *  @brief Manage a process.
 *
 *  Manage process I/O.
 */
class                process {
 private:
  unsigned long long _cmd;
  int                _fd_err;
  int                _fd_out;
  int                _signal;
  std::string        _stderr;
  std::string        _stdout;
  time_t             _timeout;
  void               _internal_copy(process const& p);

 public:
                     process(unsigned long long cmd_id,
                       int fd_out,
                       int fd_err);
                     process(process const& p);
                     ~process();
  process&           operator=(process const& p);
  void               close();
  unsigned long long cmd() const;
  std::string const& err() const;
  std::string const& out() const;
  void               read_err();
  int                read_err_fd() const;
  void               read_out();
  int                read_out_fd() const;
  int                signal() const;
  void               signal(int signum);
  time_t             timeout() const;
  void               timeout(time_t t);
};

CCC_PERL_END()

#endif /* !CCC_PERL_PROCESS_HH_ */
