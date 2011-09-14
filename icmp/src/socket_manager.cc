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

#include <QMutexLocker>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "com/centreon/connector/icmp/socket_manager.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Get instance of socket manager.
 *
 *  @return This singleton.
 */
socket_manager& socket_manager::instance() {
  static socket_manager instance;
  return (instance);
}

/**
 *  Initalize raw socket.
 *
 *  @param[in] nbr_socket The number of raw socket to create.
 */
void socket_manager::initialize(unsigned int nbr_socket) {
  QMutexLocker lock(&_mutex);
  for (unsigned int i = 0; i < nbr_socket; ++i) {
    int sock = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1)
      throw ("XXX: ");
    _free.push_back(sock);
  }
}

/**
 *  Take a raw socket.
 *
 *  @return The free socket.
 */
int socket_manager::take() {
  QMutexLocker lock(&_mutex);
  if (_free.empty())
    _wait_socket.wait(&_mutex);
  int sock = _free.front();
  _free.pop_front();
  _busy.insert(sock);
  return (sock);
}

/**
 *  Release a raw socket.
 *
 *  @param[in] sock The sock to release.
 */
void socket_manager::release(int sock) {
  QMutexLocker lock(&_mutex);
  QSet<int>::iterator it = _busy.find(sock);
  if (it != _busy.end()) {
    _busy.erase(it);
    _free.push_back(sock);
    _wait_socket.wakeOne();
  }
}

/**
 *  Default constructor.
 */
socket_manager::socket_manager() {

}

/**
 *  Default destructor.
 */
socket_manager::~socket_manager() throw() {
  QMutexLocker lock(&_mutex);
  for (QSet<int>::const_iterator it = _busy.begin(), end = _busy.end(); it != end; ++it)
    close(*it);
  for (QList<int>::const_iterator it = _free.begin(), end = _free.end(); it != end; ++it)
    close(*it);
}
