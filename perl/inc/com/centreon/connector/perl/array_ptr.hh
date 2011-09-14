/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_PERL_ARRAY_PTR_HH_
# define CCC_PERL_ARRAY_PTR_HH_

# include <stddef.h>
# include "com/centreon/connector/perl/namespace.hh"

CCC_PERL_BEGIN()

/**
 *  @class array_ptr array_ptr.hh
 *  @brief Similar to auto_ptr for arrays.
 *
 *  Provide similar feature as auto_ptr but for array pointers.
 */
template     <typename T>
class        array_ptr {
 private:
  T*         _ptr;

 public:
  /**
   *  Constructor.
   *
   *  @param[in] t Array pointer.
   */
             array_ptr(T* t) : _ptr(t) {}

  /**
   *  Copy constructor.
   *
   *  @param[in] ap Object to copy.
   */
             array_ptr(array_ptr& ap) : _ptr(ap._ptr) {
    ap._ptr = NULL;
  }

  /**
   *  Destructor.
   */
             ~array_ptr() {
    if (_ptr)
      delete [] _ptr;
  }

  /**
   *  Assignment operator.
   *
   *  @param[in] ap Object to copy.
   *
   *  @return This object.
   */
  array_ptr& operator=(array_ptr& ap) {
    if (&ap != this) {
      _ptr = ap._ptr;
      ap._ptr = NULL;
    }
    return (*this);
  }

  /**
   *  Dereferencing pointer.
   *
   *  @return Dereferenced pointer.
   */
  T&         operator*() {
    return (*_ptr);
  }

  /**
   *  Array access operator.
   *
   *  @param[in] idx Index in array.
   *
   *  @return Element at position idx.
   */
  T&         operator[](unsigned int idx) {
    return (_ptr[idx]);
  }

  /**
   *  Get the pointer associated with this object.
   *
   *  @return Pointer associated with this object.
   */
  T*         get() const {
    return (_ptr);
  }

  /**
   *  Release the associated pointer and release it.
   *
   *  @return Pointer associated with this object.
   */
  T*         release() {
    T* tmp(_ptr);
    _ptr = NULL;
    return (tmp);
  }
};

CCC_PERL_END()

#endif /* !CCC_PERL_ARRAY_PTR_HH_ */
