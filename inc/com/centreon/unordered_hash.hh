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

#ifndef CC_UNORDERED_HASH_HH
#  define CC_UNORDERED_HASH_HH

#  if defined(__GXX_EXPERIMENTAL_CXX0X__)
#    include <unordered_map>
#    include <unordered_set>
#    define umap std::unordered_map
#    define umultimap std::unordered_multimap
#    define uset std::unordered_set
#    define umultiset std::unordered_multiset
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    include <tr1/unordered_map>
#    include <tr1/unordered_set>
#    define umap std::tr1::unordered_map
#    define umultimap std::tr1::unordered_multimap
#    define uset std::tr1::unordered_set
#    define umultiset std::tr1::unordered_multiset
#  else
#    include <map>
#    include <set>
#    define umap std::map
#    define umultimap std::multimap
#    define uset std::set
#    define umultiset std::multiset
#  endif // CPP0X, GNUC4

#endif // !CC_UNORDERED_HASH_HH
