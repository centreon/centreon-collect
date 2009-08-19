/*
**  Copyright 2009 MERETHIS
**  This file is part of CentreonBroker.
**
**  CentreonBroker is free software: you can redistribute it and/or modify it
**  under the terms of the GNU General Public License as published by the Free
**  Software Foundation, either version 2 of the License, or (at your option)
**  any later version.
**
**  CentreonBroker is distributed in the hope that it will be useful, but
**  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
**  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
**  for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with CentreonBroker.  If not, see <http://www.gnu.org/licenses/>.
**
**  For more information : contact@centreon.com
*/

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "exception.h"
#include "io/net4.h"

using namespace CentreonBroker::IO;

/******************************************************************************
*                                                                             *
*                                                                             *
*                                 Net4Stream                                  *
*                                                                             *
*                                                                             *
******************************************************************************/

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  \brief Duplicate the given object's socket.
 *
 *  This function is used by the copy constructor and operator= to duplicate
 *  the file descriptor of the object given as a parameter. Namely the file
 *  descriptor will be dup()ed and the resulting new file descriptor will be
 *  stored with the current instance. If dup() failed, the method will throw a
 *  CentreonBroker::Exception.
 *
 *  \param[in] n4s Object which holds the original socket file descriptor.
 *
 *  \see Net4Stream
 *  \see operator=
 */
void Net4Stream::InternalCopy(const Net4Stream& n4s)
  throw (CentreonBroker::Exception)
{
  this->sockfd_ = dup(n4s.sockfd_);
  if (this->sockfd_ < 0)
    throw (CentreonBroker::Exception(errno, strerror(errno)));
  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  \brief Net4Stream constructor.
 *
 *  Build a Net4Stream by providing an already opened Berkeley style socket
 *  file descriptor. Once the constructor has been successfully executed, the
 *  Net4Stream object is responsible of the file descriptor (ie. it will handle
 *  all I/O operations as well as closing).
 *
 *  \param[in] sockfd Berkeley style socket file descriptor.
 */
Net4Stream::Net4Stream(int sockfd) throw () : sockfd_(sockfd) {}

/**
 *  \brief Net4Stream copy constructor.
 *
 *  Duplicate the Net4Stream object given as a parameter. The internal socket
 *  file descriptor will be dup()ed. A CentreonBroker::Exception will be thrown
 *  in case of error.
 *
 *  \param[in] n4s Net4Stream to duplicate.
 */
Net4Stream::Net4Stream(const Net4Stream& n4s) throw (CentreonBroker::Exception)
  : Stream(n4s)
{
  this->InternalCopy(n4s);
}

/**
 *  \brief Net4Stream destructor.
 *
 *  The destructor will call Close() if the call has not already been made.
 */
Net4Stream::~Net4Stream() throw ()
{
  this->Close();
}

/**
 *  \brief Overload of the assignement operator.
 *
 *  Close the current socket and duplicate the Net4Stream object given as a
 *  parameter. The internal socket file descriptor will be dup()ed. In case of
 *  error, the current object will be in a closed state and a
 *  CentreonBroker::Exception will be thrown.
 *
 *  \param[in] n4s Net4Stream to duplicate.
 *
 *  \return *this
 */
Net4Stream& Net4Stream::operator=(const Net4Stream& n4s)
  throw (CentreonBroker::Exception)
{
  this->Close();
  this->Stream::operator=(n4s);
  this->InternalCopy(n4s);
  return (*this);
}

/**
 *  \brief Close the Net4Stream socket.
 *
 *  Close the current socket. If called directly, it won't be possible to use
 *  the object without error anymore.
 */
void Net4Stream::Close() throw ()
{
  if (this->sockfd_ >= 0)
    {
      shutdown(this->sockfd_, SHUT_RDWR);
      close(this->sockfd_);
      this->sockfd_ = -1;
    }
  return ;
}

/**
 *  \brief Receive data from the network stream.
 *
 *  Receive at most size bytes from the network stream and store them in
 *  buffer. The number of bytes read is then returned. This number can be less
 *  than size. In case of error, a CentreonBroker::Exception is thrown.
 *
 *  \param[out] buffer Buffer on which to store received data.
 *  \param[in]  size   Maximum number of bytes to read.
 *
 *  \return Number of bytes read from the network stream. 0 if the connection
 *          has been shut down.
 */
int Net4Stream::Receive(char* buffer, int size)
  throw (CentreonBroker::Exception)
{
  int ret;

  ret = recv(this->sockfd_, buffer, size, 0);
  if (ret < 0)
    throw (CentreonBroker::Exception(errno, strerror(errno)));
  return (ret);
}

/**
 *  \brief Send data across the network stream.
 *
 *  Send at most size bytes from the buffer. The number of bytes actually sent
 *  is returned. This number can be less than size. In case of error, a
 *  CentreonBroker::Exception is thrown.
 *
 *  \param[in] buffer Data to send.
 *  \param[in] size   Maximum number of bytes to send.
 *
 *  \return Number of bytes actually sent to the network stream. 0 if the
 *          connection has been shut down.
 */
int Net4Stream::Send(const char* buffer, int size)
  throw (CentreonBroker::Exception)
{
  int ret;

  ret = send(this->sockfd_, buffer, size, 0);
  if (ret < 0)
    throw (CentreonBroker::Exception(errno, strerror(errno)));
  return (ret);
}


/******************************************************************************
*                                                                             *
*                                                                             *
*                               Net4Acceptor                                  *
*                                                                             *
*                                                                             *
******************************************************************************/

/**************************************
*                                     *
*          Private Methods            *
*                                     *
**************************************/

/**
 *  \brief Duplicate the internal file descriptor of the given object to the
 *         current instance.
 *
 *  This method is used by the copy constructor and operator= to duplicate the
 *  file descriptor of the given object. This will result in the current
 *  instance listening on the same port and with the same parameters as the
 *  given object. In case of error, a CentreonBroker::Exception is thrown.
 *
 *  \param[in] n4a Net4Acceptor to duplicate.
 *
 *  \see Net4Acceptor
 *  \see operator=
 */
void Net4Acceptor::InternalCopy(const Net4Acceptor& n4a)
  throw (CentreonBroker::Exception)
{
  this->sockfd_ = dup(n4a.sockfd_);
  if (this->sockfd_ < 0)
    throw (CentreonBroker::Exception(errno, strerror(errno)));
  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  \brief Net4Acceptor default constructor.
 *
 *  Build a Net4Acceptor.
 */
Net4Acceptor::Net4Acceptor() throw () : sockfd_(-1) {}

/**
 *  \brief Net4Acceptor copy constructor.
 *
 *  Duplicate the already listening acceptor. The resulting acceptor will have
 *  the exact same parameters as the provided acceptor already have. In case of
 *  error, a CentreonBroker::Exception is thrown.
 *
 *  \param[in] n4a Net4Acceptor to duplicate.
 */
Net4Acceptor::Net4Acceptor(const Net4Acceptor& n4a)
  throw (CentreonBroker::Exception) : Acceptor(n4a), sockfd_(-1)
{
  this->InternalCopy(n4a);
}

/**
 *  \brief Net4Acceptor destructor.
 *
 *  Will close the socket if it has not already been done.
 */
Net4Acceptor::~Net4Acceptor() throw ()
{
  this->Close();
}

/**
 *  \brief Overload of the assignement operator.
 *
 *  Duplicate the already listening acceptor. The resulting acceptor will have
 *  the exact same parameters as the provided acceptor already have. In case of
 *  error, a CentreonBroker::Exception is thrown.
 *
 *  \param[in] n4a Net4Acceptor to duplicate.
 *
 *  \return *this
 */
Net4Acceptor& Net4Acceptor::operator=(const Net4Acceptor& n4a)
  throw (CentreonBroker::Exception)
{
  this->Close();
  this->Acceptor::operator=(n4a);
  this->InternalCopy(n4a);
  return (*this);
}

/**
 *  \brief Wait for a new incoming client.
 *
 *  Once the acceptor is in a listening state, one can wait for incoming
 *  client by using this method. Once a client is properly connected, this
 *  method will return a Stream object (in fact a Net4Stream object). In case
 *  of error, a CentreonBroker::Exception is thrown.
 *
 *  \return A stream object representing the new client connection.
 */
Stream* Net4Acceptor::Accept()
{
  int fd;

  fd = accept(this->sockfd_, NULL, NULL);
  if (fd < 0)
    throw (CentreonBroker::Exception(errno, strerror(errno)));
  return (new Net4Stream(fd));
}

/**
 *  \brief Close the acceptor.
 *
 *  Shutdown the socket properly. No more client connection can be made through
 *  this acceptor.
 */
void Net4Acceptor::Close() throw ()
{
  if (this->sockfd_ >= 0)
    {
      shutdown(this->sockfd_, SHUT_RDWR);
      fsync(this->sockfd_);
      close(this->sockfd_);
      this->sockfd_ = -1;
    }
  return ;
}

/**
 *  \brief Put the acceptor in listen mode.
 *
 *  Before being able to Accept() new clients, the Net4Acceptor has to be put
 *  in the listen mode. Call this method with the desired port to do so. In
 *  case of error, a CentreonBroker::Exception is thrown.
 *
 *  \param[in] port  Port on which the acceptor should listen on.
 *  \param[in] iface IP address of the interface the acceptor should be to.
 *                   If NULL (default), bind to all available interfaces.
 */
void Net4Acceptor::Listen(unsigned short port, const char* iface)
  throw (CentreonBroker::Exception)
{
  struct sockaddr_in sin;

  // Close the socket if it was previously open
  this->Close();

  // Create the new socket
  this->sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (this->sockfd_ < 0)
    throw (CentreonBroker::Exception(errno, strerror(errno)));

  // Set the binding structure
  memset(&sin, 0, sizeof(sin));
  if (iface)
    {
      sin.sin_addr.s_addr = inet_addr(iface);
      if ((in_addr_t)-1 == sin.sin_addr.s_addr)
	throw (CentreonBroker::Exception(errno, strerror(errno)));
    }
  else
    sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);

  // Bind
  if (bind(this->sockfd_, (struct sockaddr*)&sin, sizeof(sin))
      || listen(this->sockfd_, 0))
    throw (CentreonBroker::Exception(errno, strerror(errno)));
  return ;
}
