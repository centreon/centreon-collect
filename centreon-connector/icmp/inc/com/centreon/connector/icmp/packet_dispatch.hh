/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_ICMP_PACKET_DISPATCH_HH
#  define CCC_ICMP_PACKET_DISPATCH_HH

#  include <list>
#  include "com/centreon/concurrency/mutex.hh"
#  include "com/centreon/concurrency/thread.hh"
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/icmp_socket.hh"
#  include "com/centreon/connector/icmp/interrupt.hh"
#  include "com/centreon/connector/icmp/packet_observer.hh"
#  include "com/centreon/handle_listener.hh"
#  include "com/centreon/handle_manager.hh"
#  include "com/centreon/task_manager.hh"

CCC_ICMP_BEGIN()

/**
 *  @class packet_dispatch packet_dispatch.hh "com/centreon/connector/icmp/packet_dispatch.hh"
 *  @brief This class send and receive data with icmp_socket.
 *
 *  This class run into a thread and send and receive data into
 *  icmp socket.
 */
class                packet_dispatch
  : private concurrency::thread, handle_listener {
public:
                     packet_dispatch(packet_observer* observer = NULL);
                     ~packet_dispatch() throw ();
  void               push(packet const& pkt);

protected:
  void               _run();

private:
                     packet_dispatch(packet_dispatch const& right);
  packet_dispatch&   operator=(packet_dispatch const& right);
  void               close(handle& h);
  void               error(handle& h);
  void               read(handle& h);
  bool               want_read(handle& h);
  bool               want_write(handle& h);
  void               write(handle& h);
  packet_dispatch&   _internal_copy(packet_dispatch const& right);

  interrupt          _interrupt;
  bool               _quit;
  concurrency::mutex _mtx;
  packet_observer*   _observer;
  std::list<packet>  _packets;
  icmp_socket        _socket;
  task_manager       _t_manager;
  handle_manager     _h_manager;
  task               _timeout;
  unsigned int       _want_write;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_PACKET_DISPATCH_HH
