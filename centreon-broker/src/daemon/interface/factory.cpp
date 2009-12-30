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

#include <memory>
#include "configuration/interface.h"
#include "db/mysql/connection.h"
#include "interface/db/destination.h"
#include "interface/factory.h"
#include "interface/ndo/destination.h"
#include "interface/ndo/source.h"
#include "interface/xml/destination.h"
#include "io/net/ipv4.h"
#include "io/net/ipv6.h"
#include "io/net/unix.h"
#include "io/split.h"

using namespace Interface;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Factory default constructor.
 */
Factory::Factory() {}

/**
 *  Factory copy constructor.
 *
 *  \param[in] factory Unused.
 */
Factory::Factory(const Factory& factory)
{
  (void)factory;
}

/**
 *  Factory destructor.
 */
Factory::~Factory() {}

/**
 *  Assignment operator overload.
 *
 *  \param[in] factory Unused.
 *
 *  \return *this
 */
Factory& Factory::operator=(const Factory& factory)
{
  (void)factory;
  return (*this);
}

/**
 *  Build an IPv4 connector according to the configuration provided.
 *
 *  \param[in] i Configuration of the IPv4 connector.
 *
 *  \return A connected IPv4 connector.
 */
IO::Net::IPv4Connector* Factory::IPv4Connector(
  const Configuration::Interface& i)
{
  std::auto_ptr<IO::Net::IPv4Connector> ipv4c(new IO::Net::IPv4Connector);

  ipv4c->Connect(i.host.c_str(), i.port);
  return (ipv4c.release());
}

/**
 *  Build an IPv6 connector according to the configuration provided.
 *
 *  \param[in] i Configuration of the IPv6 connector.
 *
 *  \return A connected IPv6 connector.
 */
IO::Net::IPv6Connector* Factory::IPv6Connector(
  const Configuration::Interface& i)
{
  std::auto_ptr<IO::Net::IPv6Connector> ipv6c(new IO::Net::IPv6Connector);

  ipv6c->Connect(i.host.c_str(), i.port);
  return (ipv6c.release());
}

/**
 *  Build a Unix connector according to the configuration provided.
 *
 *  \param[in] i Configuration of the Unix connector.
 *
 *  \return A connected Unix connector.
 */
IO::Net::UnixConnector* Factory::UnixConnector(
  const Configuration::Interface& i)
{
  std::auto_ptr<IO::Net::UnixConnector> uc(new IO::Net::UnixConnector);

  uc->Connect(i.socket.c_str());
  return (uc.release());
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Build an acceptor from its configuration.
 *
 *  \param[in] i Configuration of the new acceptor.
 *
 *  \return An acceptor matching the configuration.
 */
IO::Acceptor* Factory::Acceptor(const Configuration::Interface& i)
{
  IO::Acceptor* acceptor;

  switch (i.type)
    {
     case Configuration::Interface::IPV4_SERVER:
      {
        std::auto_ptr<IO::Net::IPv4Acceptor> ipv4a(new IO::Net::IPv4Acceptor);

        if (i.interface.empty())
          ipv4a->Listen(i.port);
        else
          ipv4a->Listen(i.port, i.interface.c_str());
        acceptor = ipv4a.get();
        ipv4a.release();
      }
      break ;
     case Configuration::Interface::IPV6_SERVER:
      {
        std::auto_ptr<IO::Net::IPv6Acceptor> ipv6a(new IO::Net::IPv6Acceptor);

        if (i.interface.empty())
          ipv6a->Listen(i.port);
        else
          ipv6a->Listen(i.port, i.interface.c_str());
        acceptor = ipv6a.get();
        ipv6a.release();
      }
      break ;
     case Configuration::Interface::UNIX_SERVER:
      {
        std::auto_ptr<IO::Net::UnixAcceptor> ua(new IO::Net::UnixAcceptor);

        ua->Listen(i.socket.c_str());
        acceptor = ua.get();
        ua.release();
      }
      break ;
     default:
      acceptor = NULL;
    }
  return (acceptor);
}

/**
 *  Build a destination from its configuration.
 *  \par Safety Strong exception safety.
 *
 *  \param[in] i Configuration of the new destination.
 *
 *  \return A destination matching the configuration.
 *
 *  \throw Exception Destination creation failed.
 */
Destination* Factory::Destination(const Configuration::Interface& i)
{
  Interface::Destination* dest;

  switch (i.type)
    {
     case Configuration::Interface::FILE:
      {
        std::auto_ptr<IO::Split> split(new IO::Split);

        // XXX : set file num + file size
        split->BaseFile(i.filename);
        dest = new Interface::XML::Destination(split.get());
        split.release();
      }
     case Configuration::Interface::IPV4_CLIENT:
      {
        std::auto_ptr<IO::Net::IPv4Connector> ipv4c(this->IPv4Connector(i));

        if (Configuration::Interface::XML == i.protocol)
          dest = new Interface::XML::Destination(ipv4c.get());
        else
          dest = new Interface::NDO::Destination(ipv4c.get());
        ipv4c.release();
      }
      break ;
     case Configuration::Interface::IPV6_CLIENT:
      {
        std::auto_ptr<IO::Net::IPv6Connector> ipv6c(this->IPv6Connector(i));

        if (Configuration::Interface::XML == i.protocol)
          dest = new Interface::XML::Destination(ipv6c.get());
        else
          dest = new Interface::NDO::Destination(ipv6c.get());
        ipv6c.release();
      }
      break ;
     case Configuration::Interface::MYSQL:
      {
        std::auto_ptr<CentreonBroker::DB::MySQLConnection> myconn(
          new CentreonBroker::DB::MySQLConnection);
        std::auto_ptr<Interface::DB::Destination> mydest;

        myconn->Connect(i.host, i.user, i.password, i.db);
        mydest.reset(new Interface::DB::Destination);
        mydest->Init(myconn.get());
        myconn.release();
        dest = mydest.release();
      }
      break ;
     case Configuration::Interface::UNIX_CLIENT:
      {
        std::auto_ptr<IO::Net::UnixConnector> uc(this->UnixConnector(i));

        if (Configuration::Interface::XML == i.protocol)
          dest = new Interface::XML::Destination(uc.get());
        else
          dest = new Interface::NDO::Destination(uc.get());
        uc.release();
      }
      break ;
     default:
      dest = NULL;
    }

  return (dest);
}

/**
 *  \brief Get the Interface instance.
 *
 *  Interface is a singleton. This method returns the only available static
 *  instance of this class.
 *  \par Safety No throw guarantee.
 *
 *  \return The Interface instance.
 */
Factory& Factory::Instance()
{
  static Factory factory;

  return (factory);
}

/**
 *  Build a source from its configuration.
 *  \par Safety Strong exception safety.
 *
 *  \param[in] i Configuration of the new destination.
 *
 *  \return A source matching the configuration.
 *
 *  \throw Exception Source creation failed.
 */
Source* Factory::Source(const Configuration::Interface& i)
{
  Interface::Source* source;

  switch (i.type)
    {
     case Configuration::Interface::FILE:
      {
        std::auto_ptr<IO::Split> split(new IO::Split);

        // XXX : set file num + file size
        source = new Interface::NDO::Source(split.get());
        split.release();
      }
      break ;
     case Configuration::Interface::IPV4_CLIENT:
      {
        std::auto_ptr<IO::Net::IPv4Connector> ipv4c(this->IPv4Connector(i));

        source = new Interface::NDO::Source(ipv4c.get());
        ipv4c.release();
      }
      break ;
     case Configuration::Interface::IPV6_CLIENT:
      {
        std::auto_ptr<IO::Net::IPv6Connector> ipv6c(this->IPv6Connector(i));

        source = new Interface::NDO::Source(ipv6c.get());
        ipv6c.release();
      }
      break ;
     case Configuration::Interface::UNIX_CLIENT:
      {
        std::auto_ptr<IO::Net::UnixConnector> uc(this->UnixConnector(i));

        source = new Interface::NDO::Source(uc.get());
        uc.release();
      }
      break ;
     default:
      source = NULL;
    }

  return (source);
}

/**
 *  Build a source-destination from its configuration.
 *  \par Safety Strong exception safety.
 *
 *  \param[in] i Configuration of the new destination.
 *
 *  \return A source-destination matching the configuration.
 *
 *  \throw Exception Source-destination creation failed.
 */
SourceDestination* Factory::SourceDestination(
                     const Configuration::Interface& i)
{
}
