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

#  if __cplusplus == 201103L
#    define CXX0X_UNORDERED 1
#  elif defined(__clang__)
#    if __has_feature(cxx_variadic_templates)
#      define TR1_UNORDERED 1
#    endif // cxx_variadic_templates.
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    define TR1_UNORDERED 1
#  endif // C++0x, tr1

// Used c++0x implementation.
#  ifdef CXX0X_UNORDERED
#    include <unordered_map>
#    include <unordered_set>
#    define umap std::unordered_map
#    define umultimap std::unordered_multimap
#    define uset std::unordered_set
#    define umultiset std::unordered_multiset
// Used tr1 implementation.
#  elif defined(TR1_UNORDERED)
#    include <tr1/unordered_map>
#    include <tr1/unordered_set>
#    define umap std::tr1::unordered_map
#    define umultimap std::tr1::unordered_multimap
#    define uset std::tr1::unordered_set
#    define umultiset std::tr1::unordered_multiset
// Used std implementation.
#  else
#    include <map>
#    include <set>
#    define umap std::map
#    define umultimap std::multimap
#    define uset std::set
#    define umultiset std::multiset
#  endif // C++0X, tr1 or std

#endif // !CC_UNORDERED_HASH_HH
