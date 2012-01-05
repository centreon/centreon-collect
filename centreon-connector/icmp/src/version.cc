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

#include "com/centreon/connector/icmp/version.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Get version major.
 *
 *  @return Centreon Connector ICMP version major.
 */
unsigned int version::get_major() throw () {
  return (major);
}

/**
 *  Get version minor.
 *
 *  @return Centreon Connector ICMP version minor.
 */
unsigned int version::get_minor() throw () {
  return (minor);
}

/**
 *  Get version patch.
 *
 *  @return Centreon Connector ICMP version patch.
 */
unsigned int version::get_patch() throw () {
  return (patch);
}

/**
 *  Get version string.
 *
 *  @return Centreon Connector ICMP version as string.
 */
char const* version::get_string() throw () {
  return (CENTREON_CONNECTOR_ICMP_VERSION_STRING);
}

/**
 *  Get version engine major.
 *
 *  @return Centreon Engine minimum version major.
 */
unsigned int version::get_engine_major() throw () {
  return (engine_major);
}

/**
 *  Get version engine minor.
 *
 *  @return Centreon Engine minimum version minor.
 */
unsigned int version::get_engine_minor() throw () {
  return (engine_minor);
}
