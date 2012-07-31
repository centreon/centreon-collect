/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CC_HTABLE_HH
#  define CC_HTABLE_HH

#  if defined(__GXX_EXPERIMENTAL_CXX0X__)
#    include <unordered_map>
#    define htable std::unordered_map
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    include <tr1/unordered_map>
#    define htable std::tr1::unordered_map
#  else
#    include <map>
#    define htable std::map
#  endif // CPP0X, GNUC4

#endif // !CC_HTABLE_HH
