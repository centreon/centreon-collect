/*
** Copyright 2012 Merethis
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

#ifndef CCCP_PIPE_HANDLE_HH
#  define CCCP_PIPE_HANDLE_HH

#  include "com/centreon/connector/perl/namespace.hh"
#  include "com/centreon/handle.hh"

CCCP_BEGIN()

/**
 *  @class pipe_handle pipe_handle.hh "com/centreon/connector/perl/pipe_handle.hh"
 *  @brief Wrap a pipe FD.
 *
 *  Wrap a pipe file descriptor within a class.
 */
class           pipe_handle : public handle {
public:
                pipe_handle(int fd = -1);
                pipe_handle(pipe_handle const& ph);
                ~pipe_handle() throw ();
  pipe_handle&  operator=(pipe_handle const& ph);
  void          close() throw ();
  native_handle get_native_handle() throw ();
  unsigned long read(void* data, unsigned long size);
  void          set_fd(int fd);
  unsigned long write(void const* data, unsigned long size);

private:
  void          _internal_copy(pipe_handle const& ph);

  int           _fd;
};

CCCP_END()

#endif // !CCCP_PIPE_HANDLE_HH
