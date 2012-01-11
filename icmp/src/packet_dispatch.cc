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

#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/connector/icmp/packet_dispatch.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 */
packet_dispatch::packet_dispatch(packet_observer* observer)
  : thread(),
    handle_listener(),
    _quit(false),
    _observer(observer),
    _t_manager(1),
    _h_manager(&_t_manager),
    _want_write(false) {

  // drop privileges.
  setuid(getuid());

  _h_manager.add(&_socket, this);
  _h_manager.add(&_interrupt, &_interrupt);
  exec();
}

/**
 *  Default destructor.
 */
packet_dispatch::~packet_dispatch() throw () {
  {
    locker lock(&_mtx);
    _quit = true;
    _interrupt.wake();
  }
  wait();
}

/**
 *  Push packet to send with icmp socket.
 *
 *  @param[in] pkt  The packet to send.
 */
void packet_dispatch::push(packet const& pkt) {
  locker lock(&_mtx);
  _packets.push_back(pkt);
  _interrupt.wake();
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
packet_dispatch::packet_dispatch(packet_dispatch const& right)
  : thread(),
    handle_listener() {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
packet_dispatch& packet_dispatch::operator=(packet_dispatch const& right) {
  return (_internal_copy(right));
}

/**
 *  Close event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void packet_dispatch::close(handle& h) {
  (void)h;
  _quit = true;
  logging::debug(logging::low) << "icmp socket was close";
}

/**
 *  Error event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void packet_dispatch::error(handle& h) {
  (void)h;
  _quit = true;
  logging::debug(logging::low) << "icmp socket has an error";
}

/**
 *  Read event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void packet_dispatch::read(handle& h) {
  try {
    timestamp now(timestamp::now());
    icmp_socket& sock(reinterpret_cast<icmp_socket&>(h));
    unsigned char buffer[4096];
    unsigned long size(sock.read(buffer, sizeof(buffer)));
    packet pkt(buffer, size, now);
    logging::debug(logging::high) << "read " << pkt;
    if (_observer)
      _observer->emit_receive(pkt);
  }
  catch (std::exception const& e) {
    logging::error(logging::low) << e.what();
  }
}

/**
 *  This methode was notify for read event.
 *
 *  @param[in] h  The handle we want to get read event.
 *
 *  @return True if handle is standard input, otherwise false.
 */
bool packet_dispatch::want_read(handle& h) {
  (void)h;
  return (true);
}

/**
 *  This methode was notify for write event.
 *
 *  @param[in] h  The handle we want to get write event.
 *
 *  @return True if handle is standard output, otherwise false.
 */
bool packet_dispatch::want_write(handle& h) {
  (void)h;
  return (&h == &_socket && _want_write);
}

/**
 *  Write event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void packet_dispatch::write(handle& h) {
  try {
    icmp_socket& sock(reinterpret_cast<icmp_socket&>(h));
    _mtx.lock();
    packet pkt(_packets.front());
    _packets.pop_front();
    _mtx.unlock();

    void const* data(pkt.get_data());
    logging::debug(logging::high) << "write " << pkt;

    sock.set_address(pkt.get_address());
    if (sock.write(data, pkt.get_size()) != pkt.get_size())
      throw (basic_error() << "icmp socket write failed");
  }
  catch (std::exception const& e) {
    logging::error(logging::low) << e.what();
  }
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
packet_dispatch& packet_dispatch::_internal_copy(packet_dispatch const& right) {
  (void)right;
  assert(!"impossible to copy packet_dispatch");
  abort();
  return (*this);
}

/**
 *  Main loop for packet dispatcher.
 */
void packet_dispatch::_run() {
  try {
    while (true) {
      _h_manager.multiplex();

      locker lock(&_mtx);
      _want_write = !_packets.empty();
      if (_quit && _packets.empty())
        break;
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::low) << e.what();
  }
}
