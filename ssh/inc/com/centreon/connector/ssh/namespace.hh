/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCS_NAMESPACE_HH
#  define CCCS_NAMESPACE_HH

#  ifdef CCCS_BEGIN
#    undef CCCS_BEGIN
#  endif // CCCS_BEGIN
#  define CCCS_BEGIN() namespace       com { \
                         namespace     centreon { \
                           namespace   connector { \
                             namespace ssh {

#  ifdef CCCS_END
#    undef CCCS_END
#  endif // CCCS_END
#  define CCCS_END() } } } }

#endif // !CCCS_NAMESPACE_HH
