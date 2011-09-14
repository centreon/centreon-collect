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

#ifndef CCC_ICMP_NAMESPACE_HH_
# define CCC_ICMP_NAMESPACE_HH_

# ifdef NAMESPACE_BEGIN
#  undef NAMESPACE_BEGIN
# endif /* NAMESPACE_BEGIN */
# define NAMESPACE_BEGIN() namespace       com { \
                             namespace     centreon { \
                               namespace   connector { \
                                 namespace icmp {

# ifdef NAMESPACE_END
#  undef NAMESPACE_END
# endif /* NAMESPACE_END */
# define NAMESPACE_END() } } } }

#endif /* !CCC_ICMP_NAMESPACE_HH_ */
