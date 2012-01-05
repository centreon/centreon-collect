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

#ifndef CCC_ICMP_NAMESPACE_HH
#  define CCC_ICMP_NAMESPACE_HH

#  ifndef CCC_ICMP_BEGIN
#    define CCC_ICMP_BEGIN() namespace       com {        \
                               namespace     centreon {   \
                                 namespace   connector {  \
                                   namespace icmp {
#  endif // !CCC_ICMP_BEGIN

#  ifndef CCC_ICMP_END
#    define CCC_ICMP_END() } } } }
#  endif // !CCC_ICMP_END

#endif // !CCC_ICMP_NAMESPACE_HH
