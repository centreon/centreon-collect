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

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/icmp_socket.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 */
icmp_socket::icmp_socket()
  : handle(), _address(0) {
  _internal_handle = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (_internal_handle == -1) {
    char const* msg(strerror(errno));
    throw (basic_error() << "create icmp socket failed: " << msg);
  }
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
icmp_socket::icmp_socket(icmp_socket const& right)
  : handle(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
icmp_socket::~icmp_socket() throw () {
  close();
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
icmp_socket& icmp_socket::operator=(icmp_socket const& right) {
  return (_internal_copy(right));
}

/**
 *  Close the icmp socket.
 */
void icmp_socket::close() {
  int ret;
  do {
    ret = ::close(_internal_handle);
  } while (ret == -1 && errno == EINTR);
  _internal_handle = -1;
}

/**
 *  Get the address to send data.
 *
 *  @return The address to send data.
 */
unsigned int icmp_socket::get_address() const throw () {
  return (_address);
}

/**
 *  Get the native handle.
 *
 *  @return Native handle.
 */
com::centreon::native_handle icmp_socket::get_native_handle() {
  return (_internal_handle);
}

/**
 *  Get the time to live value of packets.
 *
 *  @return The ttl value.
 */
unsigned char icmp_socket::get_ttl() const {
  unsigned char ttl(0);
  socklen_t size(sizeof(ttl));
  int res(getsockopt(
            _internal_handle,
            SOL_IP,
            IP_TTL,
            &ttl,
            &size));
  if (res)
    throw (basic_error() << "impossible to get ttl:"
           << strerror(errno));
  return (ttl);
}

/**
 *  Read data on the icmp socket.
 *
 *  @param[in] data  Buffer to fill.
 *  @param[in] size  Buffer size.
 *
 *  @return The number of bytes was read on the icmp socket.
 */
unsigned long icmp_socket::read(void* data, unsigned long size) {
  if (!data)
    throw (basic_error() << "read failed on icmp socket: " \
           "invalid parameter (null pointer)");

  sockaddr_in addr;
  unsigned int len_addr(sizeof(sockaddr));
  int ret;
  do {
    ret = recvfrom(
            _internal_handle,
            data,
            size,
            0,
            reinterpret_cast<sockaddr*>(&addr),
            &len_addr);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "read failed on icmp socket: "
           << strerror(errno));
  _address = addr.sin_addr.s_addr;
  ip const* header_ip(reinterpret_cast<ip const*>(data));
  int header_size(header_ip->ip_hl << 2);
  if (ret < header_size + ICMP_MINLEN)
    throw (basic_error() << "read failed on icmp socket: " \
           "invalid packet header size");
  ret -= header_size;
  memmove(
    data,
    static_cast<unsigned char*>(data) + header_size,
    ret);
  return (static_cast<unsigned long>(ret));
}

/*
 *  Set the current address.
 *
 *  @param[in] address  Address to send data.
 */
void icmp_socket::set_address(unsigned int address) throw () {
  _address = address;
}

/**
 *  Set the time to live of packets.
 *
 *  @param[in] ttl  The ttl value.
 */
void icmp_socket::set_ttl(unsigned char ttl) {
  int ret(setsockopt(
            _internal_handle,
            SOL_IP,
            IP_TTL,
            &ttl,
            sizeof(ttl)));
  if (ret)
    throw (basic_error() << "unable to set ttl: "
           << strerror(errno));
}

/**
 *  Write data on the icmp socket.
 *
 *  @param[in] data  Buffer to write on the icmp socket.
 *  @param[in] size  Size of the buffer.
 *
 *  @return The number of bytes written on the icmp socket.
 */
unsigned long icmp_socket::write(void const* data, unsigned long size) {
  if (!data)
    throw (basic_error() << "write failed on icmp socket: " \
           "invalid parameter (null pointer)");

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = _address;

  ssize_t ret;
  do {
    ret = sendto(
            _internal_handle,
            data,
            size,
            0,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(sockaddr));
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "write failed on icmp socket: "
           << strerror(errno));
  return (static_cast<unsigned long>(ret));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
icmp_socket& icmp_socket::_internal_copy(icmp_socket const& right) {
  if (this != &right) {
    _address = right._address;
    _internal_handle = dup(right._internal_handle);
    if (_internal_handle == -1)
      throw (basic_error() << "icmp_socket copy failed: "
             << strerror(errno));
  }
  return (*this);
}
