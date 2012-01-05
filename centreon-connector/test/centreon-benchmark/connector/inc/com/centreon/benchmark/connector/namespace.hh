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

#ifndef CCB_CONNECTOR_NAMESPACE_HH
#  define CCB_CONNECTOR_NAMESPACE_HH

#  ifndef CCB_CONNECTOR_BEGIN
#    define CCB_CONNECTOR_BEGIN() namespace       com {        \
                                    namespace     centreon {   \
                                      namespace   benchmark {  \
                                        namespace connector {
#  endif // !CCB_CONNECTOR_BEGIN

#  ifndef CCB_CONNECTOR_END
#    define CCB_CONNECTOR_END() } } } }
#  endif // !CCB_CONNECTOR_END

#endif // !CCB_CONNECTOR_NAMESPACE_HH
