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

#ifndef CCCS_SESSIONS_SOCKET_HANDLE_HH
#  define CCCS_SESSIONS_SOCKET_HANDLE_HH

#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/handle.hh"

CCCS_BEGIN()

namespace          sessions {
  /**
   *  @class socket_handle socket_handle.hh "com/centreon/connector/ssh/socket_handle.hh"
   *  @brief Socket handle.
   *
   *  Wrapper around a socket descriptor.
   */
  class            socket_handle : public com::centreon::handle {
  public:
                   socket_handle(
                     native_handle handl = native_handle_null);
                   ~socket_handle() throw ();
    void           close();
    native_handle  get_native_handle();
    unsigned long  read(void* data, unsigned long size);
    void           set_native_handle(native_handle handl);
    unsigned long  write(void const* data, unsigned long size);

  private:
                   socket_handle(socket_handle const& sh);
    socket_handle& operator=(socket_handle const& sh);

    native_handle  _handl;
  };
}

CCCS_END()

#endif // !CCCS_SESSIONS_SOCKET_HANDLE_HH
