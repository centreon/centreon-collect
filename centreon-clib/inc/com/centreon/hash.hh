/*
** Copyright 2012-2013 Merethis
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

#ifndef CC_HASH_HH
#  define CC_HASH_HH

#  if __cplusplus == 201103L
#    define CXX0X_UNORDERED 1
#  elif defined(__clang__)
#    if __has_feature(cxx_variadic_templates)
#      define TR1_UNORDERED 1
#    endif // cxx_variadic_templates.
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    define TR1_UNORDERED 1
#  endif // C++0x, tr1

#  include "com/centreon/namespace.hh"

// Used c++0x implementation.
#  ifdef CXX0X_UNORDERED
#    include <functional>

CC_BEGIN()

template<typename T>
std::size_t hash(T const& data) {
  std::hash<T> h;
  return (h(data));
}

CC_END()

// Used tr1 implementation.
#  elif defined(TR1_UNORDERED)
#    include <tr1/functional>

CC_BEGIN()

template<typename T>
std::size_t hash(T const& data) {
  std::tr1::hash<T> h;
  return (h(data));
}

CC_END()

// Used own implementation.
#  else

CC_BEGIN()

template<typename T>
std::size_t hash(T const& data) {
  std::size_t res(14695981039346656037ULL);
  for (typename T::const_iterator it(data.begin()), end(data.end());
       it != end;
       ++it) {
    res ^= static_cast<std::size_t>(*it);
    res *= static_cast<std::size_t>(1099511628211ULL);
  }
  return (res);
}

template<>
inline std::size_t hash<bool>(bool val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<char>(char val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<int>(int val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<long long>(long long val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<long>(long val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<short>(short val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<unsigned char>(unsigned char val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<unsigned int>(unsigned int val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<unsigned long long>(unsigned long long val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<unsigned long>(unsigned long val) {
  return (static_cast<std::size_t>(val));
}

template<>
inline std::size_t hash<unsigned short>(unsigned short val) {
  return (static_cast<std::size_t>(val));
}

CC_END()

#  endif // C++0X, tr1 or std

CC_BEGIN()

template<typename T, typename U>
std::size_t hash(std::pair<T, U> const& data) {
  std::size_t id(hash(data.first));
  return (hash(data.second) + 0x9e3779b9 + (id << 6) + (id >> 2));
}

template<typename T>
std::size_t hash(T begin, T end) {
  std::size_t res(0);
  while (begin != end) {
    res ^= hash(*begin) + 0x9e3779b9 + (res << 6) + (res >> 2);
    ++begin;
  }
  return (res);
}

template<typename T>
std::size_t hash_combine(std::size_t& seed, T const& data) {
  seed ^= hash(data) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return (seed);
}

template<typename T>
std::size_t hash_combine(std::size_t& seed, T begin, T end) {
  seed ^= hash(begin, end) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return (seed);
}

CC_END()

#endif // !CC_HASH_HH
