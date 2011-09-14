/*
** Copyright 2011 Merethis
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

#ifndef CCC_SSH_STD_IO_HH_
# define CCC_SSH_STD_IO_HH_

# include <string>
# include "com/centreon/connector/ssh/namespace.hh"

CCC_SSH_BEGIN()

/**
 *  @class std_io std_io.hh "com/centreon/connector/ssh/std_io.hh"
 *  @brief Manage standard I/O objects.
 *
 *  Handle standard I/O objects (stdin, stdout).
 */
class            std_io {
 private:
  std::string    _rbuffer;
  std::string    _wbuffer;
                 std_io();
                 std_io(std_io const& sio);
  std_io&        operator=(std_io const& sio);
  void           _parse(std::string const& cmd);

 public:
                 ~std_io();
  static std_io& instance();
  bool           read();
  void           submit_check_result(unsigned long long cmd_id,
                   bool executed,
                   int exitcode,
                   std::string const& err,
                   std::string const& out);
  void           write();
  bool           write_wanted() const;
};

CCC_SSH_END()

#endif /* !CCC_SSH_STD_IO_HH_ */
