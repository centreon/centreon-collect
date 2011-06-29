/*
** Copyright 2011 Merethis
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <QScopedPointer>
#include <QSslSocket>
#include <stdlib.h>
#include "com/centreon/broker/tcp/tls_server.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tcp;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] ts Object to copy.
 */
tls_server::tls_server(tls_server const& ts) : QTcpServer() {
  (void)ts;
  assert(false);
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] ts Object to copy.
 *
 *  @return This object.
 */
tls_server& tls_server::operator=(tls_server const& ts) {
  (void)ts;
  assert(false);
  abort();
  return (*this);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] private_key Private key used for encryption.
 *  @param[in] public_cert Public certificate used for encryption.
 *  @param[in] ca_cert     Trusted CA's certificate used to authenticate
 *                         incoming clients.
 */
tls_server::tls_server(QString const& private_key,
                       QString const& public_cert,
                       QString const& ca_cert)
  : _ca(ca_cert),
    _private(private_key),
    _public(public_cert) {}

/**
 *  Destructor.
 */
tls_server::~tls_server() {}

/**
 *  Accept incoming connection.
 *
 *  @param[in] socketDescriptor Native socket descriptor of incoming
 *                              client.
 */
void tls_server::incomingConnection(int socketDescriptor) {
  // Create socket object.
  QScopedPointer<QSslSocket> ssl_socket(new QSslSocket);
  ssl_socket->setSocketDescriptor(socketDescriptor);

  // Use only TLS protocol.
  ssl_socket->setProtocol(QSsl::TlsV1);

  // Set self certificates.
  if (!_private.isEmpty() && !_public.isEmpty()) {
    ssl_socket->setLocalCertificate(_public);
    ssl_socket->setPrivateKey(_private);
  }

  // Set CA certificate.
  if (!_ca.isEmpty()) {
    ssl_socket->addCaCertificates(_ca);
    ssl_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    ssl_socket->setPeerVerifyDepth(0);
  }
  else
    ssl_socket->setPeerVerifyMode(QSslSocket::VerifyNone);

  // Launch handshake.
  ssl_socket->startServerEncryption();

  // XXX : blocking
  // Wait for handshake to be performed.
  if (ssl_socket->waitForEncrypted(-1)) {
#if QT_VERSION < 0x040700
    std::auto_ptr<QTcpSocket> ptr(ssl_socket.take());
    _pending.enqueue(ptr);
#else
    addPendingConnection(ssl_socket.take());
#endif /* QT_VERSION < 4.7 */
    emit newConnection();
  }

  return ;
}

#if QT_VERSION < 0x040700
/**
 *  Check if the server has pending connections.
 *
 *  @return true if some connections are pending.
 */
bool tls_server::hasPendingConnections() const {
  return (!_pending.isEmpty());
}

/**
 *  Get the next pending connection.
 *
 *  @return Next pending connection.
 */
QTcpSocket* tls_server::nextPendingConnection() {
  QScopedPointer<QTcpSocket> sock;
  if (!_pending.isEmpty()) {
    sock.reset(_pending.head().release());
    _pending.dequeue();
  }
  return (sock.take());
}
#endif /* QT_VERSION < 4.7 */
