/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_ICMP_SOCKET_MANAGER_HH
# define CCC_ICMP_SOCKET_MANAGER_HH

# include <QSet>
# include <QList>
# include <QMutex>
# include <QWaitCondition>

namespace                        com {
  namespace                      centreon {
    namespace                    connector {
      namespace                  icmp {
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
      }
    }
  }
}

#endif // !CCC_ICMP_SOCKET_MANAGER_HH
