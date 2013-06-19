/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_OBJECTS_CUSTOMVARIABLESMEMBER_HH
#  define CCE_OBJECTS_CUSTOMVARIABLESMEMBER_HH

typedef struct                         customvariablesmember_struct {
  char*                                variable_name;
  char*                                variable_value;
  int                                  has_been_modified;
  struct customvariablesmember_struct* next;
}                                      customvariablesmember;

#  ifdef __cplusplus
#    include <ostream>

bool          operator==(
                customvariablesmember const& obj1,
                customvariablesmember const& obj2) throw ();
bool          operator!=(
                customvariablesmember const& obj1,
                customvariablesmember const& obj2) throw ();
std::ostream& operator<<(
                std::ostream& os,
                customvariablesmember const& obj);

#  endif // C++

#endif // !CCE_OBJECTS_CUSTOMVARIABLESMEMBER_HH


