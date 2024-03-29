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

#ifndef CCE_DELETER_LISTMEMBER_HH
#define CCE_DELETER_LISTMEMBER_HH

#include <cstddef>

namespace com::centreon::engine {

namespace deleter {
template <typename T>
void listmember(T*& ptr, void (*release)(void*)) throw() {
  while (ptr) {
    T* next(ptr->next);
    release(ptr);
    ptr = next;
  }
  ptr = NULL;
}
}  // namespace deleter

}

#endif  // !CCE_DELETER_LISTMEMBER_HH
