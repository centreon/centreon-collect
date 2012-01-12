/*
** Copyright 2011-2012 Merethis
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

#ifndef CC_ICMP_INTERRUPT_HH
#  define CC_ICMP_INTERRUPT_HH

#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/handle.hh"
#  include "com/centreon/handle_listener.hh"

CCC_ICMP_BEGIN()

/**
 *  @class interrupt interrupt.hh "com/centreon/interrupt.hh"
 *  @brief Implementation of handle manager interruption system.
 *
 *  This class allow to interrupt the handle manager before the timeout.
 */
class           interrupt : public handle_listener, public handle {
public:
                interrupt();
                interrupt(interrupt const& right);
                ~interrupt() throw ();
  interrupt&    operator=(interrupt const& right);
  void          wake();

private:
  void          close();
  void          close(handle& h);
  void          error(handle& h);
  native_handle get_native_handle();
  unsigned long read(void* data, unsigned long size);
  void          read(handle& h);
  bool          want_read(handle& h);
  unsigned long write(void const* data, unsigned long size);
  void          write(handle& h);
  void          _internal_copy(interrupt const& right);

  int           _fd[2];
};

CCC_ICMP_END()

#endif // !CC_ICMP_INTERRUPT_HH
