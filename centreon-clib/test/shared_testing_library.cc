/*
** Copyright 2011-2012 Merethis
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

extern "C" int const export_lib_version = 42;
char const* export_lib_name = "shared_testing_library";

/**
 *  Addition function.
 *
 *  @param[in] i1  first integer.
 *  @param[in] i2  second integer.
 *
 *  @return i1 + i2.
 */
extern "C" int add(int i1, int i2) {
  return (i1 + i2);
}
