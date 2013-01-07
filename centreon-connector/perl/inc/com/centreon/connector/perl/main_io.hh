/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Perl Connector.
**
** Centreon Perl Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Perl Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Perl Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_PERL_MAIN_IO_HH_
# define CCC_PERL_MAIN_IO_HH_

# include <string>
# include "com/centreon/connector/perl/namespace.hh"

CCC_PERL_BEGIN()

/**
 *  @class main_io main_io.hh
 *  @brief Manage I/O on standard input and standard output.
 *
 *  This singleton manages I/O on standard input (wait for
 *  orders from the monitoring engine) and on standard output
 *  (provide check results).
 */
class             main_io {
 private:
  std::string     _rbuffer;
  std::string     _wbuffer;
                  main_io();
                  main_io(main_io const& mio);
  main_io&        operator=(main_io const& mio);
  int             _parse(std::string const& cmd);

 public:
                  ~main_io();
  static main_io& instance();
  int             read();
  int             write();
  void            write(std::string const& data);
  bool            write_wanted() const;
};

CCC_PERL_END()

#endif /* !CCC_PERL_MAIN_IO_HH_ */
