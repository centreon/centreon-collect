/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU General Public License
** version 2 as published by the Free Software Foundation.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_ICMP_SOCKET_MANAGER_HH_
# define CCC_ICMP_SOCKET_MANAGER_HH_

# include <QSet>
# include <QList>
# include <QMutex>
# include <QWaitCondition>
# include "com/centreon/connector/icmp/namespace.hh"

NAMESPACE_BEGIN()

/**
 *  @class socket_manager socket_manager.hh
 *  @brief Socket manager is a resource manager for
 *  raw socket.
 *
 *  Socket manager initialize raw socket and dispatch resources.
 */
class                    socket_manager {
 public:
  static socket_manager& instance();

  void                   initialize(unsigned int nbr_socket = 10);

  int                    take();
  void                   release(int sock);

 private:
                         socket_manager();
                         socket_manager(socket_manager const& right);
                         ~socket_manager() throw();

  socket_manager&        operator=(socket_manager const& right);

  QList<int>             _free;
  QSet<int>              _busy;
  QWaitCondition         _wait_socket;
  QMutex                 _mutex;
};

NAMESPACE_END()

#endif // !CCC_ICMP_SOCKET_MANAGER_HH_
